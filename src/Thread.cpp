/*  Copyright (c) 2013, Leo Kuznetsov
 All rights reserved.

 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions are met:

 * Redistributions of source code must retain the above copyright notice, this
 list of conditions and the following disclaimer.

 * Redistributions in binary form must reproduce the above copyright notice,
 this list of conditions and the following disclaimer in the documentation
 and/or other materials provided with the distribution.

 * Neither the name of the {organization} nor the names of its
 contributors may be used to endorse or promote products derived from
 this software without specific prior written permission.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include "Thread.h"
#include "assert.h"

#ifdef WIN32
#pragma warning(disable: 4820 4514 4668 4189 4711)
#include <Windows.h>
#include <process.h>

unsigned int __stdcall Thread::winThreadProc(void* p) {
    Thread & t = *(Thread*)p;
    t.result = t.f(t.a);
    _endthreadex(0);
    return 0;
}

Thread::Thread(void* (*func)(void*), void* arg) : f(func), a(arg), done(false), exiting(false) {
    thread = _beginthreadex(0, 0, Thread::winThreadProc, this, 0, 0);
}

Thread::~Thread() {
    #pragma warning(suppress: 4365)
    assert(thread == (uintptr_t)0); // join() must be call before destructor
    thread = 0;
}

void* Thread::join() {
    if (!done) {
        #pragma warning(suppress: 4365)
        assert(thread != (uintptr_t)0); // do not join twice
        int r = (int)::WaitForSingleObject((HANDLE)thread, INFINITE);
        ::CloseHandle((HANDLE)thread);
        thread = 0;
        done = true;
        #pragma warning(suppress: 4365)
        assert(r == WAIT_OBJECT_0);
    }
    return result;
}

bool Thread::try_join() {
    #pragma warning(suppress: 4365)
    assert(thread != 0); // do not join twice
    int r = (int)::WaitForSingleObject((HANDLE)thread, 0);
    done = r == WAIT_OBJECT_0;
    if (done) {
        ::CloseHandle((HANDLE)thread);
        thread = 0;
    }
    return done;
}

#else

void* Thread::posixThreadProc(void* p) {
    Thread & t = *(Thread*)p;
    t.result = t.f(t.a);
    t.exiting = true;
    return t.result;
}

Thread::Thread(void* (*func)(void*), void* arg) : f(func), a(arg), done(false), exiting(false) {
    pthread_create(&thread, 0, Thread::posixThreadProc, this);
}

Thread::~Thread() {
    assert(thread == 0); // join() must be call before destructor
    thread = 0;
}

void* Thread::join() {
    if (!done) {
        assert(thread != 0); // do not join twice
        pthread_join(thread, &result);
        thread = 0;
    }
    return result;
}

bool Thread::try_join() {
    assert(thread != 0); // do not join twice
    // pthread_try_join is not supported yet
    int r = exiting ? 0 : 1;
    if (r == 0 && !done) {
        join();
    }
    done = r == 0;
    if (done) {
        thread = 0;
    }
    return done;
}

#endif