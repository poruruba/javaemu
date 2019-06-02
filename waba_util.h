#ifndef _WABA_UTIL_H_
#define _WABA_UTIL_H_

#include "waba.h"

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

long callStaticMethod(WClass* wclass, UtfString name, UtfString desc, Var params[], unsigned short numParams, unsigned char *retType, Var* retVar);
long startStaticMain(const char *className, const char *param, unsigned char *retType, Var* retVar);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif
