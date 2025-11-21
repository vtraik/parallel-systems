#include "wrappers.h"

int lock(void *lock, TYPE type, LOCK_TYPE lock_type) {
    if (type == MUTEX_LOCK) {
        return pthread_mutex_lock((pthread_mutex_t *)lock);
    } else{
        if (lock_type == WRITE) 
            return pthread_rwlock_wrlock((pthread_rwlock_t *)lock);
        else 
            return pthread_rwlock_rdlock((pthread_rwlock_t *)lock);
        
    }
}

int unlock(void *lock, TYPE type) {
    if (type == MUTEX_LOCK) {
        return pthread_mutex_unlock((pthread_mutex_t *)lock);
    } else{
        return pthread_rwlock_unlock((pthread_rwlock_t *)lock);
    }
}

