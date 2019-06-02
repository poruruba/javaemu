#ifndef _WABA_UTF_H_
#define _WABA_UTF_H_

#include "waba.h"

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

// flags for stringToUtf()
#define STU_NULL_TERMINATE 1
#define STU_USE_STATIC     2

extern WClass *stringClass;

UtfString createUtfString(const char *buf);
UtfString getUtfString(WClass *wclass, unsigned short idx);
WObject createStringFromUtf(UtfString s);
UtfString stringToUtf(WObject string, int flags);
WObject createString(const char *buf);
unsigned char* UtfToStaticUChars(UtfString str);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif

