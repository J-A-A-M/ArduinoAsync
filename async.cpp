#include "async.h"

Async::Async(unsigned short sizePool) {
	nodePool = (ScheduleNode*) malloc(sizeof(ScheduleNode)*sizePool);
	for(unsigned short i = 0; i < sizePool; i++) {
        *(nodePool + i) = {
            .lastExecution = 0,
            .interval = 0,
            .flags = 0b00000010,
            .function = nullptr
		};
	}
	nNodes = 0;
	this->sizePool = sizePool;
}

short Async::setInterval(void (*fun)(void), unsigned long time) {
	if(fun == nullptr || nNodes >= sizePool)
		return -1;

	for(unsigned short i = 0; i < sizePool; i++) {
        if((nodePool + i)->flags & 0b00000010) { // Is finished, bit 1
            *(nodePool + i) = {
                .lastExecution = millis(),
                .interval = time,
                .flags = 0b00000001,
                .function = fun
            };
            nNodes++;
            return i;
        }
	}

	return -1; // No free slot
}

short Async::setTimeout(void (*fun)(void), unsigned long time) {
	if(fun == nullptr || nNodes >= sizePool)
		return -1;

	for(unsigned short i = 0; i < sizePool; i++) {
        if((nodePool + i)->flags & 0b00000010) { // Is finished, bit 1
            *(nodePool + i) = {
                .lastExecution = millis(),
                .interval = time,
                .flags = 0b00000000,
                .function = fun
            };
            nNodes++;
            return i;
        }
	}

	return -1; // No free slot
}

bool Async::clearInterval(short id) {
    if(id < 0 || (unsigned short) id >= sizePool)
        return false;

    if((nodePool + id)->flags & 0b00000010) // Already finished, nothing to release
        return true;

    (nodePool + id)->flags = 0b00000010; // Mark as finished, bit 1
    nNodes--; // Release the slot in the counter too, otherwise the pool is seen
              // as full after sizePool calls and every later set* returns -1
    return true;
}

void Async::run() {
	for(unsigned short i = 0; i < sizePool; i++) {

        if(!((nodePool + i)->flags & 0b00000010)) { // Is not finished
            // Unsigned arithmetic already wraps correctly, so the millis()
            // overflow at ~49.7 days needs no special case. Read the clock
            // once per node: the old code sampled it twice and could compare
            // one value but subtract another.
           	unsigned long elapsedTime = millis() - (nodePool + i)->lastExecution;

        	if(elapsedTime >= (nodePool + i)->interval) {
	            (nodePool + i)->function();
	            if(!((nodePool + i)->flags & 0b00000001)) { // Is not loop, bit 0
	                (nodePool + i)->flags = 0b00000010; // Mark as finished, bit 1
	                nNodes--;
	            } else {
	                (nodePool + i)->lastExecution = millis();
	            }
	        } // End elapsed time greater than interval of node
        } // End if node not finished

	} // End for
}

Async::~Async() {
	free(nodePool);
}
