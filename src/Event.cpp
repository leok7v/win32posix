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
#include "Event.h"
#include "SystemTime.h"
#include <assert.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>

#define null NULL

#ifdef WIN32
#pragma warning(disable: 4820 4514 4668 4189 4711)
#include <Windows.h>
#include <malloc.h>

static inline DWORD milliseconds(long long nanoseconds) {
    return nanoseconds == EVENT_INFINITE ? INFINITE : (DWORD)(nanoseconds / NANOSECONDS_IN_MILLISECOND);
}

Event::Event(bool manual_reset, bool initial_state) {
    handle = ::CreateEventA(null, manual_reset, initial_state, null);
    #pragma warning(suppress: 4365)
    assert(handle != null);
}

Event::~Event() {
    #pragma warning(suppress: 4365)
    assert(handle != null);
    ::CloseHandle(handle);
}

Event& Event::set() {
    ::SetEvent(handle);
    return *this;
}

Event& Event::reset() {
    ::ResetEvent(handle);
    return *this;
}

int Event::wait(long long timeoutNanoseconds) {
    return (int)::WaitForSingleObjectEx(handle, milliseconds(timeoutNanoseconds), true);
}

int Event::wait() {
    return wait(EVENT_INFINITE);
}

int Event::wait(long long timeoutNanoseconds, bool wait_all, int n, ...) {
    va_list ap;
    va_start(ap, n);
    Event** e = (Event**)_alloca(n * sizeof(Event*));
    for (int i = 0; i < n; i++) {
        e[i] = va_arg(ap, Event*);
    }
    va_end(ap);
    return wait(timeoutNanoseconds, wait_all, n, e);
}

int Event::wait(long long timeoutNanoseconds, bool wait_all, int n, Event* e[]) {
    HANDLE* handles = (HANDLE*)_alloca(n * sizeof(HANDLE));
    for (int i = 0; i < n; i++) {
        handles[i] = e[i]->handle;
    }
    return (int)::WaitForMultipleObjects((DWORD)n, handles, wait_all, milliseconds(timeoutNanoseconds));
}

#else

pthread_condattr_t* clock_monotonic = null;
pthread_condattr_t  clock_monotonic_imp;

struct Event::Blocked { // node for blocked thread waiting on an event
    pthread_mutex_t mutex_signaled;
    pthread_cond_t  signal;
    int n;
    bool* signaled;
    Event** events;
    struct Blocked *prev, *next;
};

Event::Event(bool manual_reset, bool initial_state) :
    start(null), end(null), manual(manual_reset), signaled(initial_state) {
    pthread_mutex_init(&mutex, null);
}

Event::~Event() {
    assert(start == null && end == null); // nobody still waiting on it
    pthread_mutex_destroy(&mutex);
}

Event& Event::set() {
    pthread_mutex_lock(&mutex);
    signaled = true;
    notifyAll();
    pthread_mutex_unlock(&mutex);
    return *this;
}

Event& Event::reset() {
    pthread_mutex_lock(&mutex);
    signaled = false;
    pthread_mutex_unlock(&mutex);
    return *this;
}

int Event::wait(long long timeoutNanoseconds) {
    return wait(timeoutNanoseconds, false, 1, this);
}

int Event::wait() {
    return wait(EVENT_INFINITE);
}

int Event::wait(long long timeoutNanoseconds, bool wait_all, int n, ...) {
    va_list ap;
    va_start(ap, n);
    Event* e[n];
    for (int i = 0; i < n; i++) {
        e[i] = va_arg(ap, Event*);
    }
    va_end(ap);
    return wait(timeoutNanoseconds, wait_all, n, e);
}

void Event::notifyAll() {
    Blocked* p = start;
    while (p != null && signaled) {
        pthread_mutex_lock(&p->mutex_signaled);
        markSignaled(*p, this);
        pthread_cond_signal(&p->signal);
        pthread_mutex_unlock(&p->mutex_signaled);
        if (!manual) {
            signaled = false;
        }
        p = p == end ? null : p->next;
    }
}

void Event::insert(Blocked* b) {
    if (start == null) {
        start = end = b;
    } else {
        end->next = b;
        b->next = end;
        end = b;
    }
}

void Event::remove(Blocked* b) {
    if (start == end) {
        start = end = NULL;
    } else if (b == start) {
        start = start->next;
    } else if (b == end) {
        end = end->next;
    } else {
        Blocked* ptr = start->next;
        while (ptr != b) {
            ptr = ptr->next;
        }
        assert(ptr == b);
        ptr->prev->next = ptr->next;
        ptr->next->prev = ptr->prev;
    }
}

void Event::lock(int n, Event* e[]) {
    for (int i = 0; i < n; i++) { pthread_mutex_lock(&e[i]->mutex); }
}

void Event::unlock(int n, Event* e[]) {
    for (int i = 0; i < n; i++) { pthread_mutex_unlock(&e[i]->mutex); }
}

void Event::markSignaled(Blocked &b, Event* e) {
    for (int i = 0; i < b.n; i++) {
        if (b.events[i] == e) {
            b.signaled[i] = e->signaled;
            break;
        }
    }
}

int Event::checkSignaled(Blocked &b, bool &all) {
    int first = -1;
    all = true;
    for (int i = 0; i < b.n; i++) {
        all = all && b.signaled[i];
        if (b.signaled[i]) {
            if (first < 0) {
                first = i;
            }
        }
    }
    return first;
}

bool Event::checkDuplicates(Blocked &b) {
    for (int i = 0; i < b.n; i++) {
        for (int j = i + 1; j < b.n; j++) {
            assert(b.events[i] != b.events[j]); // cannot wait on the same event twice
            if (b.events[i] == b.events[j]) {
                return true;
            }
        }
    }
    return false;
}

typedef int (*timedwait_f)(pthread_cond_t*, pthread_mutex_t*, const struct timespec*);
#if defined(__ANDROID__) // see: http://code.google.com/p/android/issues/detail?id=36086
static timedwait_f monotonic_cond_timedwait = pthread_cond_timedwait_monotonic;
#else
static timedwait_f monotonic_cond_timedwait = pthread_cond_timedwait;
#endif

int Event::wait(long long timeoutNanoseconds, bool wait_all, int n, Event* e[]) {
    if (n <= 0) {
        return EVENT_WAIT_FAILED;
    }
    bool signaled[n];
    memset(&signaled, 0, sizeof(signaled));
    int return_value = EVENT_WAIT_OBJECT_0;
    Blocked waiting;
    memset(&waiting, 0, sizeof(waiting));
    waiting.n = n;
    waiting.signaled = signaled;
    waiting.events = e;
    if (checkDuplicates(waiting)) {
        return EVENT_WAIT_FAILED;
    }
    lock(n, e);
    for (int i = 0; i < n; i++) {
        waiting.signaled[i] = waiting.events[i]->signaled;
    }
    bool all = false;
    int first = checkSignaled(waiting, all);
    if ((wait_all && !all) || first < 0) {
        struct timespec ts = {0};
        if (timeoutNanoseconds != EVENT_INFINITE) {
            SystemTime::toTimespec(ts, SystemTime::mono() + timeoutNanoseconds);
        }
#if defined(CLOCK_MONOTONIC) && !defined(__ANDROID__) // see: http://code.google.com/p/android/issues/detail?id=36086
        if (clock_monotonic == null) {
            clock_monotonic = &clock_monotonic_imp;
            pthread_condattr_init(clock_monotonic);
            pthread_condattr_setclock(clock_monotonic, CLOCK_MONOTONIC);
        }
#endif
        pthread_mutex_init(&waiting.mutex_signaled, null);
        pthread_cond_init(&waiting.signal, clock_monotonic);
        for (int i = 0; i < n; i++) { e[i]->insert(&waiting); }
        bool done = false;
        while (!done) {
            pthread_mutex_lock(&waiting.mutex_signaled);
            unlock(n, e);
            int r;
            if (timeoutNanoseconds == EVENT_INFINITE) {
                r = pthread_cond_wait(&waiting.signal, &waiting.mutex_signaled);
            } else {
                r = monotonic_cond_timedwait(&waiting.signal, &waiting.mutex_signaled, &ts);
            }
            pthread_mutex_unlock(&waiting.mutex_signaled);
            lock(n, e);
            first = checkSignaled(waiting, all);
            if (r == ETIMEDOUT) {
                return_value = EVENT_WAIT_TIMEOUT;
                done = true;
            } else if (r != 0) {
                return_value = EVENT_WAIT_FAILED;
                done = true;
            } else if (!wait_all && first >= 0) {
                return_value = EVENT_WAIT_OBJECT_0 + first;
                done = true;
            } else {
                done = wait_all && all;
            }
            if (done) {
                for (int i = 0; i < n; i++) { e[i]->remove(&waiting); }
            }
        }
        pthread_cond_destroy(&waiting.signal);
        pthread_mutex_destroy(&waiting.mutex_signaled);
    }
    unlock(n, e);
    return return_value;
}

#endif
