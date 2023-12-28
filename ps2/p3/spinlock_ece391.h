#ifndef SPINLOCK_ECE391_H
#define SPINLOCK_ECE391_H

#include <pthread.h>

// Define a spinlock structure
typedef struct {
    pthread_mutex_t mutex;
} spinlock_t;

// Initialize a spinlock
void spinlock_init_ece391(spinlock_t *lock);

// Acquire the spinlock
void spinlock_lock_ece391(spinlock_t *lock);

// Release the spinlock
void spinlock_unlock_ece391(spinlock_t *lock);

// Destroy the spinlock
void spinlock_destroy_ece391(spinlock_t *lock);

#endif /* SPINLOCK_ECE391_H */