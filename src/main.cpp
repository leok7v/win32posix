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
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "SystemTime.h"
#include "Event.h"
#include "Thread.h"

#define null NULL
#define countof(a) (sizeof(a) / sizeof((a)[0]))

#ifdef WIN32
#pragma warning(disable: 4514 4820 4365 4189 4711)
#endif

struct Test {
    Event* e;
    Event* s;
    long long timeout;
    bool all;
};

static void* test1_wait(void* p) {
    int r = ((Test*)p)->e->wait();
    assert(r == 0);
    return (void*)"test1_wait";
}

static void* test1_wait_with_timeout(void* p) {
    int r = ((Test*)p)->e->wait(((Test*)p)->timeout);
    assert(r == 0);
    return (void*)"test1_wait_with_timeout";
}

static void* test1_wait_timeouted(void* p) {
    int r = ((Test*)p)->e->wait(((Test*)p)->timeout);
    assert(r == EVENT_WAIT_TIMEOUT);
    return (void*)"test1_wait_with_timeout";
}

static void test1() {
    void* (*f[2])(void*) = {test1_wait, test1_wait_with_timeout};
    Test test = {0};
    test.timeout = NANOSECONDS_IN_SECOND / 4;
    for (int i = 0; i < countof(f); i++) {
        {   // auto event / initial state unsignaled
            Event e(false);
            test.e = &e;
            Thread t(f[i], &test);
            SystemTime::sleep(NANOSECONDS_IN_SECOND / 32);
            e.set(); // assume the thread already riched the waiting state in quarter of a second
            t.join();
        }
        {   // auto event / initial state unsignaled
            Event e(false);
            test.e = &e;
            e.set(); // signal before strarting thread
            Thread t(f[i], &test);
            t.join();
        }
        {   // auto event / initial state signaled
            Event e(false, true);
            test.e = &e;
            Thread t(f[i], &test);
            SystemTime::sleep(NANOSECONDS_IN_SECOND / 32);
            e.set(); // assume the thread already riched the waiting state in quarter of a second
            t.join();
        }
        {   // auto event / initial state unsignaled
            Event e(false, true);
            test.e = &e;
            Thread t(f[i], &test);
            t.join();
        }
    }
    {   // auto event / initial state unsignaled
        Event e(false, false);
        test.e = &e;
        test.timeout = NANOSECONDS_IN_SECOND / 32;
        Thread t(test1_wait_timeouted, &test);
        t.join();
    }
}

static void* test2_wait(void* p) {
    Test &t = *(Test*)p;
    int r = Event::wait(t.timeout, t.all, 2, t.e, t.s);
    assert(r == 0 || r == 1);
    return (void*)"test2_wait";
}

static void test2() {
    Test test = {0};
    test.timeout = NANOSECONDS_IN_SECOND / 4;
    for (int i = 0; i < 8; i++) {
        test.all = i % 2 == 0;
        {   // auto event / initial state unsignaled
            Event e(false);
            Event s(false);
            test.e = &e;
            test.s = &s;
            Thread t(test2_wait, &test);
            SystemTime::sleep(NANOSECONDS_IN_SECOND / 32);
            if (test.all) {
                e.set(); // assume the thread already riched the waiting state in quarter of a second
                s.set();
            } else {
                if (i % 4 == 0) {
                    e.set();
                } else {
                    s.set();
                }
            }
            t.join();
        }
    }
}

Event manual_unsignaled(true, false);
Event manual_signaled(true, true);
Event auto_unsignaled_0(false, false);
Event auto_unsignaled_1(false, false);
Event auto_signaled(false, true);

static void* wait_multiple(void*) {
    // Generally it is madness to wait on a mixute of 5 manual and auto-reset events. Here only for the sake of testing:
    int r = Event::waitAll(manual_unsignaled, manual_signaled, auto_signaled, auto_unsignaled_0, auto_unsignaled_1);
    assert(EVENT_WAIT_OBJECT_0 <= r && r <= EVENT_WAIT_OBJECT_0 + 4);
    return (void*)"wait_multiple";
}

static int testAll() {
    test1();
    test2();
    Thread t1(wait_multiple);
    Thread t2(wait_multiple);
    Thread t3(wait_multiple);
    SystemTime::sleep(NANOSECONDS_IN_SECOND / 32);
    manual_unsignaled.set();
    Thread* t[3] = {&t1, &t2, &t3};
    bool done = false;
    while (!done) {
        done = true;
        for (int i = 0; i < countof(t); i++) {
            if (t[i] != null) {
                if (t[i]->try_join()) {
                    t[i] = null;
                } else {
                    done = false;
                }
            }
        }
        if (!done) {
          auto_signaled.set();
          auto_unsignaled_0.set();
          auto_unsignaled_1.set();
        }
    }
    const char* r = (const char*)t1.join();
    t2.join();
    t3.join();
    assert(strcmp(r, "wait_multiple") == 0);
    printf("done\n");
    return 0;
}

int main(int, const char **) {
    testAll();
}