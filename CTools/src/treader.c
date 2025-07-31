#include "CTools/treader.h"

#ifdef _WIN32

int thread_create(thread_t *thread, void *(*start_routine)(void *), void *arg) {
    *thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)start_routine, arg, 0, NULL);
    return *thread == NULL ? -1 : 0;
}
#include <stdio.h>
int thread_join(thread_t thread) {
    WaitForSingleObject(thread, INFINITE);
    CloseHandle(thread);
    return 0;
}

int mutex_init(mutex_t *mutex) {
    *mutex = CreateMutex(NULL, FALSE, NULL);
    return *mutex == NULL ? -1 : 0;
}

int mutex_lock(mutex_t *mutex) {
    return WaitForSingleObject(*mutex, INFINITE) == WAIT_FAILED ? -1 : 0;
}

int mutex_unlock(mutex_t *mutex) {
    return ReleaseMutex(*mutex) == 0 ? -1 : 0;
}

int mutex_destroy(mutex_t *mutex) {
    return CloseHandle(*mutex) == 0 ? -1 : 0;
}

#else

int thread_create(thread_t *thread, void *(*start_routine)(void *), void *arg) {
    return pthread_create(thread, NULL, start_routine, arg);
}

int thread_join(thread_t thread) {
    return pthread_join(thread, NULL);
}

int mutex_init(mutex_t *mutex) {
    return pthread_mutex_init(mutex, NULL);
}

int mutex_lock(mutex_t *mutex) {
    return pthread_mutex_lock(mutex);
}

int mutex_unlock(mutex_t *mutex) {
    return pthread_mutex_unlock(mutex);
}

int mutex_destroy(mutex_t *mutex) {
    return pthread_mutex_destroy(mutex);
}

#endif
