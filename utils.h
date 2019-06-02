#ifndef _UTILS_H_
#define _UTILS_H_

#ifndef NULL
#define NULL	0
#endif

#define FT_ERR_OK					0
#define FT_ERR_UNKNOWN				-1
#define FT_ERR_INVALID_PARAM		-2
#define FT_ERR_INVALID_RESPONSE		-3
#define FT_ERR_TIMEOUT				-4
#define FT_ERR_FAILED				-5
#define FT_ERR_NOTENOUGH			-6
#define FT_ERR_NOTFOUND				-7
#define FT_ERR_NOTSUPPORTED			-8
#define FT_ERR_INVALID_STATUS		-9

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

void utils_ltoa(long value, char *ptr, unsigned short radix);

unsigned short utils_get_uint16b( const unsigned char *p_bin );
unsigned short utils_get_uint16l( const unsigned char *p_bin );
short utils_get_int16b( const unsigned char *p_bin );
short utils_get_int16l( const unsigned char *p_bin );
unsigned long utils_get_uint32b( const unsigned char *p_bin );
unsigned long utils_get_uint32l( const unsigned char *p_bin );
long utils_get_int32b( const unsigned char *p_bin );
long utils_get_int32l( const unsigned char *p_bin );
void utils_set_uint16b( unsigned char *p_bin, unsigned short value );
void utils_set_uint16l( unsigned char *p_bin, unsigned short value );
void utils_set_int16b( unsigned char *p_bin, short value );
void utils_set_int16l( unsigned char *p_bin, short value );
void utils_set_uint32b( unsigned char *p_bin, unsigned long value );
void utils_set_uint32l( unsigned char *p_bin, unsigned long value );
void utils_set_int32b( unsigned char *p_bin, long value );
void utils_set_int32l( unsigned char *p_bin, long value );

#ifdef __cplusplus
}
#endif // __cplusplus


#endif
