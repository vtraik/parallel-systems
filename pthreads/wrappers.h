#pragma once

#include <pthread.h>

typedef enum{
    MUTEX_LOCK,
    RW_LOCK
} TYPE;

typedef enum{
    READ,
    WRITE
} LOCK_TYPE;

int lock(void *lock, TYPE type, LOCK_TYPE lock_type);
int unlock(void *lock, TYPE type);
