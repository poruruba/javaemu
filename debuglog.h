#ifndef _DEBUGLOG_H_
#define _DEBUGLOG_H_

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

long debuglog(const char *format, ...);

void debuglog_dump(const char *p_message, const unsigned char *p_bin, unsigned long len);
void debuglog_row(const char *p_header, const unsigned char *p_bin, unsigned long len);

#if 1
#define FUNC_CALL()		debuglog( "[FUNC_CALL  ] %s\n", __FUNCTION__ )
#define FUNC_RETURN()	debuglog( "[FUNC_RETURN] %s\n", __FUNCTION__ )
#define FUNC_ERROR(rc)	debuglog( "[FUNC_ERROR ] %s line(%d) = 0x%08x\n\r", __FUNCTION__, __LINE__, rc );

#define DEBUG_PRINT( ... )	debuglog( __VA_ARGS__ )
#define DEBUG_DUMP( ... )	debuglog_dump( __VA_ARGS__ )
#define DEBUG_FUNC_START()	debuglog( "FUNC CALL   : %s\n\r", __FUNCTION__ )
#define DEBUG_FUNC_END()	debuglog( "FUNC RETURN : %s\n\r", __FUNCTION__ )
#define DEBUG_FUNC_ERROR( rc )	debuglog( "ERROR: In %s at Line %d = 0x%08x\n\r", __FUNCTION__, __LINE__, rc );
#else
#define FUNC_CALL()		{}
#define FUNC_RETURN()	{}
#define FUNC_ERROR(rc)	{}

#define DEBUG_PRINT( ... )	{}
#define DEBUG_DUMP( ... )	{}
#define DEBUG_FUNC_START()	{}
#define DEBUG_FUNC_END()		{}
#define DEBUG_FUNC_ERROR( rc )	{}
#endif

#ifdef __cplusplus
}
#endif // __cplusplus

#endif
