#include <stdio.h>
#include <stdlib.h>
#include <emscripten/emscripten.h>

#ifdef __cplusplus
extern "C" {
#endif

int EMSCRIPTEN_KEEPALIVE setInoutBuffer(const unsigned char *buffer, long bufferSize );
int EMSCRIPTEN_KEEPALIVE setRomImage(const unsigned char *romImage, long romSize );
int EMSCRIPTEN_KEEPALIVE callStaticMain(char *className, char *param );

#ifdef __cplusplus
}
#endif

#include "utils.h"
#include "waba.h"
#include "waba_util.h"
#include "debuglog.h"
#include "mem_alloc.h"

#define DEFAULT_VM_STACK_SIZE		1000
#define DEFAULT_NM_STACK_SIZE		1000
#define DEFAULT_CLASS_HEAP_SIZE		20000
#define DEFAULT_OBJECT_HEAP_SIZE	76000
#define MEM_BLOCK_SIZE	(200*1024)
unsigned char MemArray[MEM_BLOCK_SIZE];

unsigned char *classRom_ext = NULL;
unsigned long classRomSize_ext = 0;

unsigned char *inoutBuff_ext = NULL;
unsigned long inoutBuffSize_ext = 0;

int EMSCRIPTEN_KEEPALIVE setInoutBuffer(const unsigned char *buffer, long bufferSize )
{
	FUNC_CALL();
	
	inoutBuff_ext = (unsigned char*)buffer;
	inoutBuffSize_ext = bufferSize;

	FUNC_RETURN();
	
	return FT_ERR_OK;
}

int EMSCRIPTEN_KEEPALIVE setRomImage(const unsigned char *romImage, long romSize )
{
	FUNC_CALL();
	
	classRom_ext = (unsigned char*)romImage;
	classRomSize_ext = romSize;

	FUNC_RETURN();
	
	return FT_ERR_OK;
}

int EMSCRIPTEN_KEEPALIVE callStaticMain(char *className, char *param )
{
	FUNC_CALL();

	debuglog("callStaticMain Called (className=%s)\n", className);

	long ret;

	ret = mem_initialize(MemArray, MEM_BLOCK_SIZE);
	if ( ret != FT_ERR_OK ){
		debuglog("mem_initialize error\n");
		return -1;
	}

	DEBUG_PRINT("MEM_BLOCK_SIZE = %d\n", MEM_BLOCK_SIZE);
	DEBUG_PRINT("DEFAULT_VM_STACK_SIZE = %d\n", DEFAULT_VM_STACK_SIZE);
	DEBUG_PRINT("DEFAULT_NM_STACK_SIZE = %d\n", DEFAULT_NM_STACK_SIZE);
	DEBUG_PRINT("DEFAULT_CLASS_HEAP_SIZE = %d\n", DEFAULT_CLASS_HEAP_SIZE);
	DEBUG_PRINT("DEFAULT_OBJECT_HEAP_SIZE = %d\n", DEFAULT_OBJECT_HEAP_SIZE);

	ret = VmInit(DEFAULT_VM_STACK_SIZE, DEFAULT_NM_STACK_SIZE, DEFAULT_CLASS_HEAP_SIZE, DEFAULT_OBJECT_HEAP_SIZE);
	if ( ret != FT_ERR_OK ){
		debuglog("VmInit error\n");
		mem_dispose();
		return -1;
	}

	Var retVar;
	unsigned char retType;

	debuglog("startStaticMain className=%s\n", className);
	ret = startStaticMain(className, param, &retType, &retVar);
	if( ret != FT_ERR_OK || retType != RET_TYPE_NONE){
		debuglog("return = %d\n", ret);
		debuglog("retType=%d\n", retType);
		debuglog("vmStatus.type=0x%x\n", vmStatus.type);
		debuglog("vmStatus.errNum=0x%x\n", vmStatus.errNum);
	}

	T_MEMINFO info;
	ret = getMemInfo(&info);
	if( ret == FT_ERR_OK ){
		debuglog("totalObjectMem = %d\n", info.totalObjectMem);
		debuglog("unusedObjectMem = %d\n", info.unusedObjectMem);
		debuglog("totalClassMem = %d\n", info.totalClassMem);
		debuglog("unusedClassMem = %d\n", info.unusedClassMem);
		debuglog("MEM_BLOCK_SIZE = %d\n", MEM_BLOCK_SIZE);
		debuglog("mem_get_used = %d\n", mem_get_used());
	}
	
	VmFree();
	
	mem_dispose();

	FUNC_RETURN();
	
	return ret;
}
