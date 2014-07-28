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
#include "SystemTime.h"
#include <assert.h>

#ifdef WIN32
#pragma warning(disable: 4820 4514 4668)
#include <Windows.h>

// http://www.windowstimestamp.com/description

static const unsigned long long TIMESPAN1601TO1970IN100NS = 0x019db1de019db1deLL; /* 116444732449468894LL */

void SystemTime::wallTimespec(struct timespec &ts) {
    ::FILETIME ft;
    ::GetSystemTimeAsFileTime(&ft);
    // TODO: GetSystemTimePreciseAsFileTime for Win8 & WinServer 2012 and higher
    unsigned long long t100 = (unsigned long long)(((unsigned long long)ft.dwHighDateTime << 32U) | ft.dwLowDateTime);
    long long t = (long long)((t100 - TIMESPAN1601TO1970IN100NS) * 100);
    #pragma warning(suppress: 4365)
    assert(t > 0); // is it 2,554AD yet?
    printf("now in nanos=0x%016llx\n", t);
    toTimespec(ts, t);
}

unsigned long long SystemTime::toWin100nsSystemTime(long long nanesecodns) {
    return (unsigned long long)(nanesecodns / 100U + TIMESPAN1601TO1970IN100NS);
}

void SystemTime::monoTimespec(struct timespec &ts) {
    wallTimespec(ts);
}

void SystemTime::cpuTimespec(struct timespec &ts) {
    wallTimespec(ts);
}

void SystemTime::sleep(long long timeoutNanoseconds) {
    ::Sleep((DWORD)(timeoutNanoseconds / NANOSECONDS_IN_MILLISECOND));
}

#else

#include <sys/time.h>

#ifdef __MACH__

#include <mach/mach.h>
#include <mach/thread_info.h>
#include <mach/mach_time.h>

static void mach_clock_gettime(struct timespec* tm);
static void mach_clock_get_thread_time(struct timespec* tm);

#endif

// see: http://stackoverflow.com/questions/14270300/what-is-the-difference-between-clock-monotonic-clock-monotonic-raw

void SystemTime::wallTimespec(struct timespec &ts) {
#if defined(__ANDROID__) && defined(CLOCK_REALTIME_HR)
    clock_gettime(CLOCK_REALTIME_HR, &ts);
#elif defined(CLOCK_REALTIME)
    clock_gettime(CLOCK_REALTIME, &ts);
#elif defined(__MACH__)
    mach_clock_gettime(&ts);
#else
    struct timeval tv;
    gettimeofday(&tv, 0);
    toTimespec(ts, (long long)tv.tv_sec * NANOSECONDS_IN_SECOND + tv.tv_usec * 1000);
#endif
}

void SystemTime::monoTimespec(struct timespec &ts) {
#if defined(CLOCK_MONOTONIC)
    clock_gettime(CLOCK_MONOTONIC, &ts);
#elif defined(__MACH__)
    mach_clock_gettime(&ts);
#else
    wallTimespec(ts);
#endif
}

// see: http://stackoverflow.com/questions/6814792/why-is-clock-gettime-so-erratic

void SystemTime::cpuTimespec(struct timespec &ts) {
#if defined(CLOCK_THREAD_CPUTIME_ID)
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts);
#elif __MACH__
    mach_clock_get_thread_time(&ts);
#else
    clock_gettime(CLOCK_REALTIME, &tm);
#endif
}

void SystemTime::sleep(long long nanoseconds) {
    struct timespec rq = { (long)(nanoseconds / NANOSECONDS_IN_SECOND), (long)(nanoseconds % NANOSECONDS_IN_SECOND) };
    struct timespec rm = { 0, 0 };
    nanosleep(&rq, &rm);
}

#ifdef __MACH__

static void mach_clock_gettime(struct timespec* tm) {
    static mach_timebase_info_data_t tb;
    static long long td_delta;
    long long t = mach_absolute_time(); // rumored not to be influenced by NTP adjustments
    if (tb.denom == 0) {
        struct timeval tv;
        gettimeofday(&tv, 0); // remember the delta once per application life time
        td_delta = (long long)tv.tv_sec * NANOSECONDS_IN_SECOND + tv.tv_usec * 1000 - t;
        mach_timebase_info(&tb);
    }
    t = t * tb.numer / tb.denom + td_delta;
    tm->tv_sec = (__darwin_time_t)(t / NANOSECONDS_IN_SECOND);
    tm->tv_nsec = (long)(t % NANOSECONDS_IN_SECOND);
}

static void mach_clock_get_thread_time(struct timespec* tm) {
    struct thread_basic_info tbi = {{0}};
    mach_msg_type_number_t flag = THREAD_BASIC_INFO_COUNT;
    int r = thread_info(mach_thread_self(), THREAD_BASIC_INFO, (task_info_t)&tbi, &flag);
    if (r == KERN_SUCCESS) {
        long long sec = tbi.user_time.seconds + tbi.system_time.seconds;
        long long nsec = (tbi.user_time.microseconds + tbi.system_time.microseconds) * 1000LL +
                          sec * NANOSECONDS_IN_SECOND;
        tm->tv_sec = (long)(nsec / NANOSECONDS_IN_SECOND);
        tm->tv_nsec = (long)(nsec % NANOSECONDS_IN_SECOND);
    }
    assert(r == KERN_SUCCESS && (tm->tv_sec != 0 || tm->tv_nsec != 0));
}

#endif

#endif // WIN32