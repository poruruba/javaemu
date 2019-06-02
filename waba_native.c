#include "waba.h"
#include "waba_utf.h"
#include "utils.h"
#include "waba_native.h"
#include "alloc_class.h"
#include "debuglog.h"
#include "waba_util.h"
#include <string.h>
#include <stdlib.h>

#ifdef WIN32
#include <Windows.h>
#endif

extern unsigned char *inoutBuff_ext;
extern unsigned long inoutBuffSize_ext;

// base/framework/System_gc_()V
long FCSystem_gc(Var stack[]){
	gc();
	return 0;
}

// base/framework/Util_byteArrayCopy_([BI[BII)V
long Util_byteArrayCopy(Var stack[]){
	WObject byteArray;
	unsigned char *src, *dest;
	int len;

	if( stack[0].obj == WOBJECT_NULL || stack[2].obj == WOBJECT_NULL )
		return ERR_NullObjectAccess;

	len = stack[4].intValue;
	byteArray = stack[0].obj;
	if( arrayRangeCheck( byteArray, stack[1].intValue, len ) == 0 )
		return ERR_IndexOutOfRange;

	src = (unsigned char *)WOBJ_arrayStart(byteArray);
	src += stack[1].intValue;

	byteArray = stack[2].obj;
	if( arrayRangeCheck( byteArray, stack[3].intValue, len ) == 0 )
		return ERR_IndexOutOfRange;

	dest = (unsigned char *)WOBJ_arrayStart(byteArray);
	dest += stack[3].intValue;

	memmove( dest, src, len );

	return 0;
}

// base/framework/Util_byteArrayFill_([BIIB)V
long Util_byteArrayFill(Var stack[]){
	WObject byteArray;
	unsigned char *bytes;
	int len;
	unsigned char value;

	if( stack[0].obj == WOBJECT_NULL )
		return ERR_NullObjectAccess;

	byteArray = stack[0].obj;
	len = stack[2].intValue;
	if( arrayRangeCheck( byteArray, stack[1].intValue, len ) == 0 )
		return ERR_IndexOutOfRange;

	bytes = (unsigned char *)WOBJ_arrayStart(byteArray);
	bytes += stack[1].intValue;
	value = (unsigned char)( stack[3].intValue );
	
	memset( bytes, value, len );

	return 0;
}

// base/framework/Util_byteArrayCompare_([BI[BII)I
long Util_byteArrayCompare(Var stack[]){
	Var v;
	WObject byteArray;
	unsigned char *src, *dest;
	int len;

	v.intValue = 0;
	if( stack[0].obj == WOBJECT_NULL || stack[2].obj == WOBJECT_NULL )
		return ERR_NullObjectAccess;

	byteArray = stack[0].obj;
	len = stack[4].intValue;
	if( arrayRangeCheck( byteArray, stack[1].intValue, len ) == 0 )
		return ERR_IndexOutOfRange;

	src = (unsigned char *)WOBJ_arrayStart(byteArray);
	src += stack[1].intValue;

	byteArray = stack[2].obj;
	if( arrayRangeCheck( byteArray, stack[3].intValue, len ) == 0 )
		return ERR_IndexOutOfRange;

	dest = (unsigned char *)WOBJ_arrayStart(byteArray);
	dest += stack[3].intValue;

	v.intValue = memcmp( dest, src, len );
	stack[0] = v;

	return 0;
}

// base/framework/System_arraycopy_(Ljava/lang/Object;ILjava/lang/Object;II)V
long FCSystem_arrayCopy(Var stack[]){
	WObject srcArray, dstArray;
	long srcStart, dstStart, len, typeSize;
	unsigned char *srcPtr, *dstPtr, srcType;

	srcArray = stack[0].obj;
	srcStart = stack[1].intValue;
	dstArray = stack[2].obj;
	dstStart = stack[3].intValue;
	len = stack[4].intValue;
	if (srcArray == WOBJECT_NULL || dstArray == WOBJECT_NULL)
		return ERR_NullObjectAccess;

	// ensure both src and dst are arrays
	if (WOBJ_class(srcArray) != NULL || WOBJ_class(dstArray) != NULL)
		return ERR_ArrayStoreException;

	// NOTE: This is not a full check to see if the two arrays are compatible.
	// Any two arrays of objects are compatible according to this check
	// see also: compatibleArray()
	srcType = WOBJ_arrayType(srcArray);
	if (srcType != WOBJ_arrayType(dstArray))
		return ERR_ArrayStoreException;

	// check ranges
	if (arrayRangeCheck(srcArray, srcStart, len) == 0 ||
		arrayRangeCheck(dstArray, dstStart, len) == 0) 
		return ERR_IndexOutOfRange;
	typeSize = arrayTypeSize(srcType);
	srcPtr = (unsigned char *)WOBJ_arrayStart(srcArray) + (typeSize * srcStart);
	dstPtr = (unsigned char *)WOBJ_arrayStart(dstArray) + (typeSize * dstStart);
	memmove((unsigned char *)dstPtr, (unsigned char *)srcPtr, len * typeSize);

	return 0;
}

//
// Convert
//

// base/framework/Convert_toInt_(Ljava/lang/String;)I
long Convert_StringToInt(Var stack[]){
	WObject string, charArray;
	long i, isNeg, len, value;
	unsigned short *chars;
	Var v;

	v.intValue = 0;
	string = stack[0].obj;
	if (string == WOBJECT_NULL)
		return ERR_NullObjectAccess;
	charArray = WOBJ_StringCharArrayObj(string);
	if (charArray == WOBJECT_NULL)
		return ERR_NullObjectAccess;
	chars = (unsigned short *)WOBJ_arrayStart(charArray);
	len = WOBJ_arrayLen(charArray);

	// NOTE: We do it all here instead of calling atoi() since it looks
	// like various versions of CE don't support atoi(). It's also faster
	// this way since we don't have to convert to a byte array.
	isNeg = 0;
	if (len > 0 && chars[0] == '-')
		isNeg = 1;
	value = 0;
	for (i = isNeg; i < len; i++){
		if (chars[i] < (unsigned short)'0' || chars[i] > (unsigned short)'9')
			return 1;
		value = (value * 10) + ((long)chars[i] - (long)'0');
	}
	if (isNeg)
		value = -(value);

	v.intValue = value;

	stack[0] = v;
	return 0;
}

// base/framework/Convert_toString_(I)Ljava/lang/String;
long Convert_IntToString(Var stack[]){
	Var v;
	char buf[20];

	utils_ltoa( stack[0].intValue, buf, 10 );
	v.obj = createString(buf);
	stack[0] = v;
	return 0;
}

// base/framework/Convert_toString_(C)Ljava/lang/String;
long Convert_CharToString(Var stack[]){
	Var v;
	char buf[2];

	buf[0] = (char)stack[0].intValue;
	buf[1] = 0;
	v.obj = createString(buf);

	stack[0] = v;
	return 0;
}

// base/framework/Convert_toString_(Z)Ljava/lang/String;
long Convert_BooleanToString(Var stack[]){
	Var v;
	char *s;

	if (stack[0].intValue == 0)
		s = "false";
	else
		s = "true";
	v.obj = createString(s);

	stack[0] = v;
	return 0;
}

// base/framework/FCSystem_sleep_(I)I
long FCSystem_sleep(Var stack[]) {
	Var v;
	long millis;

	millis = stack[0].intValue;
#ifdef WIN32
	Sleep(millis);
#endif

	v.obj = 0;

	stack[0] = v;
	return 0;
}

// base/framework/System_getClassName_(Ljava/lang/Object;)Ljava/lang/String;
long FCSystem_getClassName(Var stack[]){
	Var v;
	WObject obj;
	WClass *wclass;

	obj = stack[0].obj;
	if( obj == WOBJECT_NULL )
		return ERR_NullObjectAccess;
	wclass = WOBJ_class( obj );
	v.obj = createStringFromUtf( getUtfString( wclass, wclass->classNameIndex ) );

	stack[0] = v;
	return 0;
}

// base/framework/System_hasClass_(Ljava/lang/String;)Z
long FCSystem_hasClass(Var stack[]){
	Var v;
	UtfString utf;

	v.intValue = 0;
	if( stack[0].obj == WOBJECT_NULL )
		return ERR_NullObjectAccess;

	utf = stringToUtf( stack[0].obj, STU_USE_STATIC | STU_NULL_TERMINATE );
	if( getClass( utf ) != NULL )
		v.intValue = 1;
	else
		v.intValue = 0;

	stack[0] = v;
	return 0;
}

// base/framework/System_print_(Ljava/lang/String;)V
long FCSystem_print(Var stack[]){
	UtfString utf;

	utf = stringToUtf( stack[0].obj, STU_USE_STATIC | STU_NULL_TERMINATE );
	debuglog( utf.str );

	return 0;
}

// base/framework/System_newInstance_(Ljava/lang/String;)Ljava/lang/Object;
long FCSystem_newInstance(Var stack[]){
	Var v;
	UtfString utf, baseutf;
	long ret;
	unsigned char retType;

	v.obj = WOBJECT_NULL;
	if( stack[0].obj == WOBJECT_NULL )
		return ERR_NullObjectAccess;

	utf = stringToUtf( stack[0].obj, STU_USE_STATIC | STU_NULL_TERMINATE );
	baseutf = createUtfString("");

	ret = newClass( utf, baseutf, &retType, &v );
	if (ret != 0 || retType == RET_TYPE_NONE)
		return -1;

	if (retType == RET_TYPE_EXCEPTION)
		return ERR_Unknown;

	stack[0] = v;

	return 0;
}

// base/framework/System_printStackTrace_()V
long FCSystem_printStackTrace(Var stack[]){
	VmPrintStackTrace();
	return 0;
}

// base/framework/System_getInput_(I)[Ljava/lang/String;
long FCSystem_getInput(Var stack[]) {
	Var v;
	unsigned char num;
	unsigned char i;
	WObject strArray;
	WObject *obj;
	unsigned long ptr;
	long max;

	if (inoutBuff_ext == NULL)
		return ERR_CondNotSatisfied;

	max = stack[0].intValue;
	num = inoutBuff_ext[0];
	if (max >= 0 && num > max)
		num = (unsigned char)max;
	strArray = createArrayObject(TYPE_OBJECT, num);
	obj = (WObject *)WOBJ_arrayStart(strArray);

	if (pushObject(strArray) != FT_ERR_OK)
		return ERR_OutOfObjectMem;

	ptr = 1;
	for (i = 0; i < num; i++) {
		obj[i] = createString((const char*)&inoutBuff_ext[ptr]);
		if (obj[i] == WOBJECT_NULL) {
			popObject();
			return ERR_OutOfObjectMem;
		}
		ptr += strlen((const char*)&inoutBuff_ext[ptr]) + 1;
	}

	popObject();

	v.obj = strArray;
	stack[0] = v;

	return 0;
}

// base/framework/System_setOutput_([Ljava/lang/String;)V
long FCSystem_setOutput(Var stack[]) {
	WObject strArray, charArray;
	WObject *obj;
	long len;
	unsigned short *chars;
	unsigned long ptr;
	unsigned char i;
	long j;

	if (inoutBuff_ext == NULL)
		return ERR_NullObjectAccess;

	strArray = stack[0].obj;
	if (strArray == WOBJECT_NULL)
		return ERR_NullObjectAccess;
	int num = WOBJ_arrayLen(strArray);
	if (num > 255)
		return ERR_ParamError;
	obj = (WObject *)WOBJ_arrayStart(strArray);
	ptr = 1;
	for (i = 0; i < num; i++) {
		charArray = WOBJ_StringCharArrayObj(obj[i]);
		if (charArray == WOBJECT_NULL)
			return ERR_NullObjectAccess;
		chars = (unsigned short *)WOBJ_arrayStart(charArray);
		len = WOBJ_arrayLen(charArray);

		if ((ptr + len + 1) > inoutBuffSize_ext)
			return ERR_OutOfObjectMem;

		for (j = 0; j < len; j++)
			inoutBuff_ext[ptr + j] = (unsigned char)chars[j];
		inoutBuff_ext[ptr + len] = '\0';
		ptr += len + 1;
	}

	inoutBuff_ext[0] = (unsigned char)num;

	return 0;
}

// This array is used to map a hash value to a corresponding native function.
// It must remain sorted by hash value because a binary search is performed
// to find a method by its hash value. There is a small chance of collision
// if two hashes match and if one occurs, the function name should be changed
// to avoid collision. To prevent users from creating invalid native methods
// that hash to a valid value, native methods could be restricted to a specific
// set of classes (this is probably not necessary since any verifier probably
// wouldn't allow native methods to get by anyway).

NativeMethod nativeMethods[] = {
	// base/framework/System_gc_()V
	{ 320166981UL, FCSystem_gc },
	// base/framework/System_print_(Ljava/lang/String;)V
	{ 320167194UL, FCSystem_print },
	// base/framework/System_newInstance_(Ljava/lang/String;)Ljava/lang/Object;
	{ 320175153UL, FCSystem_newInstance },
	// base/framework/System_getClassName_(Ljava/lang/Object;)Ljava/lang/String;
	{ 320178738UL, FCSystem_getClassName },
	// base/framework/System_arraycopy_(Ljava/lang/Object;ILjava/lang/Object;II)V
	{ 320182067UL, FCSystem_arrayCopy },
	// base/framework/System_hasClass_(Ljava/lang/String;)Z
	{ 320184157UL, FCSystem_hasClass },
	// base/framework/System_printStackTrace_()V
	{ 320187986UL, FCSystem_printStackTrace },
	// base/framework/System_getInput_(I)[Ljava/lang/String;
	{ 320190814UL, FCSystem_getInput },
	// base/framework/System_sleep_(I)I
	{ 320192265UL, FCSystem_sleep },
	// base/framework/System_setOutput_([Ljava/lang/String;)V
	{ 320200671UL, FCSystem_setOutput },

	// base/framework/Convert_toInt_(Ljava/lang/String;)I
	{ 706105882UL, Convert_StringToInt },
	// base/framework/Convert_toString_(C)Ljava/lang/String;
	{ 706126749UL, Convert_CharToString },
	// base/framework/Convert_toString_(I)Ljava/lang/String;
	{ 706127133UL, Convert_IntToString },
	// base/framework/Convert_toString_(Z)Ljava/lang/String;
	{ 706128221UL, Convert_BooleanToString },

	// base/framework/Util_byteArrayCopy_([BI[BII)V
	{ 3646096023UL, Util_byteArrayCopy },
	// base/framework/Util_byteArrayCompare_([BI[BII)I
	{ 3646114394UL, Util_byteArrayCompare },
	// base/framework/Util_byteArrayFill_([BIIB)V
	{ 3646149781UL, Util_byteArrayFill },
};

int NumberOfNativeMethods = sizeof(nativeMethods) / sizeof(NativeMethod);

unsigned char *nativeLoadClass(UtfString className ){
	char path[128];

	strncpy( path, className.str, className.len );
	path[ className.len ] = '\0';
	return getClassCode( path );
}
