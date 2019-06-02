#include "waba.h"
#include "utils.h"
#include "waba_utf.h"
#include <string.h>

//
// UtfString Routines
//

// pointer to String class (for performance)
WClass *stringClass = NULL;

UtfString createUtfString(const char *buf) {
	UtfString s;

	if (buf == NULL) {
		s.str = NULL;
		s.len = 0;
	}else {
		s.str = (char*)buf;
		s.len = (unsigned short)strlen(buf);
	}
	return s;
}

UtfString getUtfString(WClass *wclass, unsigned short idx) {
	UtfString s;

	if (idx >= 1 && CONS_tag(wclass, idx) == CONSTANT_Utf8) {
		s.str = (char *)CONS_utfStr(wclass, idx);
		s.len = CONS_utfLen(wclass, idx);
	} else {
		s.str = "";
		s.len = 0;
	}
	return s;
}

WObject createStringFromUtf(UtfString s) {
	WObject obj, charArrayObj;
	unsigned short *charStart;
	unsigned long i;

	// create and fill char array
	charArrayObj = createArrayObject(TYPE_CHAR, s.len);
	if (charArrayObj == WOBJECT_NULL)
		return WOBJECT_NULL;
	if (pushObject(charArrayObj) != FT_ERR_OK)
		return WOBJECT_NULL;
	charStart = (unsigned short *)WOBJ_arrayStart(charArrayObj);
	for (i = 0; i < s.len; i++)
		charStart[i] =(unsigned short)s.str[i];
	// create String object and set char array
	obj = createObject(stringClass);
	if( obj == WOBJECT_NULL ){
		popObject();
		return WOBJECT_NULL;
	}
	WOBJ_StringCharArrayObj(obj) = charArrayObj;
	popObject(); // charArrayObj
	return obj;
}

#define STU_STATIC_SIZE		256
static unsigned char sbytes[STU_STATIC_SIZE];

unsigned char* UtfToStaticUChars(UtfString str) {
	unsigned short i;

	for( i = 0 ; i < str.len ; i++ )
		sbytes[i] = str.str[i];
	sbytes[i] = '\0';
	return sbytes;
}

// NOTE: Only set STU_USE_STATIC if the string is temporary and will not be
// needed before stringToUtf is called again. The flags parameter is a
// combination of the STU constants.
UtfString stringToUtf(WObject string, int flags) {
	UtfString s, e;
	WObject charArray;
	unsigned long i, extra;
	unsigned short *chars, len;
	unsigned char *bytes;
	int nullTerminate, useStatic;

	e.len = 0;
	e.str = NULL;
	if (string == WOBJECT_NULL)
		return e;
	charArray = WOBJ_StringCharArrayObj(string);
	if (charArray == WOBJECT_NULL)
		return e;
	len = (unsigned short)WOBJ_arrayLen(charArray);
	nullTerminate = flags & STU_NULL_TERMINATE;
	useStatic = flags & STU_USE_STATIC;
	extra = 0;
	if (nullTerminate)
		extra = 1;
	if (useStatic && (len + extra) <= STU_STATIC_SIZE)
		bytes = sbytes;
	else {
		WObject byteArray;

		byteArray = createArrayObject(TYPE_BYTE, len + extra);
		if (byteArray == WOBJECT_NULL)
			return e;
		bytes = (unsigned char *)WOBJ_arrayStart(byteArray);
	}
	chars = (unsigned short *)WOBJ_arrayStart(charArray);
	for (i = 0; i < len; i++)
		bytes[i] = (unsigned char)chars[i];
	if (nullTerminate)
		bytes[i] = '\0';
	s.str = (char *)bytes;
	s.len = len;
	return s;
}

WObject createString(const char *buf) {
	return createStringFromUtf(createUtfString(buf));
}
