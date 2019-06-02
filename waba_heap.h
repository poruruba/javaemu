#ifndef _WABA_HEAP_H_
#define _WABA_HEAP_H_

#include "waba.h"

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

unsigned long getUnusedMemSize(void);
unsigned long getTotalMemSize(void);
unsigned long getNumHandles(void);

int initObjectHeap(unsigned long heapSize);
void freeObjectHeap(void);
WObject allocObject(long size);
Var *objectPtr(WObject obj);

void markObject(WObject obj);
void sweepObjects(void);

#define FIRST_OBJ 2244
#define VALID_OBJ(o) (o > FIRST_OBJ && o <= FIRST_OBJ + getNumHandles() )

#ifdef __cplusplus
}
#endif // __cplusplus

#endif
