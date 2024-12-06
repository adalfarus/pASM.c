#ifndef TIMID_H
#define TIMID_H

#include <stdint.h>
#include <time.h>
#include "CTools/treader.h"

#define MAX_TIMERS 100
#define MAX_THREADS 5

// Time Format Enums
typedef enum {
    WEEKS,
    DAYS,
    HOURS,
    MINUTES,
    SECONDS,
    MILLISEC,
    MICROSEC
} TimeFormat;

// Timer structure
typedef struct {
    uint64_t start_time;
    uint64_t end_time;
    uint64_t paused_time;
    int is_paused;
    mutex_t lock;
} TimidTimer;

// Function declarations
void timer_init_all(void);
void timer_start(int index);
void timer_pause(int index);
void timer_resume(int index);
void timer_stop(int index);
uint64_t timer_get_elapsed(int index);
void timer_reset(int index);
uint64_t current_time_ns(void);
const char* get_readable_time(uint64_t ns, TimeFormat format);

#endif //TIMID_H
