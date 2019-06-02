#include "utils.h"

static void byte_swap(char *ptr, int size)
{
	int i;
	char temp;

	for (i = 0; i < size / 2; i++) {
		temp = ptr[i];
		ptr[i] = ptr[size - i - 1];
		ptr[size - i - 1] = temp;
	}
}

void utils_ltoa(long value, char *ptr, unsigned short radix)
{
	int i;
	unsigned char minus;

	if (value < 0)
		minus = 1;
	else
		minus = 0;

	if (value == 0) {
		ptr[0] = '0';
		ptr[1] = '\0';
	}

	for (i = 0; value != 0; i++) {
		if ((value % radix) <= 9) {
			ptr[i] = (char)('0' + (value % radix));
		}
		else {
			ptr[i] = (char)('a' + (value % radix) - 10);
		}
		value /= radix;
	}
	if (minus != 0)
		ptr[i++] = '-';

	byte_swap(ptr, i);
	ptr[i] = '\0';
}

unsigned short utils_get_uint16b( const unsigned char *p_bin )
{
	return (unsigned short)( ( ( ( (unsigned short)p_bin[0] ) << 8 ) | (unsigned short)p_bin[1] ) );
}

unsigned short utils_get_uint16l( const unsigned char *p_bin )
{
	return (unsigned short)( ( ( ( (unsigned short)p_bin[1] ) << 8 ) | (unsigned short)p_bin[0] ) );
}

short utils_get_int16b( const unsigned char *p_bin )
{
	return (short)( ( ( ( (short)p_bin[0] ) << 8 ) | (short)p_bin[1] ) );
}

short utils_get_int16l( const unsigned char *p_bin )
{
	return (short)( ( ( ( (short)p_bin[1] ) << 8 ) | (short)p_bin[0] ) );
}

unsigned long utils_get_uint32b( const unsigned char *p_bin )
{
	return ( ( ( (unsigned long)p_bin[0] ) << 24 ) | ( ( (unsigned long)p_bin[1] ) << 16 ) | ( ( (unsigned long)p_bin[2] ) << 8 ) | (unsigned long)p_bin[3] );
}

unsigned long utils_get_uint32l( const unsigned char *p_bin )
{
	return ( ( ( (unsigned long)p_bin[3] ) << 24 ) | ( ( (unsigned long)p_bin[2] ) << 16 ) | ( ( (unsigned long)p_bin[1] ) << 8 ) | (unsigned long)p_bin[0] );
}

long utils_get_int32b( const unsigned char *p_bin )
{
	return ( ( ( (long)p_bin[0] ) << 24 ) | ( ( (long)p_bin[1] ) << 16 ) | ( ( (long)p_bin[2] ) << 8 ) | (long)p_bin[3] );
}

long utils_get_int32l( const unsigned char *p_bin )
{
	return ( ( ( (long)p_bin[3] ) << 24 ) | ( ( (long)p_bin[2] ) << 16 ) | ( ( (long)p_bin[1] ) << 8 ) | (long)p_bin[0] );
}

void utils_set_uint16b( unsigned char *p_bin, unsigned short value )
{
	p_bin[0] = (unsigned char)( ( value >> 8 ) & 0xff );
	p_bin[1] = (unsigned char)( value & 0xff );
}

void utils_set_uint16l( unsigned char *p_bin, unsigned short value )
{
	p_bin[0] = (unsigned char)( value & 0xff );
	p_bin[1] = (unsigned char)( ( value >> 8 ) & 0xff );
}

void utils_set_int16b( unsigned char *p_bin, short value )
{
	p_bin[0] = (unsigned char)( ( value >> 8 ) & 0xff );
	p_bin[1] = (unsigned char)( value & 0xff );
}

void utils_set_int16l( unsigned char *p_bin, short value )
{
	p_bin[0] = (unsigned char)( value & 0xff );
	p_bin[1] = (unsigned char)( ( value >> 8 ) & 0xff );
}

void utils_set_uint32b( unsigned char *p_bin, unsigned long value )
{
	p_bin[0] = (unsigned char)( ( value >> 24 ) & 0xff );
	p_bin[1] = (unsigned char)( ( value >> 16 ) & 0xff );
	p_bin[2] = (unsigned char)( ( value >> 8 ) & 0xff );
	p_bin[3] = (unsigned char)( value & 0xff );
}

void utils_set_uint32l( unsigned char *p_bin, unsigned long value )
{
	p_bin[0] = (unsigned char)( value & 0xff );
	p_bin[1] = (unsigned char)( ( value >> 8 ) & 0xff );
	p_bin[2] = (unsigned char)( ( value >> 16 ) & 0xff );
	p_bin[3] = (unsigned char)( ( value >> 24 ) & 0xff );
}

void utils_set_int32b( unsigned char *p_bin, long value )
{
	p_bin[0] = (unsigned char)( ( value >> 24 ) & 0xff );
	p_bin[1] = (unsigned char)( ( value >> 16 ) & 0xff );
	p_bin[2] = (unsigned char)( ( value >> 8 ) & 0xff );
	p_bin[3] = (unsigned char)( value & 0xff );
}

void utils_set_int32l( unsigned char *p_bin, long value )
{
	p_bin[0] = (unsigned char)( value & 0xff );
	p_bin[1] = (unsigned char)( ( value >> 8 ) & 0xff );
	p_bin[2] = (unsigned char)( ( value >> 16 ) & 0xff );
	p_bin[3] = (unsigned char)( ( value >> 24 ) & 0xff );
}
