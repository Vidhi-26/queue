#ifndef Q3PRODUCER_H
#define Q3PRODUCER_H
#include "q3buffer.h"
#include <uPRNG.h>

/*
* Producer task that produces integers from 1 to Produce,
* yielding for a random time up to Delay between productions.
*/
_Task Producer {
BoundedBuffer<int>& buffer;
const int Produce;
const int Delay;
void main() {
    PRNG prng;
    for (int i = 1; i <= Produce; i++) {
        yield(prng(Delay));  /* random delay before producing */
        buffer.insert(i);
    }
}
public:
	Producer( BoundedBuffer<int> & buffer, const int Produce, const int Delay ): 
        buffer(buffer), Produce(Produce), Delay(Delay) {}
};

#endif // Q3PRODUCER_H
