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
#ifndef __EVENT_H__
#define __EVENT_H__

#ifndef WIN32
#include <pthread.h>
#endif

enum { /* EVENT_ prefix because of Win32 #defines with the same names and values */
    EVENT_INFINITE = -1,
    EVENT_WAIT_OBJECT_0 = 0,
    EVENT_WAIT_TIMEOUT = 0x00000102,
    EVENT_WAIT_FAILED = -1,
    EVENT_WAIT_ABANDONED = 0x00000080
};

class Event {
public:
    Event(bool manual_reset = false, bool initial_state = false);
    virtual ~Event();
    Event& set();
    Event& reset();
    int wait(long long timeoutNanoseconds);
    int wait();

    static inline int waitAll(long long timeoutNanoseconds, Event& e0, Event& e1) {
        return wait(timeoutNanoseconds, true, 2, &e0, &e1);
    }

    static inline int waitAll(long long timeoutNanoseconds, Event& e0, Event& e1, Event& e2) {
        return wait(timeoutNanoseconds, true, 3, &e0, &e1, &e2);
    }

    static inline int waitAll(Event& e0, Event& e1) {
        return wait(EVENT_INFINITE, true, 2, &e0, &e1);
    }

    static inline int waitAll(Event& e0, Event& e1, Event& e2) {
        return wait(EVENT_INFINITE, true, 3, &e0, &e1, &e2);
    }

    static inline int waitAll(Event& e0, Event& e1, Event& e2, Event& e3) {
        return wait(EVENT_INFINITE, true, 4, &e0, &e1, &e2, &e3);
    }

    static inline int waitAll(Event& e0, Event& e1, Event& e2, Event& e3, Event& e4) {
        return wait(EVENT_INFINITE, true, 5, &e0, &e1, &e2, &e3, &e4);
    }

    static inline int waitAny(long long timeoutNanoseconds, Event& e0, Event& e1) {
        return wait(timeoutNanoseconds, false, 2, &e0, &e1);
    }

    static inline int waitAny(long long timeoutNanoseconds, Event& e0, Event& e1, Event& e2) {
        return wait(timeoutNanoseconds, false, 3, &e0, &e1, &e2);
    }

    static inline int waitAny(Event& e0, Event& e1) {
        return wait(EVENT_INFINITE, false, 2, &e0, &e1);
    }

    static inline int waitAny(Event& e0, Event& e1, Event& e2) {
        return wait(EVENT_INFINITE, false, 3, &e0, &e1, &e2);
    }

    static inline int waitAny(Event& e0, Event& e1, Event& e2, Event& e3) {
        return wait(EVENT_INFINITE, false, 4, &e0, &e1, &e2, &e3);
    }

    static inline int waitAny(Event& e0, Event& e1, Event& e2, Event& e3, Event& e4) {
        return wait(EVENT_INFINITE, false, 5, &e0, &e1, &e2, &e3, &e4);
    }

    static int wait(long long timeoutNanoseconds, bool wait_all, int n, ...);

private:
    static int wait(long long timeoutNanoseconds, bool wait_all, int n, Event* e[]);
#ifndef WIN32
    struct Blocked;
    void notifyAll();
    void insert(Blocked* b);
    void remove(Blocked* b);
    static void lock(int n, Event* e[]);
    static void unlock(int n, Event* e[]);
    static void markSignaled(Blocked& b, Event* e);
    static int  checkSignaled(Blocked& b, bool &all);
    static bool checkDuplicates(Blocked &b);
    Blocked* start; // list of blocked threads waiting for this event
    Blocked* end;
    bool manual;
    bool signaled;
    pthread_mutex_t mutex;
#else
    void* handle;
#endif
};

#endif /* __EVENT_H__ */
