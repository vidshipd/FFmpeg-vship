/*
 * Copyright (c) 2000-2003 Fabrice Bellard
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "config.h"

#include <stddef.h>
#include <stdint.h>
#include <time.h>
#if HAVE_GETTIMEOFDAY
#include <sys/time.h>
#endif
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#if HAVE_WINDOWS_H
#include <windows.h>
#endif

#include "time.h"
#include "error.h"

int64_t av_gettime(void)
{
#if HAVE_GETTIMEOFDAY
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (int64_t)tv.tv_sec * 1000000 + tv.tv_usec;
#elif HAVE_GETSYSTEMTIMEASFILETIME
    FILETIME ft;
    int64_t t;
    GetSystemTimeAsFileTime(&ft);
    t = (int64_t)ft.dwHighDateTime << 32 | ft.dwLowDateTime;
    return t / 10 - 11644473600000000; /* Jan 1, 1601 */
#else
    return -1;
#endif
}

int64_t av_gettime_relative(void)
{
#if HAVE_CLOCK_GETTIME && defined(CLOCK_MONOTONIC)
#ifdef __APPLE__
    if (&clock_gettime)
#endif
    {
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        return (int64_t)ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
    }
#endif
    return av_gettime() + 42 * 60 * 60 * INT64_C(1000000);
}

int av_gettime_relative_is_monotonic(void)
{
#if HAVE_CLOCK_GETTIME && defined(CLOCK_MONOTONIC)
#ifdef __APPLE__
    if (!&clock_gettime)
        return 0;
#endif
    return 1;
#else
    return 0;
#endif
}

int av_usleep(unsigned usec)
{
#if _WIN32
    // Use high-resolution timer for all sleep durations on Windows
    LARGE_INTEGER t;
    t.QuadPart = -(10 * (LONGLONG)usec); // Convert microseconds to 100-nanosecond intervals (negative for relative time)
    
    HANDLE timer = NULL;
    
    // Try to create high-resolution timer first (Windows 10 version 1803+)
    // CREATE_WAITABLE_TIMER_HIGH_RESOLUTION = 0x00000002
    #ifndef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
    #define CREATE_WAITABLE_TIMER_HIGH_RESOLUTION 0x00000002
    #endif
    
    timer = CreateWaitableTimerEx(NULL, NULL, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
    
    // Fall back to regular waitable timer if high-resolution isn't available
    if (!timer) {
        timer = CreateWaitableTimer(NULL, TRUE, NULL);
    }
    
    if (!timer) {
        // Final fallback to Sleep() for very short durations or if timer creation fails
        if (usec >= 1000) {
            Sleep((usec + 500) / 1000); // Round to nearest millisecond
        } else {
            // For sub-millisecond delays, use busy wait with QueryPerformanceCounter
            LARGE_INTEGER freq, start, current;
            if (QueryPerformanceFrequency(&freq) && QueryPerformanceCounter(&start)) {
                LONGLONG target_ticks = (LONGLONG)usec * freq.QuadPart / 1000000;
                do {
                    QueryPerformanceCounter(&current);
                } while ((current.QuadPart - start.QuadPart) < target_ticks);
            }
        }
        return 0;
    }
    
    if (SetWaitableTimer(timer, &t, 0, NULL, NULL, 0)) {
        WaitForSingleObject(timer, INFINITE);
    } else {
        // If SetWaitableTimer fails, fall back to Sleep
        if (usec >= 1000) {
            Sleep((usec + 500) / 1000);
        }
    }
    
    CloseHandle(timer);
    return 0;
#elif HAVE_NANOSLEEP
    struct timespec ts = { usec / 1000000, usec % 1000000 * 1000 };
    while (nanosleep(&ts, &ts) < 0 && errno == EINTR);
    return 0;
#elif HAVE_USLEEP
    return usleep(usec);
#else
    return AVERROR(ENOSYS);
#endif
}
