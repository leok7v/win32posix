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
#pragma once
#ifndef __THREAD_H__
#define __THREAD_H__
#ifdef WIN32
#pragma warning(disable: 4820 4514)
#include <vadefs.h>
#else
#include <pthread.h>
#endif

class Thread {
public:
    Thread(void* (*f)(void*), void* arg = 0);
    virtual ~Thread();
    void* join(); /* ok to call after try_join for result */
    bool try_join();
private:
#ifdef WIN32
    uintptr_t thread;
    static unsigned int __stdcall winThreadProc(void* p);
#else
    static void* posixThreadProc(void* p);
    pthread_t thread; // do not use pthread_exit if you want to keep try_join() working...
#endif
    void* (*f)(void*);
    void* a;
    void* result;
    volatile bool  done;
    volatile bool  exiting;
};

/* IMPORTANT: try_join() only works if:
   TerminateThread/pthread_exit/pthread_cancel has not be called on thread handle
   and thread function returns natural way via return statement.
*/

#endif /* __THREAD_H__ */
