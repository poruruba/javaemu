#include "utils.h"
#include "mem_alloc.h"

static unsigned char initialized = 0;
static unsigned char *MemBlock = NULL;
static unsigned long MemSize = 0;

typedef struct {
	unsigned short prev;
	unsigned short next;
	unsigned char used;
	unsigned char dummy;
	unsigned short id;
} HandleInfo; /* sizeof(HandleInfo) must equal 8 */

#define FLG_UNUSED		0x00
#define FLG_USED		0x01

#define MEM_GET_HANDLE(ptr)	(*((HandleInfo*)ptr))
#define MEM_SET_HANDLE(ptr,info)	( (* ((HandleInfo*)(ptr)) ) = (info) )

long mem_get_used(void)
{
	HandleInfo *ptr = (HandleInfo*)MemBlock;
	HandleInfo info;
	unsigned long used = 0;

	if( initialized == 0 )
		return -1;

	info = MEM_GET_HANDLE( ptr );
	while( info.next != (unsigned short)~0 ){
		if( info.used == FLG_USED )
			used += info.next;
		ptr += 1 + info.next;
		info = MEM_GET_HANDLE( ptr );
	}
	
	return used << 3;
}

long mem_initialize( unsigned char *mem_block, unsigned long mem_size )
{
	HandleInfo info;

	if( initialized != 0 )
		return -1;

	if( mem_size > ( 0x0000ffff << 3 ) )
		return -2;

	MemBlock = mem_block;
	MemSize = mem_size;

	info.prev = (unsigned short)~0;
	info.next = (unsigned short)( MemSize / 8 - 2 );
	info.used = FLG_UNUSED;
	info.id = DEFAULT_MEM_ID;
	MEM_SET_HANDLE( (HandleInfo*)MemBlock, info );
	info.prev = info.next;
	info.next = (unsigned short)~0;
	info.used = FLG_USED;
	info.id = DEFAULT_MEM_ID;
	MEM_SET_HANDLE( (HandleInfo*)MemBlock + MemSize / 8 - 1, info );
	initialized = 1;
	
	return 0;
}

void *mem_alloc2( unsigned long size, unsigned short mem_id )
{
	unsigned char *ret = NULL;
	HandleInfo *ptr;
	HandleInfo info, tInfo;
	unsigned short temp;

	if( initialized == 0 )
		return NULL;

	if( size > ( 0x0000ffff << 3 ) )
		return NULL;

	size = ( size + 7 ) >> 3;
	ptr = (HandleInfo*)MemBlock;
	info = MEM_GET_HANDLE( ptr );
	do{
		if( info.used == FLG_UNUSED && info.next >= size )
		{
			ret = (unsigned char*)( ptr + 1 );
			if( info.next == size || info.next == ( size + 1 ))
			{
				info.used = FLG_USED;
				info.id = mem_id;
				MEM_SET_HANDLE( ptr, info );
				break;
			}
			temp = (unsigned short)( info.next - ( 1 + size ) );
			info.next = (unsigned short)size;
			info.used = FLG_USED;
			MEM_SET_HANDLE( ptr, info );
			ptr += 1 + size;
			tInfo.next = temp;
			tInfo.prev = (unsigned short)size;
			tInfo.used = FLG_UNUSED;
			tInfo.id = mem_id;
			MEM_SET_HANDLE( ptr, tInfo );
			ptr += 1 + temp;
			tInfo = MEM_GET_HANDLE( ptr );
			tInfo.prev = temp;
			MEM_SET_HANDLE( ptr, tInfo );
			break;
		}else{
			ptr += 1 + info.next;
		}
		info = MEM_GET_HANDLE( ptr );
	}while( info.next != (unsigned short)~0 );

	return ret;
}

void mem_free( const void *thePtr )
{
	HandleInfo info, tInfo;
	unsigned short temp;
	HandleInfo *ptr;

	if( initialized == 0 )
		return;

	if( thePtr == NULL )
		return;
	ptr = (HandleInfo*)thePtr;
	ptr -= 1;
	info = MEM_GET_HANDLE( ptr );
	info.used = FLG_UNUSED;
	MEM_SET_HANDLE( ptr, info );
	if( info.prev != (unsigned short)~0 )
	{
		ptr -= info.prev + 1;
		tInfo = MEM_GET_HANDLE( ptr );
		if( tInfo.used == FLG_UNUSED )
		{
			temp = (unsigned short)( tInfo.next + 1 + info.next );
			tInfo.next = temp;
			MEM_SET_HANDLE( ptr, tInfo );
			ptr += 1 + tInfo.next;
			tInfo = MEM_GET_HANDLE( ptr );
			tInfo.prev = temp;
			MEM_SET_HANDLE( ptr, tInfo );
		}else{
			ptr += 1 + tInfo.next;
			ptr += 1 + info.next;
		}
	}
	tInfo = MEM_GET_HANDLE( ptr );
	if( tInfo.used == FLG_UNUSED )
	{
		temp = (unsigned short)( tInfo.prev + 1 + tInfo.next );
		ptr += 1 + tInfo.next;
		tInfo = MEM_GET_HANDLE( ptr );
		tInfo.prev = temp;
		MEM_SET_HANDLE( ptr, tInfo );
		ptr -= temp + 1;
		tInfo = MEM_GET_HANDLE( ptr );
		tInfo.next = temp;
		MEM_SET_HANDLE( ptr, tInfo );
	}
}

void mem_dispose(void)
{
	if( initialized != 0 ){
		MemBlock = NULL;
		MemSize = 0;
		initialized = 0;
	}
}

