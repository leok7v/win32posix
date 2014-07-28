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
#ifndef __SystemTime_H__
#define __SystemTime_H__

#include <time.h>

#ifdef WIN32
#pragma warning(push)
#pragma warning(disable: 4514 4820)

typedef struct timespec {
    long tv_sec;
    long tv_nsec;
} typespec;
#endif

enum { 
  NANOSECONDS_IN_MICROSECOND = 1000,
  NANOSECONDS_IN_MILLISECOND = 1000 * NANOSECONDS_IN_MICROSECOND,
  NANOSECONDS_IN_SECOND = 1000 * NANOSECONDS_IN_MILLISECOND
};

class SystemTime {
public:
    static inline long long mono() { struct timespec ts; monoTimespec(ts); return fromTimespec(ts); }
    static inline long long wall() { struct timespec ts; wallTimespec(ts); return fromTimespec(ts); }
    static inline long long cpu()  { struct timespec ts; cpuTimespec(ts); return fromTimespec(ts); }
    static void sleep(long long nanoseconds);
    static void wallTimespec(struct timespec &ts); /* a.k.a. CLOCK_REALTIME or better */
    static void monoTimespec(struct timespec &ts); /* a.k.a. CLOCK_MONOTONIC not affected by NTP adjustments */
    static void cpuTimespec(struct timespec &ts);  /* calling thread's time elapsed time */
    static inline void toTimespec(struct timespec& r, long long nanoseconds) {
      r.tv_sec = (long)(nanoseconds / NANOSECONDS_IN_SECOND);
      r.tv_nsec = (long)(nanoseconds % NANOSECONDS_IN_SECOND);
    }
    static inline long long fromTimespec(const struct timespec& ts) {
        return NANOSECONDS_IN_SECOND * (long long)ts.tv_sec + ts.tv_nsec;
    }
    static unsigned long long toWin100nsSystemTime(long long nanoseconds);
private:
    SystemTime() { /* do not instantiate */ }
};

/*
 IMPORTANT: to make nanoseconds "signed long long" (ease of use) and
 to postpone int128 transition to 2,554AD the base for
 SystemTime is the *nix Epoch: Jan 1, 1970

 on Windows: GetSystemTimeAsFileTime is based on:
             nanoseconds since January 1, 1601 00:00:00 +0000 (UTC) (with >= 100-nanosecond precision)
 *nix & bsd: nanoseconds since the Epoch, 1970-01-01 00:00:00 +0000 (UTC)

 One julian year in nanoseconds: 0x00701d7851f6c000
 One solar centrury in nanocseconds: 0x2bcb45b5182a8000
 
 Signed 64 bit value wraps around in ~292 years on Windows means negative nanoseconds since ~1893
 Unsigned 64 bit value wraps around in ~584 years 
 On Windows: 1601 + 584 = ~2,185AD.
 On *nix:    1970 + 584 = ~2,554AD.

 Please upgrade to signed 128 bit int when universally supported
*/

#ifdef WIN32
#pragma warning(pop)
#endif

#endif /* __SystemTime_H__ */
