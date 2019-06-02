#include "waba.h"
#include "utils.h"
#include "mem_alloc.h"
#include "waba_heap.h"
#include <string.h>

//
// Memory Management
//

// Here's the garbage collector. I implemented the mark and sweep below
// after testing out a few different ones and reading:
//
// Garbage Collection, Algorithms for Automatic Dynamic Memory Management
// by Richard Jones and Rafael Lins
//
// which is an excellent book. Also, this collector has gone through a
// lot of testing. It runs when the system is completely out of memory
// which can happen at any time.. for example during class loading.
//
// To test it out, tests were run where 1000's of random objects were
// loaded, constructed and random methods called on them over some
// period of days. This found a couple subtle bugs that were
// fixed like when the garbage collector ran in the middle of array
// allocation and moved pointers around from under the array allocator
// code (those have all been fixed).
//
// The heap is comprised of Hos objects (an array) that grows from
// the "right" of object memory and objects that take up the space on
// on the "left" side. The Hos array keeps track of where the objects
// are on the left.
//
// The Hos structure (strange, but aptly named) is used to keep
// track of handles (pointers to memory locations), order (order
// of handles with respect to memory) and temporary items (used
// during the scan phase).
//
// The 3 items in the Hos structure do not relate to each other. They
// are each a part of 3 conceptually distinct arrays that grow
// from the right of the heap while the objects grow from the left.
// So, when the Hos array is indexed, it is always negative (first
// element is 0, next is -1, next is -2, etc).

// NOTE: The total amount of memory used up at any given
// time in the heap is: objectSize + (numHandles * sizeof(Hos))
typedef struct {
	Hos *hos; // handle, order and scan arrays (interlaced)
	unsigned long numHandles;
	unsigned long numFreeHandles;
	unsigned char *mem;
	unsigned long memSize; // total size of memory (including free)
	unsigned long objectSize; // size of all objects in heap
} ObjectHeap;

static ObjectHeap heap = { 0 };

// NOTE: this method is only for printing the status of memory
// and can be removed. Also note, there is no such thing as
// the "amount of free memory" because of garbage collection.
unsigned long getUnusedMemSize(void) {
	return heap.memSize - (heap.objectSize + (heap.numHandles * sizeof(Hos)));
}

unsigned long getTotalMemSize(void)
{
	return heap.memSize;
}

unsigned long getNumHandles(void)
{
	return heap.numHandles;
}

int initObjectHeap(unsigned long heapSize) {

	if (heap.mem != NULL)
		return FT_ERR_INVALID_STATUS;

	heap.numHandles = 0;
	heap.numFreeHandles = 0;
	heap.memSize = heapSize;

	// align to 4 byte boundry for correct alignment of the Hos array
	heap.memSize = (heap.memSize + 3) & ~3;

	// allocate and zero out memory region
	heap.mem = (unsigned char *)mem_alloc(heap.memSize);
	if (heap.mem == NULL)
		return FT_ERR_NOTENOUGH;
	memset(heap.mem, 0x00, heap.memSize);
	heap.hos = (Hos *)(&heap.mem[heap.memSize - sizeof(Hos)]);
	heap.objectSize = 0;

	return FT_ERR_OK;
}

void freeObjectHeap(void) {
	WObject obj;
	unsigned long h;
	WClass *wclass;

	if (heap.mem == NULL)
		return;

	// call any native object destroy methods to free system resources
	for (h = 0; h < heap.numHandles; h++) {
		obj = h + FIRST_OBJ + 1;
		if (objectPtr(obj) != NULL) {
			wclass = WOBJ_class(obj);
			if (wclass != NULL && wclass->objDestroyFunc)
				wclass->objDestroyFunc(obj);
		}
	}

	heap.numHandles = 0;
	heap.numFreeHandles = 0;
	heap.memSize = 0;

	mem_free(heap.mem);
	heap.mem = NULL;
}

// NOTE: size passed must be 4 byte aligned (see arraySize())
WObject allocObject(long size) {
	unsigned long i, sizeReq, hosSize;

	if (size <= 0) {
		VmSetFatalErrorNum(ERR_ParamError);
		return WOBJECT_NULL;
	}
	sizeReq = size;
	if (heap.numFreeHandles == 0)
		sizeReq += sizeof(Hos);
	hosSize = heap.numHandles * sizeof(Hos);

	if (sizeReq + hosSize + heap.objectSize > heap.memSize) {
		gc();
		// heap.objectSize changed or we are out of memory
		if (sizeReq + hosSize + heap.objectSize > heap.memSize) {
			VmSetFatalErrorNum(ERR_OutOfObjectMem);
			return WOBJECT_NULL;
		}
	}
	if (heap.numFreeHandles) {
		i = heap.hos[-(long)(heap.numHandles - heap.numFreeHandles)].order;
		heap.numFreeHandles--;
	}
	else {
		// no free handles, get a new one
		i = heap.numHandles;
		heap.hos[-(long)i].order = i;
		heap.numHandles++;
	}

	heap.hos[-(long)i].ptr = (Var *)&heap.mem[heap.objectSize];
	heap.objectSize += size;

	return FIRST_OBJ + i + 1;
}

// NOTE: we made this function a #define and it showed no real performance
// gain over having it a function on either PalmOS or Windows when
// optimization was turned on.
Var *objectPtr(WObject obj) {
	if (obj == WOBJECT_NULL)
		return NULL;
	return heap.hos[-(long)(obj - FIRST_OBJ - 1)].ptr;
}

// mark bits in the handle order array since it is not used during
// the mark object process (its used in the sweep phase)
#define MARK(o) (heap.hos[- (long)(o - FIRST_OBJ - 1)].order |= 0x80000000L)
#define IS_MARKED(o) (heap.hos[- (long)(o - FIRST_OBJ - 1)].order & 0x80000000L)

// mark this object and all the objects this object refers to and all
// objects those objects refer to, etc.
void markObject(WObject obj) {
	WClass *wclass;
	WObject *arrayStart, o;
	unsigned long i, len, numScan;
	unsigned char type;

	if (obj == WOBJECT_NULL)
		return;
	if (!VALID_OBJ(obj) || objectPtr(obj) == NULL || IS_MARKED(obj))
		return;
	MARK(obj);
	numScan = 0;

markinterior:
	wclass = WOBJ_class(obj);
	if (wclass == NULL) {
		// array - see if it contains object references
		type = WOBJ_arrayType(obj);
		if (type == TYPE_OBJECT || type == TYPE_ARRAY) {
			// for an array of arrays or object array
			arrayStart = (WObject *)WOBJ_arrayStart(obj);
			len = WOBJ_arrayLen(obj);
			for (i = 0; i < len; i++) {
				o = arrayStart[i];
				if (VALID_OBJ(o) && objectPtr(o) != NULL && !IS_MARKED(o)) {
					MARK(o);
					heap.hos[-(long)numScan].temp = o;
					numScan++;
				}
			}
		}
	}
	else {
		// object
		len = wclass->numVars;
		for (i = 0; i < len; i++) {
			o = WOBJ_var(obj, i).obj;
			if (VALID_OBJ(o) && objectPtr(o) != NULL && !IS_MARKED(o)) {
				MARK(o);
				heap.hos[-(long)numScan].temp = o;
				numScan++;
			}
		}
	}
	if (numScan > 0) {
		// Note: we use goto since we want to avoid recursion here
		// since structures like linked links could create deep
		// stack calls
		--numScan;
		obj = heap.hos[-(long)numScan].temp;
		goto markinterior;
	}
}

// NOTE: There are no waba methods that are called when objects are destroyed.
// This is because if a method was called, the object would be on its way to
// being GC'd and if we set another object (or static field) to reference it,
// after the GC, the reference would be stale.
void sweepObjects(void) {
	WObject obj;
	WClass *wclass;
	unsigned long i, h, objSize, prevObjectSize, numUsedHandles;
	unsigned char *src, *dst;

	prevObjectSize = heap.objectSize;
	heap.objectSize = 0;

	// move all the marks over into the scan array so we don't have
	// to do lots of bit shifting
	for (i = 0; i < heap.numHandles; i++) {
		if (heap.hos[-(long)i].order & 0x80000000L) {
			heap.hos[-(long)i].order &= 0x7FFFFFFFL; // clear mark bit
			heap.hos[-(long)i].temp = 1;
		}
		else {
			heap.hos[-(long)i].temp = 0;
		}
	}
	numUsedHandles = 0;
	for (i = 0; i < heap.numHandles; i++) {
		// we need to scan in memory order so we can compact things without
		// copying objects over each other
		h = heap.hos[-(long)i].order;
		obj = h + FIRST_OBJ + 1;
		if (heap.hos[-(long)h].temp == 0) {
			// handle is free - dereference object
			if (objectPtr(obj) != NULL) {
				wclass = WOBJ_class(obj);
				// for non-arrays, call objDestroy if present
				if (wclass != NULL && wclass->objDestroyFunc)
					wclass->objDestroyFunc(obj);
				heap.hos[-(long)h].ptr = NULL;
			}
			continue;
		}
		wclass = WOBJ_class(obj);
		if (wclass == NULL)
			objSize = arraySize(WOBJ_arrayType(obj), WOBJ_arrayLen(obj));
		else
			objSize = WCLASS_objectSize(wclass);

		// copy object to new heap
		src = (unsigned char *)heap.hos[-(long)h].ptr;
		dst = &heap.mem[heap.objectSize];
		if (src != dst)
			// NOTE: overlapping regions need to copy correctly
			memmove(dst, src, objSize);
		heap.hos[-(long)h].ptr = (Var *)dst;
		heap.hos[-(long)numUsedHandles].order = h;
		heap.objectSize += objSize;
		numUsedHandles++;
	}
	heap.numFreeHandles = heap.numHandles - numUsedHandles;
	for (i = 0; i < heap.numHandles; i++)
		if (!heap.hos[-(long)i].temp) {
			// add free handle to free section of order array
			heap.hos[-(long)numUsedHandles].order = i;
			numUsedHandles++;
		}
	// zero out the part of the heap that is now junk
	memset(&heap.mem[heap.objectSize], 0x00, prevObjectSize - heap.objectSize);
}
