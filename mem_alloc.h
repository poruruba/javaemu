#ifndef _ALLOC_MEM_H_
#define _ALLOC_MEM_H_

#define DEFAULT_MEM_ID	0x0000

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

long mem_get_used();
long mem_initialize( unsigned char *mem_block, unsigned long mem_size );
void *mem_alloc2( unsigned long size, unsigned short mem_id );
#define mem_alloc( size )	mem_alloc2( size, DEFAULT_MEM_ID )
void mem_free( const void *ptr );
void mem_dispose();

#ifdef __cplusplus
}
#endif // __cplusplus

#endif

