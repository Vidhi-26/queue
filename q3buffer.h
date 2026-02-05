#ifndef Q3BUFFER_H
#define Q3BUFFER_H
#include <cassert>

#ifdef NOBUSY
#include "BargingCheck.h"
#endif // NOBUSY

template<typename T> 
class BoundedBuffer {
/* Circular buffer */
T * buffer;         
unsigned int front, back;
const unsigned int capacity;  
unsigned int sz;

/* Synchronization */
unsigned long int waits;
bool isPoisoned;
uOwnerLock mutex;
uCondLock canProduce, canConsume;

#ifdef NOBUSY
    /* Barging-avoidance */
    BCHECK_DECL;
    uCondLock gate;
    bool baton = false;

    // --- Helper routines to reshape control flow (definition in-class for templates) ---
    void waitIfBargerPresent_() {
        if ( baton ) {
            ++waits;
            gate.wait( mutex );              // park one-shot barger
            // do not touch baton here; caller decides baton state after wakeup
        }
    }

    void blockIfFullAndDesignate_() {
        if ( sz != capacity ) return;

        // allow an already-queued barger to pass first, then park producer
        if ( !gate.empty() ) gate.signal();
        baton = true;                        // designated waiter
        ++waits;
        canProduce.wait( mutex );
        baton = false;                       // woke by consumer
    }

    void handoffAfterInsert_() {
        if ( !canConsume.empty() ) {
            baton = true;
            CONS_SIGNAL( canConsume );       // required macro placement
            canConsume.signal();
        } else if ( !gate.empty() ) {
            baton = true;
            gate.signal();
        } else {
            baton = false;
        }
    }

    void preWaitIfEmptyOrPoison_() {
        if ( sz != 0 ) return;

        // let a barger through first if present
        if ( !gate.empty() ) {
            gate.signal();
            baton = true;
        }

        if ( isPoisoned ) {
            mutex.release();
            _Throw Poison();
        }

        ++waits;
        canConsume.wait( mutex );            // designated consumer wakes
        baton = false;

        // if wakeup due to poison broadcast and still empty → terminate
        if ( isPoisoned && sz == 0 ) {
            mutex.release();
            _Throw Poison();
        }
    }

    bool finalizePoisonEmptyCase_() {
        // If buffer drained and poisoned, release any remaining consumers
        if ( sz == 0 && isPoisoned ) {
            baton = false;
            canConsume.broadcast();          // NOTE: no CONS_SIGNAL() before poison broadcast
            return true;                     // caller should early-return with elem already saved
        }
        return false;
    }

    void handoffAfterRemove_() {
        if ( !canProduce.empty() ) {
            baton = true;
            PROD_SIGNAL( canProduce );       // required macro placement
            canProduce.signal();
        } else if ( !gate.empty() ) {
            baton = true;
            gate.signal();
        } else {
            baton = false;
        }
    }
#endif // NOBUSY

public:
    _Exception Poison {};
    BoundedBuffer( const unsigned int size = 10 )
        : buffer(new T[size]), front(0), back(0), capacity(size), sz(0), 
          waits(0), isPoisoned(false) {}
    unsigned long int blocks() { return waits; }
    void poison();
    void insert( T elem );
    T remove() __attribute__(( warn_unused_result ));
    ~BoundedBuffer() { delete[] buffer; }
};

#ifdef BUSY

/* Busy-waiting implementation */
template<typename T>
void BoundedBuffer<T>::insert(T elem) {
    mutex.acquire();
    while (sz == capacity) {
        ++waits;
        canProduce.wait(mutex);
    }

    assert(sz < capacity);

    buffer[back] = elem;
    back = (back + 1) % capacity;
    ++sz;

    canConsume.signal();
    mutex.release();
}

template<typename T>
T BoundedBuffer<T>::remove() {
    mutex.acquire();
    while (sz == 0){
        if (isPoisoned){
            mutex.release();
            _Throw Poison();
        }
        ++waits;
        canConsume.wait(mutex);
    }

    assert(sz > 0);

    T elem = buffer[front];
    front = (front + 1) % capacity;
    --sz;

    canProduce.signal();
    mutex.release();
    return elem;
}

template<typename T>
void BoundedBuffer<T>::poison() {
    mutex.acquire();
    isPoisoned = true;
    canConsume.broadcast();
    mutex.release();
}

#endif // BUSY

#ifdef NOBUSY

/* Non-busy-waiting implementation with barging avoidance */
template<typename T>
void BoundedBuffer<T>::insert( T elem ) {
    mutex.acquire();
    PROD_ENTER;                              // macro: immediately after acquiring mutex

    // 1) One-shot park if a baton is in flight (anti-barging gate)
    waitIfBargerPresent_();

    // 2) If full, coordinate designation and block once (no loop)
    blockIfFullAndDesignate_();

    // 3) Do the insert
    assert( sz < capacity );
    buffer[back] = elem;
    back = (back + 1) % capacity;
    ++sz;
    INSERT_DONE;                             // macro: immediately after insertion

    // 4) Choose successor fairly (consumer preferred, else gate, else clear baton)
    handoffAfterInsert_();

    mutex.release();
}

template<typename T>
T BoundedBuffer<T>::remove() {
    mutex.acquire();
    CONS_ENTER;                              // macro: immediately after acquiring mutex

    // 1) One-shot park if a baton is in flight (anti-barging gate)
    waitIfBargerPresent_();
    baton = false;                           // consumer becomes active designee

    // 2) If empty (and possibly poisoned), coordinate waits/termination
    preWaitIfEmptyOrPoison_();

    // 3) Remove element
    assert( sz > 0 );
    T elem = buffer[front];
    front = (front + 1) % capacity;
    --sz;
    REMOVE_DONE;                             // macro: immediately after removal

    // 4) If drained and poisoned, wake everyone and terminate consumer path
    if ( finalizePoisonEmptyCase_() ) {
        mutex.release();
        return elem;
    }

    // 5) Otherwise pass baton appropriately
    handoffAfterRemove_();

    mutex.release();
    return elem;
}

template<typename T>
void BoundedBuffer<T>::poison() {
    // drain the gate so no newcomer is mid-transition when we flip the flag
    while ( !gate.empty() ) uThisTask().yield();
    mutex.acquire();
    isPoisoned = true;
    canConsume.broadcast();                  // NOTE: no CONS_SIGNAL before poison broadcast
    mutex.release();
}

#endif // NOBUSY

#endif // Q3BUFFER_H
