#include <stdio.h>
#include <stdlib.h>
#include <emscripten/emscripten.h>

#ifdef __cplusplus
extern "C" {
#endif

int EMSCRIPTEN_KEEPALIVE setInoutBuffer(const unsigned char *buffer, long bufferSize );
int EMSCRIPTEN_KEEPALIVE setRomImage(unsigned char *romImage, long romSize );
int EMSCRIPTEN_KEEPALIVE callStaticMain(char *className, char *param );

#ifdef __cplusplus
}
#endif

#include <string.h>
#include "utils.h"
#include "waba.h"
#include "waba_utf.h"
#include "debuglog.h"
#include "waba_util.h"
#include "mem_alloc.h"
#include "alloc_class.h"

#define DEFAULT_VM_STACK_SIZE		1000
#define DEFAULT_NM_STACK_SIZE		1000
#define DEFAULT_CLASS_HEAP_SIZE		20000
#define DEFAULT_OBJECT_HEAP_SIZE	76000
#define MEM_BLOCK_SIZE	(200*1024)

unsigned char MemArray[MEM_BLOCK_SIZE];

unsigned char *classRom_ext;
unsigned long classRomSize_ext = 0;

unsigned char *inoutBuff_ext;
unsigned long inoutBuffSize_ext = 0;

int EMSCRIPTEN_KEEPALIVE setInoutBuffer(const unsigned char *buffer, long bufferSize )
{
	FUNC_CALL();
	
	inoutBuff_ext = (unsigned char*)buffer;
	inoutBuffSize_ext = bufferSize;

	FUNC_RETURN();
	
	return FT_ERR_OK;
}

int EMSCRIPTEN_KEEPALIVE setRomImage(unsigned char *romImage, long romSize )
{
	FUNC_CALL();
	
	classRomSize_ext = romSize;
	classRom_ext = romImage;
	

	FUNC_RETURN();
	
	return FT_ERR_OK;
}

int EMSCRIPTEN_KEEPALIVE callStaticMain(char *className, char *param )
{
	FUNC_CALL();

	unsigned long i;
	debuglog("callStaticMain Called\n");

	debuglog("className=%s\n", className);

	long ret;
	Var retVar;
	unsigned char retType;

	mem_initialize(MemArray, MEM_BLOCK_SIZE);

	ret = VmInit(DEFAULT_VM_STACK_SIZE, DEFAULT_NM_STACK_SIZE, DEFAULT_CLASS_HEAP_SIZE, DEFAULT_OBJECT_HEAP_SIZE);
	if ( ret != FT_ERR_OK ){
		DEBUG_PRINT("VmInit error\n");
		mem_dispose();
		return -1;
	}

	debuglog("startStaticMain className=%s\n", className);
	ret = startStaticMain(className, param, &retType, &retVar);
	if( ret != FT_ERR_OK ){
		debuglog("return = %d\n", ret);
		debuglog("retType=%d\n", retType);
		debuglog("vmStatus.type=0x%x\n", vmStatus.type);
		debuglog("vmStatus.errNum=0x%x\n", vmStatus.errNum);
	}

	VmFree();
	mem_dispose();
	
	FUNC_RETURN();

	return ret;
}
