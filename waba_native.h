#ifndef _WABA_NATIVE_H_
#define _WABA_NATIVE_H_

#include "waba.h"

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

extern NativeMethod nativeMethods[];
extern int NumberOfNativeMethods;

unsigned char *nativeLoadClass( UtfString className );

#ifdef __cplusplus
}
#endif // __cplusplus

#endif

