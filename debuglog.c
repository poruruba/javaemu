#include "debuglog.h"
#include <stdio.h>
#include <stdarg.h>
#include "utils.h"

long debuglog(const char *p_format, ...)
{
	va_list args;

	va_start( args, p_format );
	vprintf(p_format, args);
	va_end( args );

	return FT_ERR_OK;
}

#if 0
long syslog( const char *p_format, ... )
{
	va_list args;
	FILE *fp;

	fp = fopen( "c:\\dump.log", "a+" );
	if( fp == NULL )
		return -1;

	va_start( args, p_format );
	vfprintf( fp, p_format, args );
	va_end( args );

	fclose( fp );

	return 0;
}
#endif

void debuglog_dump( const char *p_message, const unsigned char *p_bin, unsigned long len )
{
  unsigned long i;
  
  debuglog( "%s", p_message );
  for( i = 0 ; i < len ; i++ )
	  debuglog( "%02x", p_bin[i] );
  debuglog( "\n" );
}

void debuglog_row( const char *p_header, const unsigned char *p_bin, unsigned long len )
{
	unsigned long i;
	
	debuglog( "%s\n", p_header );
	debuglog( "          " );
	for( i = 0 ; i < 16 ; i++ )
	{
		if( ( i % 8 ) == 0 )
			debuglog( " " );
		if( ( i % 4 ) == 0 )
			debuglog( "%2d", i );
		else
			debuglog( " ." );
	}
	
	for( i = 0 ; i < len ; i++ )
	{
		if( ( i % 16 ) == 0 )
			debuglog( "\n%08X :", i );
		if( ( i % 8 ) == 0 )
			debuglog( " " );
		debuglog( "%02X", p_bin[i] );
	}
	debuglog( "\n" );
}