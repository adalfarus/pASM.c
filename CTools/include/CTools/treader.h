#ifndef TREADER_H
#define TREADER_H

#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif

// Define the unified types
#ifdef _WIN32
typedef HANDLE thread_t;
typedef HANDLE mutex_t;
#else
typedef pthread_t thread_t;
typedef pthread_mutex_t mutex_t;
#endif

// Define the unified functions
#ifdef __cplusplus
extern "C" {
#endif

int thread_create(thread_t *thread, void *(*start_routine)(void *), void *arg);
int thread_join(thread_t thread);
int mutex_init(mutex_t *mutex);
int mutex_lock(mutex_t *mutex);
int mutex_unlock(mutex_t *mutex);
int mutex_destory(mutex_t *mutex);

#ifdef __cplusplus
}
#endif

#endif //TREADER_H
