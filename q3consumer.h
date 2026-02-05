#ifndef Q3CONSUMER_H
#define Q3CONSUMER_H
#include "q3buffer.h"
#include <uPRNG.h>

/*
* Each Consumer repeatedly removes integers from the shared buffer
* and adds them to its private sum.
*/
_Task Consumer {
BoundedBuffer<int>& buffer;
const int Delay;
int& sum;
void main(){
    PRNG prng;
    try {
        while (true) {
            yield(prng(Delay));  /* random delay before consuming */
            int val = buffer.remove();
            sum += val;
        }
    } catch (BoundedBuffer<int>::Poison&) {
        return;                 /* terminate task if buffer poisoned */
    }
}
public:
	Consumer( BoundedBuffer<int> & buffer, const int Delay, int & sum ): buffer(buffer), Delay(Delay), sum(sum) {

    }
};

#endif // Q3CONSUMER_H
