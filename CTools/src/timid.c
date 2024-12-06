#include "CTools/timid.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <inttypes.h>
#include <CTools/treader.h>

// Global static array for timers
static TimidTimer timers[MAX_TIMERS];

// Get current time in nanoseconds
uint64_t current_time_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1e9 + ts.tv_nsec;
}

// Initialize all timers
void timer_init_all(void) {
    for (int i = 0; i < MAX_TIMERS; i++) {
        timers[i].start_time = 0;
        timers[i].end_time = 0;  // Need to indicate if timer has stopped or not?
        timers[i].paused_time = 0;
        timers[i].is_paused = 0;
        mutex_init(&timers[i].lock);
    }
}

// Start timer
void timer_start(const int index) {
    if (index >= MAX_TIMERS) return;
    mutex_lock(&timers[index].lock);
    timers[index].start_time = current_time_ns();
    timers[index].paused_time = 0;
    timers[index].is_paused = 0;
    mutex_unlock(&timers[index].lock);
}

// Pause timer
void timer_pause(const int index) {
    if (index >= MAX_TIMERS) return;
    mutex_lock(&timers[index].lock);
    if (!timers[index].is_paused) {
        timers[index].paused_time = current_time_ns();
        timers[index].is_paused = 1;
    }
    mutex_unlock(&timers[index].lock);
}

void _resume(const int index) {
    if (timers[index].is_paused) {
        timers[index].start_time += (current_time_ns() - timers[index].paused_time);
        timers[index].is_paused = 0;
    }
}

// Resume timers
void timer_resume(const int index) {
    if (index >= MAX_TIMERS) return;
    mutex_lock(&timers[index].lock);
    _resume(index);
    mutex_unlock(&timers[index].lock);
}

// Stop timer
void timer_stop(const int index) {
    if (index >= MAX_TIMERS) return;
    mutex_lock(&timers[index].lock);
    _resume(index);
    timers[index].end_time = current_time_ns();
    mutex_unlock(&timers[index].lock);
}

// Get elapsed time in nanoseconds
uint64_t timer_get_elapsed(const int index) {
    if (index >= MAX_TIMERS) return 0;
    mutex_lock(&timers[index].lock);
    const uint64_t elapsed = (timers[index].end_time ? timers[index].end_time : current_time_ns()) - timers[index].start_time;
    mutex_unlock(&timers[index].lock);
    return elapsed;
}

// Reset timer
void timer_reset(const int index) {
    if (index >= MAX_TIMERS) return;
    mutex_lock(&timers[index].lock);
    timers[index].start_time = 0;
    timers[index].end_time = 0;
    timers[index].paused_time = 0;
    timers[index].is_paused = 0;
    mutex_unlock(&timers[index].lock);
}

// Get readable time format
const char* get_readable_time(const uint64_t ns, const TimeFormat format) {
    static char buffer[256];
    double time_val;
    const char *Time[] = {"weeks", "days", "hours", "minutes", "seconds", "milliseconds", "microseconds"};

    switch (format) {
        case WEEKS:
            time_val = ns / 1e9 / 60 / 60 / 24  / 7;
            break;
        case DAYS:
            time_val = ns / 1e9 / 60 / 60 / 24;
            break;
        case HOURS:
            time_val = ns / 1e9 / 60 / 60;
            break;
        case MINUTES:
            time_val = ns / 1e9 / 60;
            break;
        case SECONDS:
            time_val = ns / 1e9;
            break;
        case MILLISEC:
            time_val = ns / 1e6;
            break;
        case MICROSEC:
            time_val = ns / 1e3;
            break;
        default:
            time_val = -1;
    }
    sprintf(buffer, "%.2f %s", time_val, format < 6 ? Time[format] : "");
    return buffer;
}
