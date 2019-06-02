/*
Copyright (C) 1998, 1999, 2000 Wabasoft

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; either version 2 of the License, or (at your option) any later version. 

This program is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE. See the GNU General Public License for more details. 

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc., 675 Mass Ave,
Cambridge, MA 02139, USA. 
*/

#include "waba.h"
#include "waba_native.h"
#include "waba_utf.h"
#include "opcode.h"
#include "alloc_class.h"
#include "utils.h"
#include "mem_alloc.h"
#include "debuglog.h"
#include "waba_heap.h"
#include <string.h>

/*

Welcome to the WabaVM source. This is the platform indepdenent code for the
interpreter, class parser and garbage collector. I like the code myself (of
course I wrote it, so that's not saying much) and hopefully it doesn't seem
too complicated. Actually, I'm hoping it reads rather easily given what it
does.

If you're looking through the source and wondering about the QUICKBIND stuff,
you should know that you can turn it off completely by removing all the code
with an #ifdef QUICKBIND around it. It's an optimization that speeds things
up quite a bit. So, if you're trying to figure out the code, ignore the
QUICKBIND stuff at first and then look at it later on.

The SMALLMEM define determines whether the VM uses a small or large memory
model (16 bit or 32 bit offsets). The default is SMALLMEM which means that if
progams use memory beyond a certain size, they jump to being slow since they
can't QUICKBIND any more. It still works since the QUICKBIND is adaptive, if
the offset fits, it uses it, if not, it doesn't use QUICKBIND.

This file should pretty much compile on any platform. To port the VM, you
create the 3 native method (nm) files. See the nmport_a.c if you are
interested in starting a port or to see how easy/difficult it would be.

Have a blast!

Rick Wild

*/

/*
"True words are not beautiful, beautiful words are not true.
 The good are not argumentative, the argumentative are not good.
 Knowers do not generalize, generalists do not know.
 Sages do not accumulate anything but give everything to others,
 having more the more they give.
 The Way of heaven helps and does not harm.
 The Way for humans is to act without contention."

 - Lao-tzu, Tao Te Ching circa 500 B.C.
*/

//
// Error Handling
//

// for debug
static const char *errorMessages[] = {
	"",
	"sanity",
	"can't allocate memory",
	"out of class memory",
	"out of object memory",
	"native stack overflow",
	"native stack underflow",
	"stack overflow",
	"native error return",
	"bad opcode",
	"can't find class",
	"can't find method",
	"can't find field",
	"class too large",
	"can't load const",
	"can't load field",
	"can't load method",
	"clinit method error",
	"constant to var",
	"bad param number",
	"param error",
	"can't create object",
	"not array",
	"bad class name",
	"can't find native",
	"not main class",
	"null object access",
	"null array access",
	"index out of range",
	"divide by zero",
	"bad class cast",
	"negative array size",
	"can't array store",
	"bad class code",
};


// The VM keeps an array of constant offsets for each constant in a class
// in the runtime class structure (see WClassStruct). For each constant,
// the offset is an offset from the start of the bytes defining the class.
// Depdending on whether SMALLMEM is defined, the offset is either a 16 or
// 32 bit quantity. So, if SMALLMEM is defined, the maximum offset is 2^16.
// However, we also keep a bit in the constant to determine whether the
// constant is an offset that is "bound" or not. So, the maximum value of
// an offset if SMALLMEM is defined (the small memory model) is 32767.
//
// This means under the small memory model, the biggest class constant
// pool we can have is 32K. Under the large memory model (SMALLMEM not
// defined) the maximum class constant pool size that we could have is
// 2^31 bytes. Using SMALLMEM can save quite a bit of memory since
// constant pools tend to be large.
//
// When a constant offset is "bound", instead of the offset being
// an offset into the constant pool, it is (with the exception of methods)
// a pointer offset from the start of the class heap to the actual data
// the constant refers to.
//
// For example, when a field constant is bound, it contains an offset
// from the start of the class heap to the actual WClassField * structure
// for the field. For class offsets, it is an offset to the WClass *
// structure. For method offsets, the offset is a virtual method number
// and class index. Only class, field and methods can be bound.
//
// A bound offset will only be bound if the offset of the actual structure
// in the class heap is within the range that can fit in the offset. For
// example, in a small memory model, if a WClassField * structure exists
// beyond 32K from the start of the class heap, its offset can't be bound.
// If that happens, the offset simply won't be bound and will retain
// an offset into the constant pool (known now as an "adaptive bind").
//
// Binding of constants (adaptive quickbind) will only be performed if
// QUICKBIND is defined. When an offset is bound, it's CONS_boundBit
// will be set to 1.

//
// private function prototypes
//

static unsigned char *skipClassConstant(WClass *wclass, unsigned short idx, unsigned char *p);
static unsigned char *loadClassField(WClass *wclass, WClassField *field, unsigned char *p);
static Var constantToVar(WClass *wclass, unsigned short idx);
static unsigned char *loadClassMethod(WClass *wclass, WClassMethod *method, unsigned char *p);
static WObject createMultiArray(long ndim, char *desc, Var *sizes);
static WClassField *getField(WClass *wclass, UtfString name, UtfString desc, WClass **vclass);
static WClassField *getFieldByIndex(WClass *wclass, unsigned short fieldIndex, WClass **vclass);
static WClass *getClassByIndex(WClass *wclass, unsigned short classIndex);
static long countMethodParams(UtfString desc);
static NativeFunc getNativeMethod(WClass *wclass, UtfString methodName, UtfString methodDesc);
static void setClassHooks(WClass *wclass);
static unsigned char arrayType(char c);

//
// global vars
//
static int vmInitialized = 0;

// virtual machine stack
static Var *vmStack;
static unsigned long vmStackSize; // in Var units
static unsigned long vmStackPtr;

// native method stack
static WObject *nmStack;
static unsigned long nmStackSize; // in WObject units
static unsigned long nmStackPtr;

// class heap
static unsigned char *classHeap;
static unsigned long classHeapSize;
static unsigned long classHeapUsed;
static WClass **classHashList;

// error status
ErrorStatus vmStatus;

//
// public functions
//

/*
 "I have three treasures that I keep and hold:
  one is mercy,
  the second is frugality,
  the third is not presuming to be at the head of the world.
  By reason of mercy, one can be brave.
  By reason of frugality, one can be broad.
  By not presuming to be at the head of the world,
  one can make your potential last."
 */

long VmInit(unsigned long vmStackSizeInBytes, unsigned long nmStackSizeInBytes,
	unsigned long _classHeapSize, unsigned long _objectHeapSize ) {
	unsigned long i;

	if( vmInitialized )
		return FT_ERR_INVALID_STATUS;

	VmResetError();

	// NOTE: ordering is important here. The first thing we
	// need to do is initialize the global variables so if we
	// return not fully initialized, a VmFree() call will still
	// operate correctly. Also, its important not to statically
	// initialize them since VmInit() can be called after VmFree()
	// and if they were just statically intialized, they wouldn't
	// get reset.
	vmStack = NULL;
	vmStackSize = vmStackSizeInBytes / sizeof(Var);
	vmStackPtr = 0;
	nmStack = NULL;
	nmStackSize = nmStackSizeInBytes / sizeof(WObject);
	nmStackPtr = 0;
	classHeap = NULL;
	classHeapSize = _classHeapSize;
	classHeapUsed = 0;

	// allocate stacks and init
	vmStack = (Var *)mem_alloc(vmStackSizeInBytes);
	nmStack = (WObject *)mem_alloc(nmStackSizeInBytes);
	classHeap = (unsigned char *)mem_alloc(classHeapSize);
	classHashList = (WClass**)mem_alloc( sizeof(WClass*) * CLASS_HASH_SIZE );
	if (vmStack == NULL || nmStack == NULL || classHeap == NULL || classHashList == NULL)
		goto error;

	// zero out memory areas
	memset((unsigned char *)vmStack, 0x00, vmStackSizeInBytes);
	memset((unsigned char *)nmStack, 0x00, nmStackSizeInBytes);
	memset((unsigned char *)classHeap, 0x00, classHeapSize);
	for (i = 0; i < CLASS_HASH_SIZE; i++)
		classHashList[i] = NULL;

	if (initObjectHeap(_objectHeapSize) != FT_ERR_OK)
		goto error;

	if( initClassBlock() != FT_ERR_OK ){
		freeObjectHeap();
		goto error;
	}

	stringClass = getClass(createUtfString("java/lang/String"));
	if (stringClass == NULL) {
		closeClassBlock();
		freeObjectHeap();
		goto error;
	}

	vmInitialized = 1;
	
	return FT_ERR_OK;

error:
	if (vmStack != NULL) {
		mem_free(vmStack);
		vmStack = NULL;
	}
	if (nmStack != NULL) {
		mem_free(nmStack);
		nmStack = NULL;
	}
	if (classHeap != NULL) {
		mem_free(classHeap);
		classHeap = NULL;
	}
	if (classHashList != NULL) {
		mem_free(classHashList);
		classHashList = NULL;
	}

	return FT_ERR_NOTENOUGH;
}

void VmFree(void) {
	if (!vmInitialized)
		return;

	// NOTE: The object heap is freed first since it requires access to
	// the class heap to call object destroy methods
	// destroy methods 

	stringClass = NULL;
	closeClassBlock();
	freeObjectHeap();

	mem_free(vmStack);
	vmStack = NULL;
	mem_free(nmStack);
	nmStack = NULL;
	mem_free(classHeap);
	classHeap = NULL;
	mem_free(classHashList);
	classHashList = NULL;

	vmInitialized = 0;
}

long newClass( UtfString className, UtfString baseClassName, unsigned char *retType, Var* retVar ){
	WClass* wclass, *basewclass;
	WObject wobject = WOBJECT_NULL;
	Var params[1];
	WClassMethod* initMethod;
	long ret;

	wclass = getClass( className );
	if (wclass == NULL)
		return FT_ERR_NOTFOUND;

	if( baseClassName.len != 0 && baseClassName.str != NULL ){
		basewclass = getClass( baseClassName );
		if (basewclass == NULL)
			return FT_ERR_FAILED;

		if (compatible(wclass, basewclass) == 0)
			return FT_ERR_INVALID_STATUS;
	}

	wobject = createObject(wclass);
	if (wobject == WOBJECT_NULL)
		return FT_ERR_NOTENOUGH;
	if (pushObject(wobject) != FT_ERR_OK)  // make sure it doesn't get GC'd
		return FT_ERR_NOTENOUGH;
	params[0].obj = wobject;

	initMethod = getMethod(wclass, createUtfString("<init>"), createUtfString("()V"), NULL);
	if (initMethod != NULL)
	{
		ret = executeMethod(wclass, initMethod, params, 1, retType, retVar);
		if (ret != 0 || *retType != RET_TYPE_NONE){
			popObject();
			return FT_ERR_FAILED;
		}
	}

	popObject();
	retVar->obj = wobject;

	return FT_ERR_OK;
}

//
// Class Loader
//
static unsigned long genHashCode(UtfString name) {
	unsigned long value, i;

	value = 0;
	for (i = 0; i < name.len; i++)
		value += name.str[i];
	value = (value << 6) + name.len;
	return value;
}

static unsigned char *allocClassPart(unsigned long size) {
	unsigned char *p;

	// align to 4 byte boundry
	size = (size + 3) & ~3;
	if (classHeapUsed + size > classHeapSize) {
		VmSetFatalErrorNum(ERR_OutOfClassMem);
		return NULL;
	}
	p = &classHeap[classHeapUsed];
	classHeapUsed += size;
	return p;
}

WClass *getClass(UtfString className) {
	WClass *wclass, *superClass;
	WClassMethod *method;
	UtfString iclassName;
	unsigned short i, superClassIndex;
	unsigned long classHash, size;
	Var retVar;
	unsigned char *p;
	long ret;
	unsigned char retType;

	// look for class in hash list
	classHash = genHashCode(className) % CLASS_HASH_SIZE;
	wclass = classHashList[classHash];
	while (wclass != NULL) {
		iclassName = getUtfString(wclass, wclass->classNameIndex);
		if (className.len == iclassName.len &&
			strncmp(className.str, iclassName.str, className.len) == 0 )
			return wclass;
		wclass = wclass->nextClass;
	}

	p = nativeLoadClass(className);
	if (p == NULL) {
		VmSetFatalError(ERR_CantFindClass, &className, 1 );
		return NULL;
	}

	// NOTE: The garbage collector may run at any time during the
	// loading process so we need to make sure the class is valid
	// whenever an allocation may occur (static class vars, <clinit>, etc)
	// The various int variables will be initialzed to zero since
	// the entire class areas is zeroed out to start with.
	wclass = (WClass *)allocClassPart(sizeof(WClass));
	if (wclass == NULL)
		return NULL;

	wclass->byteRep = p;
	wclass->objDestroyFunc = NULL;

	// parse constants
	if( p[0] != 0xca || p[1] != 0xfe || p[2] != 0xba || p[3] != 0xbe )
		return NULL;
	p += 4;
	p += 2; // minor version
	p += 2; // major version
	wclass->numConstants = utils_get_uint16b(p);
	p += 2;
	if (wclass->numConstants != 0){
		size = sizeof(ConsOffsetType) * ( wclass->numConstants );
		wclass->constantOffsets = (ConsOffsetType *)allocClassPart(size);
		if (wclass->constantOffsets == NULL)
			return NULL;
		wclass->constantOffsets[0] = CONSTANT_Reserved;
		for (i = 1; i < wclass->numConstants; i++) {
			if (p - wclass->byteRep > MAX_consOffset) {
				VmSetFatalError(ERR_ClassTooLarge, &className, 1 );
				return NULL;
			}
			wclass->constantOffsets[i] = (ConsOffsetType)( p - wclass->byteRep );
			p = skipClassConstant(wclass, i, p);
			if( p == NULL ){
				VmSetFatalError(ERR_LoadConst, &className, 1 );
				return NULL;
			}
		}
	}
	else{
		wclass->constantOffsets = NULL;
	}

	// second attribute section
	wclass->attrib2 = p;
	p += 2; // access flag
	p += 2; // this class
	p += 2; // super class

	// assign class name
	wclass->classNameIndex = CONS_nameIndex(wclass, WCLASS_thisClass(wclass));

	// NOTE: add class to class list here so garbage collector can
	// find it during the loading process if it needs to collect.
	wclass->nextClass = classHashList[classHash];
	classHashList[classHash] = wclass;

	// load superclasses (recursive) here so we can resolve var
	// and method offsets in one pass
	superClassIndex = WCLASS_superClass(wclass);
	if (superClassIndex != 0) {
		superClass = getClassByIndex(wclass, superClassIndex);
		if (superClass == NULL){
			VmSetFatalErrorNum(ERR_CantFindClass);
			return NULL; // can't find superclass
		}
		// fill in superclasses table
		wclass->numSuperClasses = superClass->numSuperClasses + 1;
		size = wclass->numSuperClasses * sizeof(WClass *);
		wclass->superClasses = (WClass **)allocClassPart(size);
		if (wclass->superClasses == NULL) {
			VmSetFatalErrorNum(ERR_OutOfClassMem);
			return NULL;
		}
		memmove(wclass->superClasses, superClass->superClasses, superClass->numSuperClasses * sizeof(WClass *));
		wclass->superClasses[superClass->numSuperClasses] = superClass;

		// inherit num of superclass variables to start
		wclass->numVars = superClass->numVars;
	}else{
		superClass = NULL;
		wclass->numSuperClasses = 0;
		wclass->superClasses = NULL;
		wclass->numVars = 0;
	}

	// skip past interfaces
	p += 2 + (utils_get_uint16b(p) * 2);

	// parse fields
	wclass->numFields = utils_get_uint16b(p);
	p += 2;
	if (wclass->numFields != 0) {
		size = sizeof(WClassField) * wclass->numFields;
		wclass->fields = (WClassField *)allocClassPart(size);
		if (wclass->fields == NULL)
			return NULL;
		for (i = 0; i < wclass->numFields; i++){
			p = loadClassField(wclass, &wclass->fields[i], p);
			if( p == NULL ){
				VmSetFatalError(ERR_LoadField, &className, 1 );
				return NULL;
			}
		}
	}
	else{
		wclass->fields = NULL;
	}

	// parse methods
	wclass->numMethods = utils_get_uint16b(p);
	p += 2;
	if (wclass->numMethods != 0) {
		size = sizeof(WClassMethod) * wclass->numMethods;
		wclass->methods = (WClassMethod *)allocClassPart(size);
		if (wclass->methods == NULL)
			return NULL;
		for (i = 0; i < wclass->numMethods; i++){
			p = loadClassMethod(wclass, &wclass->methods[i], p);
			if( p == NULL ){
				VmSetFatalError(ERR_LoadMethod, &className, 1 );
				return NULL;
			}
		}
	}
	else{
		wclass->methods = NULL;
	}

	// skip final attributes section

	// set hooks (before class init which might create/free objects of this type)
	setClassHooks(wclass);

	// if our superclass has a destroy func, we inherit it. If not, ours overrides
	// our base classes destroy func (the native destroy func should call the
	// superclasses)
	if (superClass != NULL && wclass->objDestroyFunc == NULL)
		wclass->objDestroyFunc = superClass->objDestroyFunc;

	// call static class initializer method if present
	method = getMethod(wclass, createUtfString("<clinit>"), createUtfString("()V"), NULL);
	if (method != NULL){
		ret = executeMethod(wclass, method, NULL, 0, &retType, &retVar);
		if (ret != 0 || retType != RET_TYPE_NONE){
			VmSetFatalError(ERR_CLInitMethodError, &className, 1);
			return NULL;
		}
	}

	return wclass;
}

static unsigned char *skipClassConstant(WClass *wclass, unsigned short idx, unsigned char *p) {
	p++;
	switch (CONS_tag(wclass, idx)) {
		case CONSTANT_Utf8:
			p += 2; // length
			p += CONS_utfLen(wclass, idx);
			break;
		case CONSTANT_Integer:
		case CONSTANT_Float:
		case CONSTANT_Fieldref:
		case CONSTANT_Methodref:
		case CONSTANT_InterfaceMethodref:
		case CONSTANT_NameAndType:
			p += 4;
			break;
		case CONSTANT_Class:
		case CONSTANT_String:
			p += 2;
			break;
		case CONSTANT_Long:
		case CONSTANT_Double:
		default:
			return NULL;
	}
	return p;
}

static Var constantToVar(WClass *wclass, unsigned short idx) {
	Var v;
	unsigned short stringIndex;

	switch (CONS_tag(wclass, idx)) {
		case CONSTANT_Integer:
			v.intValue = CONS_integer(wclass, idx);
			break;
		case CONSTANT_String:
			stringIndex = CONS_stringIndex(wclass, idx);
			v.obj = createStringFromUtf(getUtfString(wclass, stringIndex));
			break;
		case CONSTANT_Float:
		case CONSTANT_Long:
		case CONSTANT_Double:
		default:
			VmSetFatalErrorNum(ERR_ConstantToVar);
			v.obj = WOBJECT_NULL; // bad constant
			break;
	}
	return v;
}

static unsigned char *loadClassField(WClass *wclass, WClassField *field, unsigned char *p) {
	unsigned long i, bytesCount;
	unsigned short attrCount, nameIndex;
	UtfString attrName;

	field->header = p;

	// compute offset of this field's variable in the object
	if (!FIELD_isStatic(field))
		field->var.varOffset = wclass->numVars++;
	else
		field->var.staticVar.obj = WOBJECT_NULL;

	p += 2; // access flag
	p += 2; // field name
	p += 2; // descriptor
	attrCount = utils_get_uint16b(p);
	p += 2;

	for (i = 0; i < attrCount; i++) {
		nameIndex = utils_get_uint16b(p);
		attrName = getUtfString(wclass, nameIndex);
		p += 2;
		bytesCount = utils_get_uint32b(p);
		p += 4;
		if (FIELD_isStatic(field) && attrName.len == 13 && bytesCount == 2 &&
			strncmp(attrName.str, "ConstantValue", 13) == 0)
			field->var.staticVar = constantToVar(wclass, utils_get_uint16b(p));
		else
			; // MS Java has COM_MapsTo field attributes which we skip
		p += bytesCount;
	}
	return p;
}

static unsigned char *loadClassMethod(WClass *wclass, WClassMethod *method, unsigned char *p) {
	unsigned long i, j, bytesCount;
	unsigned short attrCount, attrNameIndex, numAttributes;
	long numParams;
	unsigned char *attrStart;
	UtfString attrName, methodName, methodDesc;
	WClassHandler *handler;
	unsigned long size;

	method->header = p;
	p += 2; // access flag
	p += 2; // method name
	p += 2; // descriptor
	attrCount = utils_get_uint16b(p);
	p += 2;
	method->code.codeAttr = NULL;
	for (i = 0; i < attrCount; i++) {
		attrStart = p;
		attrNameIndex = utils_get_uint16b(p);
		p += 2;
		attrName = getUtfString(wclass, attrNameIndex);
		bytesCount = utils_get_uint32b(p);
		p += 4;
		if (attrName.len != 4 || strncmp(attrName.str, "Code", 4) != 0) {
			p += bytesCount;
			continue;
		}
		// Code Attribute
		method->code.codeAttr = attrStart;
		p += 2; // max_stack
		p += 2; // max_locals
		p += 4 + utils_get_uint32b(p); // code_lenth, code

		// Handler
		method->numHandlers = utils_get_uint16b(p); // exception_table_length
		p += 2;

		size = sizeof(WClassHandler) * method->numHandlers;
		method->handlers = (WClassHandler*)allocClassPart( size );
		if( method->handlers == NULL ){
			method->numHandlers = 0;
			return NULL;
		}
		for (j = 0; j < method->numHandlers; j++){
			handler = &(method->handlers[j]);
			handler->start_pc = utils_get_uint16b(p);
			p += 2;
			handler->end_pc = utils_get_uint16b(p);
			p += 2;
			handler->handler_pc = utils_get_uint16b(p);
			p += 2;
			handler->catch_type = utils_get_uint16b(p);
			p += 2;
		}

		// skip attributes
		numAttributes = utils_get_uint16b(p);
		p += 2;
		for (j = 0; j < numAttributes; j++) {
			p += 2; // attribute_name_index
			p += 4 + utils_get_uint32b(p); // attribute_length, attribute_info
		}
	}

	// determine numParams, isInit and returnsValue
	methodDesc = getUtfString(wclass, METH_descIndex(method));
	methodName = getUtfString(wclass, METH_nameIndex(method));
	numParams = countMethodParams(methodDesc);
	if (numParams < 0){
		VmSetFatalError(ERR_BadParamNum, &methodName, 1 );
		return NULL;
	}
	method->numParams = (unsigned short)numParams;

	// whether method is <init>
	if (methodName.len > 2 && methodName.str[0] == '<' && methodName.str[1] == 'i')
		method->isInit = 1;
	else
		method->isInit = 0;
	if (methodDesc.str[methodDesc.len - 1] != 'V')
		method->returnsValue = 1;
	else
		method->returnsValue = 0;

	// resolve native functions
	if (METH_isNative(method)){
		method->code.nativeFunc = getNativeMethod(wclass, methodName, methodDesc);
		if( method->code.nativeFunc == NULL )
			return NULL;
	}
	return p;
}

static long countMethodParams(UtfString desc) {
	unsigned long n;
	char *c;

	c = desc.str;
	if (*c++ != '(')
		return -1;
	n = 0;
	while (1) {
		switch (*c) {
			case 'B':
			case 'C':
			case 'I':
			case 'S':
			case 'Z':
			case 'F':
				n++;
				c++;
				break;
			case 'D':
			case 'J':
				// long/double not supported
				return -2;
			case 'L':
				c++;
				while (*c++ != ';');
				n++;
				break;
			case '[':
				c++;
				break;
			case ')':
				return n;
			default:
				return -3;
		}
	}
}

//
// Object Routines
//
WObject createObject(WClass *wclass) {
	WObject obj;

	if (wclass == NULL){
		VmSetFatalErrorNum(ERR_ParamError);
		return WOBJECT_NULL;
	}
	if (WCLASS_isAbstract(wclass)){
		VmSetFatalErrorNum(ERR_CantCreateObject);
		return WOBJECT_NULL; // interface or abstract class
	}
	obj = allocObject(WCLASS_objectSize(wclass));
	if (obj == WOBJECT_NULL)
		return WOBJECT_NULL;
	WOBJ_class(obj) = wclass;
	return obj;
}

long arrayTypeSize(unsigned char type) {
	switch (type) {
		case TYPE_OBJECT:  // object
		case TYPE_ARRAY:  // array
		case TYPE_FLOAT:  // float
		case TYPE_INT: // int
			return 4;
		case TYPE_BOOLEAN: // boolean
		case TYPE_BYTE: // byte
			return 1;
		case TYPE_CHAR:  // char
		case TYPE_SHORT:  // short
			return 2;
//		case TYPE_DOUBLE:  // double (invalid)
//		case TYPE_LONG: // long (invalid)
//			return 8;
	}
	VmSetFatalErrorNum(ERR_ParamError);
	return 0;
}

long arraySize(unsigned char type, long len) {
	long typesize, size;

	typesize = arrayTypeSize(type);
	size = (3 * sizeof(Var)) + (typesize * len);
	// align to 4 byte boundry
	size = (size + 3) & ~3;
	return size;
}

WObject createArrayObject(unsigned char type, long len) {
	WObject obj;

	if( len < 0 ){
		VmSetFatalErrorNum(ERR_ParamError);
		return WOBJECT_NULL;
	}
	obj = allocObject(arraySize(type, len));
	if ( obj == WOBJECT_NULL)
		return WOBJECT_NULL;
	// pointer to class is NULL for arrays
	WOBJ_class(obj) = NULL;
	WOBJ_arrayType(obj) = type;
	WOBJ_arrayLen(obj) = len;
	return obj;
}

static unsigned char arrayType(char c) {
	switch(c) {
		case 'L': // object
			return TYPE_OBJECT;
		case '[': // array
			return TYPE_ARRAY;
		case 'Z': // boolean
			return TYPE_BOOLEAN;
		case 'B': // byte
			return TYPE_BYTE;
		case 'C': // char
			return TYPE_CHAR;
		case 'S': // short
			return TYPE_SHORT;
		case 'I': // int
			return TYPE_INT;
		case 'F': // float
			return TYPE_FLOAT;
		case 'D': // double(invalid)
			return TYPE_DOUBLE;
		case 'J': // long(invalid)
			return TYPE_LONG;
	}
	VmSetFatalErrorNum(ERR_ParamError);
	return TYPE_UNKNOWN;
}

static WObject createMultiArray(long ndim, char *desc, Var *sizes) {
	WObject arrayObj, subArray, *itemStart;
	long i, len;
	unsigned char type;

	len = sizes[0].intValue;
	if( len < 0 ){
		VmSetFatalErrorNum(ERR_ParamError);
		return WOBJECT_NULL;
	}
	type = arrayType(desc[0]);
	if( type == TYPE_UNKNOWN )
		return WOBJECT_NULL;
	arrayObj = createArrayObject(type, len);
	if (arrayObj == WOBJECT_NULL)
		return WOBJECT_NULL;
	if (type != TYPE_ARRAY || ndim <= 1)
		return arrayObj;
	// NOTE: it is acceptable to push only the "upper"
	// array objects and not the most derived subarrays
	// because if the array is only half filled and we gc,
	// the portion that is filled will still be found since
	// its container was pushed
	if (pushObject(arrayObj) != FT_ERR_OK)
		return WOBJECT_NULL;

	// create subarray (recursive)
	itemStart = (WObject *)WOBJ_arrayStart(arrayObj);
	for (i = 0; i < len; i++) {
		// NOTE: we have to recompute itemStart after calling createMultiArray()
		// since calling it can cause a GC to occur which moves memory around
		subArray = createMultiArray(ndim - 1, desc + 1, sizes + 1);
		if( subArray == WOBJECT_NULL ){
			popObject();
			return WOBJECT_NULL;
		}
		itemStart[i] = subArray;
	}
	popObject();
	return arrayObj;
}

int arrayRangeCheck(WObject array, long start, long count) {
	long len;

	if (array == WOBJECT_NULL || start < 0 || count < 0)
		return 0;
	len = WOBJ_arrayLen(array);
	if (start + count > len)
		return 0;
	return 1;
}

static WClass *getClassByIndex(WClass *wclass, unsigned short classIndex) {
	UtfString className;

	className = getUtfString(wclass, CONS_nameIndex(wclass, classIndex));
	if (className.len > 1 && className.str[0] == '['){
		VmSetFatalErrorNum(ERR_BadClassName);
		return NULL; // arrays have no associated class
	}
	return getClass(className);
}

static WClassField *getField(WClass *wclass, UtfString name, UtfString desc, WClass **vclass) {
	WClassField *field;
	UtfString fname, fdesc;
	unsigned short i, n;

	n = wclass->numSuperClasses;
	while (1) {

		for (i = 0; i < wclass->numFields; i++) {
			field = &wclass->fields[i];
			fname = getUtfString(wclass, FIELD_nameIndex(field));
			fdesc = getUtfString(wclass, FIELD_descIndex(field));
			if (name.len == fname.len &&
				desc.len == fdesc.len &&
				strncmp(name.str, fname.str, name.len) == 0 &&
				strncmp(desc.str, fdesc.str, desc.len) == 0){
				return field;
			}
		}

		if (vclass == NULL)
			break; // not a virtual lookup or no superclass

		if (n == 0)
			break;
		// look in superclass
		wclass = wclass->superClasses[--n];
	}

	{
		UtfString utfs[3];
		utfs[0] = getUtfString(wclass, wclass->classNameIndex);
		utfs[1] = name;
		utfs[2] = desc;
		VmSetFatalError(ERR_CantFindField, utfs, sizeof(utfs) );
	}
	return NULL;
}

static WClassField *getFieldByIndex(WClass *wclass, unsigned short fieldIndex, WClass **vclass) {
	WClass *targetClass;
	unsigned short classIndex, nameAndTypeIndex;
	UtfString fieldName, fieldDesc;

	classIndex = CONS_classIndex(wclass, fieldIndex);
	targetClass = getClassByIndex(wclass, classIndex);
	if (targetClass == NULL)
		return NULL;
	nameAndTypeIndex = CONS_nameAndTypeIndex(wclass, fieldIndex);
	fieldName = getUtfString(wclass, CONS_nameIndex(wclass, nameAndTypeIndex));
	fieldDesc = getUtfString(wclass, CONS_typeIndex(wclass, nameAndTypeIndex));
	return getField(targetClass, fieldName, fieldDesc, vclass);
}

//
// Method Routines
//

// vclass is used to return the class the method was found in
// when the search is virtual (when a vclass is given)
WClassMethod *getMethod(WClass *wclass, UtfString name, UtfString desc, WClass **vclass) {
	WClassMethod *method;
	UtfString mname, mdesc;
	unsigned long i, n;

	n = wclass->numSuperClasses;
	while (1) {
		for (i = 0; i < wclass->numMethods; i++) {
			method = &wclass->methods[i];
			mname = getUtfString(wclass, METH_nameIndex(method));
			mdesc = getUtfString(wclass, METH_descIndex(method));
			if (name.len == mname.len &&
				desc.len == mdesc.len &&
				strncmp(name.str, mname.str, name.len) == 0 &&
				strncmp(desc.str, mdesc.str, desc.len) == 0) {
				if (vclass != NULL)
					*vclass = wclass;
				return method;
			}
		}

		if (vclass == NULL)
			break; // not a virtual lookup or no superclass

		if (n == 0)
			break;
		// look in superclass
		wclass = wclass->superClasses[--n];
	}

	return NULL;
}

// return 1 if two classes are compatible (if wclass is compatible
// with target). this function is not valid for checking to see if
// two arrays are compatible (see compatibleArray()).
// see page 135 of the book by Meyers and Downing for the basic algorithm
int compatible(WClass *source, WClass *target) {
	int targetIsInterface;
	unsigned long i, n;

	if (!source || !target){
		VmSetFatalErrorNum(ERR_ParamError);
		return 0; // source or target is array
	}
	targetIsInterface = 0;
	if (WCLASS_isInterface(target))
		targetIsInterface = 1;
	n = source->numSuperClasses;
	while (1) {
		if (targetIsInterface) {
			for (i = 0; i < WCLASS_numInterfaces(source); i++) {
				unsigned short classIndex;
				WClass *interfaceClass;

				classIndex = WCLASS_interfaceIndex(source, i);
				interfaceClass = getClassByIndex(source, classIndex);
				// NOTE: Either one of the interfaces in the source class can
				// equal the target interface or one of the interfaces
				// in the target interface (class) can equal one of the
				// interfaces in the source class for the two to be compatible
				if (interfaceClass == target)
					return 1;
				if (compatible(interfaceClass, target))
					return 1;
			}
		} else if (source == target)
			return 1;
		if (n == 0)
			break;
		// look in superclass
		source = source->superClasses[--n];
	}
	return 0;
}

int compatibleArray(WObject obj, UtfString arrayName) {
	WClass *wclass;

	wclass = WOBJ_class(obj);
	if (wclass != NULL){
 		return 0; // source is not array
	}
	if (arrayName.len <= 1 || arrayName.str[0] != '['){
		return 0; // target is not array
	}

	// NOTE: this isn't a full check to see if the arrays
	// are the same type. Any two arrays of objects (or other
	// arrays since they are objects) will test equal here.
	if (WOBJ_arrayType(obj) != arrayType(arrayName.str[1]))
		return 0;
	return 1;
}

long getMemInfo(T_MEMINFO *p_info)
{
	p_info->totalObjectMem = getTotalMemSize();
	p_info->unusedObjectMem = getUnusedMemSize();
	p_info->totalClassMem = classHeapSize;
	p_info->unusedClassMem = classHeapSize - classHeapUsed;
	p_info->vmStackSize = vmStackSize;
	p_info->vmStackPtr = vmStackPtr;
	p_info->nmStackSize = nmStackSize;
	p_info->nmStackPtr = nmStackPtr;

	return FT_ERR_OK;
}

//
// garbage collection
//
void gc(void) {
	WClass *wclass;
	WObject obj;
	unsigned long i, j;

	// mark objects on vm stack
	for (i = 0; i < vmStackPtr; i++)
		if (VALID_OBJ(vmStack[i].obj))
			markObject(vmStack[i].obj);

	// mark objects on native stack
	for (i = 0; i < nmStackPtr; i++)
		if (VALID_OBJ(nmStack[i]))
			markObject(nmStack[i]);

	// mark all static class objects
	for (i = 0; i < CLASS_HASH_SIZE; i++) {
		wclass = classHashList[i];
		while (wclass != NULL) {
			for (j = 0; j < wclass->numFields; j++) {
				WClassField *field;

				field = &wclass->fields[j];
				if (!FIELD_isStatic(field))
					continue;
				obj = field->var.staticVar.obj;
				if (VALID_OBJ(obj))
					markObject(obj);
			}
			wclass = wclass->nextClass;
		}
	}
	sweepObjects();
}

//
// Native Method Stack
//
int pushObject(WObject obj) {
	// prevent the pushed object from being freed by the garbage
	// collector. Used in native methods and with code calling
	// the VM. For example, if you have the following code
	//
	// obj1 = createObject(...);
	// obj2 = createObject(...);
	//
	// or..
	//
	// obj1 = createObject(...);
	// m = getMethod(..)
	//
	// since the second statement can cause a memory allocation
	// resulting in garbage collection (in the latter a class
	// load that allocates static class variables), obj1
	// would normally be freed. Pushing obj1 onto the "stack"
	// (which is a stack for this purpose) prevents that
	//
	// the code above should be change to:
	//
	// obj1 = createObject(...);
	// pushObject(obj1);
	// obj2 = createObject(...);
	// pushObject(obj2);
	// ..
	// if (popObject() != obj2)
	//   ..error..
	// if (popObject() != obj1)
	//   ..error..
	//

	// NOTE: Running out of Native Stack space can cause serious program
	// failure if any code doesn't check the return code of pushObject().
	// Any code that does a pushObject() should check for failure and if
	// failure occurs, then abort.

	if( obj == WOBJECT_NULL ){
		return FT_ERR_INVALID_PARAM;
	}

	if (nmStackPtr >= nmStackSize) {
		VmSetFatalErrorNum(ERR_NativeStackOverflow);
		return FT_ERR_NOTENOUGH;
	}
	nmStack[nmStackPtr++] = obj;
	return FT_ERR_OK;
}

WObject popObject(void) {
	if (nmStackPtr == 0) {
		VmSetFatalErrorNum(ERR_NativeStackUnderflow);
		return WOBJECT_NULL;
	}
	return nmStack[--nmStackPtr];
}

static NativeFunc getNativeMethod(WClass *wclass, UtfString methodName, UtfString methodDesc) {
	UtfString className;
	NativeMethod *nm;
	unsigned long hash, classHash, methodHash;
	unsigned short top, bot, mid;

	className = getUtfString(wclass, wclass->classNameIndex);
	classHash = genHashCode(className) % 65536;
	methodHash = (genHashCode(methodName) + genHashCode(methodDesc)) % 65536;
	hash = (classHash << 16) + methodHash;

	// binary search to find matching hash value
	top = 0;
	bot = (unsigned short)NumberOfNativeMethods;
	if (bot == 0)
		return NULL;
	while (1) {
		mid = (unsigned short)( (bot + top) / 2 );
		nm = &nativeMethods[mid];
		if (hash == nm->hash)
			return nm->func;
		if (mid == top)
			break; // not found
		if (hash < nm->hash)
			bot = mid;
		else
			top = mid;
	}
	VmSetFatalError(ERR_CantFindNative, &methodName, 1 );

#if 1
	{
	// for debug
	unsigned short i;

	debuglog("** Native Method Missing:\n");
	debuglog("// ");
	for (i = 0; i < className.len; i++)
		debuglog("%c", className.str[i]);
	debuglog("_");
	for (i = 0; i < methodName.len; i++)
		debuglog("%c", methodName.str[i]);
	debuglog("_");
	for (i = 0; i < methodDesc.len; i++)
		debuglog("%c", methodDesc.str[i]);
	debuglog("\n");
	debuglog("{ %uUL, func },\n", hash);
	}
#endif
	
	return NULL;
}

static ClassHook classHooks[] =
	{
//	{ "waba/io/Catalog", CatalogDestroy, 9 },
	{ NULL, NULL, 0 }
	};

// Hooks are used for objects that access system resources. Classes
// that are "hooked" may allocate extra variables so system resource
// pointers can be stored in the object. All hooked objects
// have an object destroy function that is called just before they
// are garbage collected allowing system resources to be deallocated.
static void setClassHooks(WClass *wclass) {
	UtfString className;
	ClassHook *hook;
	unsigned short i, nameLen;

	// NOTE: Like native methods, we could hash the hook class names into
	// a value if we make sure that the only time we'd check an object for
	// hooks is if it was in the waba package. This would make lookup
	// faster and take up less space. If the hook table is small, though,
	// it doesn't make much difference.
	className = getUtfString(wclass, wclass->classNameIndex);
	if (className.len < 6)
		return; // all hook classes are >= 6 character names
	i = 0;
	while (1) {
		hook = &classHooks[i++];
		if (hook->className == NULL)
			break;
		if (className.str[5] != hook->className[5])
			continue; // quick check to see if we should compare at all
		nameLen = (unsigned short)strlen(hook->className);
		if (className.len == nameLen &&
			strncmp(className.str, hook->className, nameLen) == 0) {
			wclass->objDestroyFunc = hook->destroyFunc;
			wclass->numVars += hook->varsNeeded;
			return;
		}
	}
}

/*
 "Thirty spokes join at the hub;
  their use for the cart is where they are not.
  When the potter's wheel makes a pot,
  the use of the pot is precisely where there is nothing.
  When you open doors and windows for a room,
  it is where there is nothing that they are useful to the room.
  Therefore being is for benefit,
  Nonbeing is for usefulness."
*/

//
// Method Execution
//

//
// This is the interpreter code. Each method call pushes a frame on the
// stack. The structure of a stack frame is:
//
// local var 1
// local var 2
// local var N
// local stack 1
// local stack 2
// local stack N
//
// when a method is called, the following is pushed on the stack before
// the next frame:
//
// method
// wclass
// stack(base)
// pc
// var
// stack
//
// NOTE: You can, of course, increase the performance of the interpreter
// in a number of ways. I've found a good book on assembly optimization
// to be:
//
// Inner Loops - A sourcebook for fast 32-bit software development
// by Rick Booth
//
long executeMethod(WClass *wclass, WClassMethod *method, Var params[], unsigned short numParams, unsigned char *retType, Var* retValue) {
	Var *var;
	Var *stack;
	unsigned char *pc;
	unsigned long baseFramePtr;
	WClass *curwclass;
	WClassMethod *curmethod;
	UtfString utf;

	// for internal use
	WObject obj;
	long i;
	Var *objPtr;
	long ret;

	// the variables wclass, method, var, stack, and pc need to be
	// pushed and restored when calling methods using "goto methoinvoke"

	// also note that this method does recurse when we hit static class
	// initializers which execute within a class load operation and this
	// is why we exit when we keep trace of the baseFramePtr.

	*retType = RET_TYPE_NONE;
	baseFramePtr = vmStackPtr;

	curwclass = wclass;
	curmethod = method;

	debuglog("[Call] executeMethod\n");
	utf = getUtfString(wclass, wclass->classNameIndex);
	debuglog("\tclassName: %s\n", UtfToStaticUChars(utf));
	utf = getUtfString(wclass, METH_nameIndex(method));
	debuglog("\tmethodName: %s\n", UtfToStaticUChars(utf));


	if (METH_isNative(curmethod)) {
		if (curmethod->code.nativeFunc == NULL)
			goto bad_class_code_fatal_error;
		if (vmStackPtr + numParams + 1 + 3 >= vmStackSize)
			goto stack_overflow_fatal_error;
	} else {
		if (curmethod->code.codeAttr == NULL)
			goto bad_class_code_fatal_error;  // method has no code code attribute - compiler is broken
		if (vmStackPtr + METH_maxLocals(curmethod) + METH_maxStack(curmethod) + 3 >= vmStackSize)
			goto stack_overflow_fatal_error;
	}

	// push params into local vars of frame
	for( i = 0 ; i < numParams ; i++ )
		vmStack[vmStackPtr + i] = params[i];

method_invoke:
	// execute native method
	if (METH_isNative(curmethod)) {
		// the active frame for a native method is:
		//
		// instance (if not static)
		// param 1
		// ...
		// param N
		// num params
		// stack(base)
		// method pointer
		// class pointer
		stack = &vmStack[vmStackPtr];
		vmStackPtr += numParams;
		vmStack[vmStackPtr++].intValue = numParams;
		vmStack[vmStackPtr++].refValue = stack;
		vmStack[vmStackPtr++].refValue = curmethod;
		vmStack[vmStackPtr++].refValue = curwclass;

		ret = curmethod->code.nativeFunc( stack );
		if( ret == 0 ){
			if (curmethod->returnsValue){
				*retValue = stack[0];
				*retType = RET_TYPE_RETURN;
			}else{
				*retType = RET_TYPE_NONE;
			}
		}else if( ret > 0 ){
			retValue->obj = CreateRuntimeException((unsigned short)ret);
			*retType = RET_TYPE_EXCEPTION;
		}else{
			VmSetFatalErrorNum(ERR_NativeErrorReturn);
			goto fatal_error;
		}

		goto method_return;
	}

	// push active stack frame:
	//
	// local var 1
	// ...
	// local var N
	// local stack 1
	// ...
	// local stack N
	// stack(base)
	// method pointer
	// class pointer
	var = &vmStack[vmStackPtr];
	vmStackPtr += METH_maxLocals(curmethod);
	stack = &vmStack[vmStackPtr];
	vmStackPtr += METH_maxStack(curmethod);
	vmStack[vmStackPtr++].refValue = stack;
	vmStack[vmStackPtr++].refValue = curmethod;
	vmStack[vmStackPtr++].refValue = curwclass;
	pc = METH_code(curmethod);

step:
	if( vmStatus.type == TYPE_FATAL_ERROR )
		goto method_return;

	if(*retType == RET_TYPE_EXCEPTION) {
		for( i = 0 ; i < curmethod->numHandlers; i++ ){
			WClassHandler* handler = &(curmethod->handlers)[i];
			if( ( METH_code( curmethod ) + handler->start_pc <= pc ) && ( pc <= METH_code( curmethod ) + handler->end_pc ) ){
				if( compatible( WOBJ_class( retValue->obj ), getClassByIndex( curwclass, handler->catch_type ) ) ){
					pc = METH_code( curmethod ) + handler->handler_pc;
					// reset stack(base)
					stack = (Var *)vmStack[vmStackPtr - 3].refValue;
					stack[0] = *retValue;
					stack++;
					*retType = RET_TYPE_NONE;
					goto step;
				}
			}
		}
		goto method_return;

	}
	switch (*pc) {
		case OP_nop:
			pc++;
			break;
		case OP_aconst_null:
			stack[0].obj = WOBJECT_NULL;
			stack++;
			pc++;
			break;
		case OP_iconst_m1:
		case OP_iconst_0:
		case OP_iconst_1:
		case OP_iconst_2:
		case OP_iconst_3:
		case OP_iconst_4:
		case OP_iconst_5:
			// NOTE: testing shows there is no real performance gain to
			// splitting these out into seperate case statements
			stack[0].intValue = (((long)(*pc)) - OP_iconst_0);
			stack++;
			pc++;
			break;
		case OP_bipush:
			stack[0].intValue = ((char *)pc)[1];
			stack++;
			pc += 2;
			break;
		case OP_sipush:
			stack[0].intValue = utils_get_int16b(&pc[1]);
			stack++;
			pc += 3;
			break;
		case OP_ldc:
			*stack = constantToVar(curwclass, (unsigned short)pc[1]);
			stack++;
			pc += 2;
			break;
		case OP_ldc_w:
			*stack = constantToVar(curwclass, utils_get_uint16b(&pc[1]));
			stack++;
			pc += 3;
			break;
		case OP_iload:
		case OP_aload:
			*stack = var[pc[1]];
			stack++;
			pc += 2;
			break;
		case OP_iload_0:
		case OP_iload_1:
		case OP_iload_2:
		case OP_iload_3:
			*stack = var[*pc - OP_iload_0];
			stack++;
			pc++;
			break;
		case OP_aload_0:
		case OP_aload_1:
		case OP_aload_2:
		case OP_aload_3:
			*stack = var[*pc - OP_aload_0];
			stack++;
			pc++;
			break;
		case OP_iaload:
			obj = stack[-2].obj;
			i = stack[-1].intValue;
			if (obj == WOBJECT_NULL) goto null_array_error;
			objPtr = objectPtr(obj);
			if (i < 0 || i >= WOBJ_arrayLenP(objPtr)) goto index_range_error;
			stack[-2].intValue = ((long *)WOBJ_arrayStartP(objPtr))[i];
			stack--;
			pc++;
			break;
		case OP_saload:
			obj = stack[-2].obj;
			i = stack[-1].intValue;
			if (obj == WOBJECT_NULL) goto null_array_error;
			objPtr = objectPtr(obj);
			if (i < 0 || i >= WOBJ_arrayLenP(objPtr)) goto index_range_error;
			stack[-2].intValue = (long)(((short *)WOBJ_arrayStartP(objPtr))[i]);
			stack--;
			pc++;
			break;
		case OP_aaload:
			obj = stack[-2].obj;
			i = stack[-1].intValue;
			if (obj == WOBJECT_NULL) goto null_array_error;
			objPtr = objectPtr(obj);
			if (i < 0 || i >= WOBJ_arrayLenP(objPtr)) goto index_range_error;
			stack[-2].obj = ((WObject *)WOBJ_arrayStartP(objPtr))[i];
			stack--;
			pc++;
			break;
		case OP_baload:
			obj = stack[-2].obj;
			i = stack[-1].intValue;
			if (obj == WOBJECT_NULL) goto null_array_error;
			objPtr = objectPtr(obj);
			if (i < 0 || i >= WOBJ_arrayLenP(objPtr)) goto index_range_error;
			stack[-2].intValue = (long)(((char *)WOBJ_arrayStartP(objPtr))[i]);
			stack--;
			pc++;
			break;
		case OP_caload:
			obj = stack[-2].obj;
			i = stack[-1].intValue;
			if (obj == WOBJECT_NULL) goto null_array_error;
			objPtr = objectPtr(obj);
			if (i < 0 || i >= WOBJ_arrayLenP(objPtr)) goto index_range_error;
			stack[-2].intValue = (long)(((unsigned short *)WOBJ_arrayStartP(objPtr))[i]);
			stack--;
			pc++;
			break;
		case OP_astore:
		case OP_istore:
			stack--;
			var[pc[1]] = *stack;
			pc += 2;
			break;
		case OP_istore_0:
		case OP_istore_1:
		case OP_istore_2:
		case OP_istore_3:
			stack--;
			var[*pc - OP_istore_0] = *stack;
			pc++;
			break;
		case OP_astore_0:
		case OP_astore_1:
		case OP_astore_2:
		case OP_astore_3:
			stack--;
			var[*pc - OP_astore_0] = *stack;
			pc++;
			break;
		case OP_iastore:
			obj = stack[-3].obj;
			i = stack[-2].intValue;
			if (obj == WOBJECT_NULL) goto null_array_error;
			objPtr = objectPtr(obj);
			if (i < 0 || i >= WOBJ_arrayLenP(objPtr)) goto index_range_error;
			((long *)WOBJ_arrayStartP(objPtr))[i] = stack[-1].intValue;
			stack -= 3;
			pc++;
			break;
		case OP_sastore:
			obj = stack[-3].obj;
			i = stack[-2].intValue;
			if (obj == WOBJECT_NULL) goto null_array_error;
			objPtr = objectPtr(obj);
			if (i < 0 || i >= WOBJ_arrayLenP(objPtr)) goto index_range_error;
			((short *)WOBJ_arrayStartP(objPtr))[i] = (short)stack[-1].intValue;
			stack -= 3;
			pc++;
			break;
		case OP_aastore:
			obj = stack[-3].obj;
			i = stack[-2].intValue;
			if (obj == WOBJECT_NULL) goto null_array_error;
			objPtr = objectPtr(obj);
			if (i < 0 || i >= WOBJ_arrayLenP(objPtr)) goto index_range_error;
			if( WOBJ_arrayType( obj ) != TYPE_OBJECT )
				goto array_store_error;
			((WObject *)WOBJ_arrayStartP(objPtr))[i] = stack[-1].obj;
			stack -= 3;
			pc++;
			break;
		case OP_bastore:
			obj = stack[-3].obj;
			i = stack[-2].intValue;
			if (obj == WOBJECT_NULL) goto null_array_error;
			objPtr = objectPtr(obj);
			if (i < 0 || i >= WOBJ_arrayLenP(objPtr)) goto index_range_error;
			((char *)WOBJ_arrayStartP(objPtr))[i] = (char)stack[-1].intValue;
			stack -= 3;
			pc++;
			break;
		case OP_castore:
			obj = stack[-3].obj;
			i = stack[-2].intValue;
			if (obj == WOBJECT_NULL) goto null_array_error;
			objPtr = objectPtr(obj);
			if (i < 0 || i >= WOBJ_arrayLenP(objPtr)) goto index_range_error;
			((unsigned short *)WOBJ_arrayStartP(objPtr))[i] = (unsigned short)stack[-1].intValue;
			stack -= 3;
			pc++;
			break;
		case OP_pop:
			stack--;
			pc++;
			break;
		case OP_pop2:
			stack -= 2;
			pc++;
			break;
		case OP_dup:
			stack[0] = stack[-1];
			stack++;
			pc++;
			break;
		case OP_dup_x1:
			stack[0] = stack[-1];
			stack[-1] = stack[-2];
			stack[-2] = stack[0];
			stack++;
			pc++;
			break;
		case OP_dup_x2:
			stack[0] = stack[-1];
			stack[-1] = stack[-2];
			stack[-2] = stack[-3];
			stack[-3] = stack[0];
			stack++;
			pc++;
			break;
		case OP_dup2:
			stack[1] = stack[-1];
			stack[0] = stack[-2];
			stack += 2;
			pc++;
			break;
		case OP_dup2_x1:
			stack[1] = stack[-1];
			stack[0] = stack[-2];
			stack[-1] = stack[-3];
			stack[-2] = stack[1];
			stack[-3] = stack[0];
			stack += 2;
			pc++;
			break;
		case OP_dup2_x2:
			stack[1] = stack[-1];
			stack[0] = stack[-2];
			stack[-1] = stack[-3];
			stack[-2] = stack[-4];
			stack[-3] = stack[1];
			stack[-4] = stack[0];
			stack += 2;
			pc++;
			break;
		case OP_swap:
			{
			Var v;

			v = stack[-2];
			stack[-2] = stack[-1];
			stack[-1] = v;
			pc++;
			break;
			}
		case OP_iadd:
			stack[-2].intValue += stack[-1].intValue;
			stack--;
			pc++;
			break;
		case OP_isub:
			stack[-2].intValue -= stack[-1].intValue;
			stack--;
			pc++;
			break;
		case OP_imul:
			stack[-2].intValue *= stack[-1].intValue;
			stack--;
			pc++;
			break;
		case OP_idiv:
			if (stack[-1].intValue == 0)
				goto div_by_zero_error;
			stack[-2].intValue /= stack[-1].intValue;
			stack--;
			pc++;
			break;
		case OP_irem:
			if (stack[-1].intValue == 0)
				goto div_by_zero_error;
			stack[-2].intValue = stack[-2].intValue % stack[-1].intValue;
			stack--;
			pc++;
			break;
		case OP_ineg:
			stack[-1].intValue = - stack[-1].intValue;
			pc++;
			break;
		case OP_ishl:
			stack[-2].intValue = stack[-2].intValue << stack[-1].intValue;
			stack--;
			pc++;
			break;
		case OP_ishr:
			stack[-2].intValue = stack[-2].intValue >> stack[-1].intValue;
			stack--;
			pc++;
			break;
		case OP_iushr:
			i = stack[-1].intValue;
			if (stack[-2].intValue >= 0)
				stack[-2].intValue = stack[-2].intValue >> i;
			else {
				stack[-2].intValue = stack[-2].intValue >> i;
				if (i >= 0)
					stack[-2].intValue += (long)2 << (31 - i);
				else
					stack[-2].intValue += (long)2 << ((- i) + 1);
			}
			stack--;
			pc++;
			break;
		case OP_iand:
			stack[-2].intValue &= stack[-1].intValue;
			stack--;
			pc++;
			break;
		case OP_ior:
			stack[-2].intValue |= stack[-1].intValue;
			stack--;
			pc++;
			break;
		case OP_ixor:
			stack[-2].intValue ^= stack[-1].intValue;
			stack--;
			pc++;
			break;
		case OP_iinc:
			var[pc[1]].intValue += (char)pc[2];
			pc += 3;
			break;
		case OP_i2b:
			stack[-1].intValue = (long)((char)(stack[-1].intValue & 0xFF));
			pc++;
			break;
		case OP_i2c:
			stack[-1].intValue = (long)((unsigned short)(stack[-1].intValue & 0xFFFF));
			pc++;
			break;
		case OP_i2s:
			stack[-1].intValue = (long)((short)(stack[-1].intValue & 0xFFFF));
			pc++;
			break;
		case OP_ifeq:
			if (stack[-1].intValue == 0)
				pc += utils_get_int16b(&pc[1]);
			else
				pc += 3;
			stack--;
			break;
		case OP_ifne:
			if (stack[-1].intValue != 0)
				pc += utils_get_int16b(&pc[1]);
			else
				pc += 3;
			stack--;
			break;
		case OP_iflt:
			if (stack[-1].intValue < 0)
				pc += utils_get_int16b(&pc[1]);
			else
				pc += 3;
			stack--;
			break;
		case OP_ifge:
			if (stack[-1].intValue >= 0)
				pc += utils_get_int16b(&pc[1]);
			else
				pc += 3;
			stack--;
			break;
		case OP_ifgt:
			if (stack[-1].intValue > 0)
				pc += utils_get_int16b(&pc[1]);
			else
				pc += 3;
			stack--;
			break;
		case OP_ifle:
			if (stack[-1].intValue <= 0)
				pc += utils_get_int16b(&pc[1]);
			else
				pc += 3;
			stack--;
			break;
		case OP_if_icmpeq:
			if (stack[-2].intValue == stack[-1].intValue)
				pc += utils_get_int16b(&pc[1]);
			else
				pc += 3;
			stack -= 2;
			break;
		case OP_if_icmpne:
			if (stack[-2].intValue != stack[-1].intValue)
				pc += utils_get_int16b(&pc[1]);
			else
				pc += 3;
			stack -= 2;
			break;
		case OP_if_icmplt:
			if (stack[-2].intValue < stack[-1].intValue)
				pc += utils_get_int16b(&pc[1]);
			else
				pc += 3;
			stack -= 2;
			break;
		case OP_if_icmpge:
			if (stack[-2].intValue >= stack[-1].intValue)
				pc += utils_get_int16b(&pc[1]);
			else
				pc += 3;
			stack -= 2;
			break;
		case OP_if_icmpgt:
			if (stack[-2].intValue > stack[-1].intValue)
				pc += utils_get_int16b(&pc[1]);
			else
				pc += 3;
			stack -= 2;
			break;
		case OP_if_icmple:
			if (stack[-2].intValue <= stack[-1].intValue)
			pc += utils_get_int16b(&pc[1]);
			else
				pc += 3;
			stack -= 2;
			break;
		case OP_if_acmpeq:
			if (stack[-2].obj == stack[-1].obj)
				pc += utils_get_int16b(&pc[1]);
			else
				pc += 3;
			stack -= 2;
			break;
		case OP_if_acmpne:
			if (stack[-2].obj != stack[-1].obj)
				pc += utils_get_int16b(&pc[1]);
			else
				pc += 3;
			stack -= 2;
			break;
		case OP_goto:
			pc += utils_get_int16b(&pc[1]);
			break;
		case OP_jsr:
			stack[0].pc = pc + 3;
			stack++;
			pc += utils_get_int16b(&pc[1]);
			break;
		case OP_ret:
			pc = var[pc[1]].pc;
			break;
		case OP_tableswitch:
			{
			long key, low, high, defaultOff;
			unsigned char *npc;

			key = stack[-1].intValue;
			npc = pc + 1;
			npc += (4 - ((npc - METH_code(curmethod)) % 4)) % 4;
			defaultOff = utils_get_int32b(npc);
			npc += 4;
			low = utils_get_int32b(npc);
			npc += 4;
			high = utils_get_int32b(npc);
			npc += 4;
			if (key < low || key > high)
				pc += defaultOff;
			else
				pc += utils_get_int32b(&npc[(key - low) * 4]);
			stack--;
			break;
			}
		case OP_lookupswitch:
			{
			long key, low, mid, high, npairs, defaultOff;
			unsigned char *npc;

			key = stack[-1].intValue;
			npc = pc + 1; 
//			npc += (4 - ((npc - METH_code(method)) % 4)) % 4;
			npc += (4 - ((npc - METH_code(curmethod)) % 4)) % 4;
			defaultOff = utils_get_int32b(npc);
			npc += 4;
			npairs = utils_get_int32b(npc);
			npc += 4;

			// binary search
			if (npairs > 0) {
				low = 0;
				high = npairs;
				while (1) {
					mid = (high + low) / 2;
					i = utils_get_int32b(npc + (mid * 8));
					if (key == i) {
						pc += utils_get_int32b(npc + (mid * 8) + 4); // found
						break;	
					}
					if (mid == low) {
						pc += defaultOff; // not found
						break;
					}
					if (key < i)
						high = mid;
					else
						low = mid;
				}
			} else
				pc += defaultOff; // no pairs
			stack--;
			break;
			}
		case OP_ireturn:
		case OP_areturn:
			*retValue = stack[-1];
			*retType = RET_TYPE_RETURN;
			goto method_return;
		case OP_return:
			*retType = RET_TYPE_NONE;
			goto method_return;
		case OP_getfield:
			{
			WClassField *field;

			field = getFieldByIndex(curwclass, utils_get_uint16b(&pc[1]), NULL);
			if (field == NULL)
				goto fatal_error;
			obj = stack[-1].obj;
			if (obj == WOBJECT_NULL)
				goto null_obj_error;
			stack[-1] = WOBJ_var(obj, field->var.varOffset);
			pc += 3;
			break;
			}
		case OP_putfield:
			{
			WClassField *field;

			field = getFieldByIndex(curwclass, utils_get_uint16b(&pc[1]), NULL);
			if (field == NULL)
				goto fatal_error;
			obj = stack[-2].obj;
			if (obj == WOBJECT_NULL)
				goto null_obj_error;
			WOBJ_var(obj, field->var.varOffset) = stack[-1];
			stack -= 2;
			pc += 3;
			break;
			}
		case OP_getstatic:
			{
			WClassField *field;
			WClass *vclass;

			field = getFieldByIndex(curwclass, utils_get_uint16b(&pc[1]), &vclass);
			if (field == NULL)
				goto fatal_error;
			stack[0] = field->var.staticVar;
			stack++;
			pc += 3;
			break;
			}
		case OP_putstatic:
			{
			WClassField *field;
			WClass *vclass;

			field = getFieldByIndex(curwclass, utils_get_uint16b(&pc[1]), &vclass);
			if (field == NULL)
				goto fatal_error;
			field->var.staticVar = stack[-1];
			stack--;
			pc += 3;
			break;
			}
		case OP_new:
			{
			unsigned short classIndex;

			classIndex = utils_get_uint16b(&pc[1]);
			stack[0].obj = createObject(getClassByIndex(curwclass, classIndex));
			if( stack[0].obj == WOBJECT_NULL )
				goto out_of_objectmem_fatal_error;
			stack++;
			pc += 3;
			break;
			}
		case OP_newarray:
			if( stack[-1].intValue < 0 )
				goto negative_array_size_error;
			stack[-1].obj = createArrayObject(pc[1], stack[-1].intValue);
			if( stack[-1].obj == WOBJECT_NULL )
				goto out_of_objectmem_fatal_error;
			pc += 2;
			break;
		case OP_anewarray:
			if( stack[-1].intValue < 0 )
				goto negative_array_size_error;
			stack[-1].obj = createArrayObject(TYPE_OBJECT, stack[-1].intValue);
			if( stack[-1].obj == WOBJECT_NULL )
				goto out_of_objectmem_fatal_error;
			pc += 3;
			break;
		case OP_arraylength:
			obj = stack[-1].obj;
			if (obj == WOBJECT_NULL)
				goto null_array_error;
			stack[-1].intValue = WOBJ_arrayLen(obj);
			pc++;
			break;
		case OP_instanceof:
		case OP_checkcast:
			{
			unsigned short classIndex;
			UtfString className;
			WClass *source, *target;
			int comp;

			obj = stack[-1].obj;
			if (obj == WOBJECT_NULL) {
				if (*pc == OP_instanceof)
					stack[-1].intValue = 0;
				pc += 3;
				break;
			}
			source = WOBJ_class(obj);
			classIndex = utils_get_uint16b(&pc[1]);
			target = getClassByIndex(curwclass, classIndex);
			if (target != NULL) {
				className = getUtfString(target, target->classNameIndex);
				comp = compatible(source, target); // target is not array
			} else {
				// target is either array or target class was not found
				// if either of these cases is true, the index couldn't be
				// bound to a pointer, so we still have a pointer into the
				// constant pool and can use the string reference in the constant
				className = getUtfString(curwclass, CONS_nameIndex(curwclass, classIndex));
				comp = compatibleArray(obj, className); // target is array
			}
			if (*pc == OP_checkcast) {
				if (comp == 0)
					goto class_cast_error;
			} else {
				stack[-1].intValue = comp;
			}
			pc += 3;
			break;
			}
		case OP_wide:
			pc++;
			switch (*pc) {
				case OP_iload:
				case OP_aload:
					stack[0] = var[utils_get_uint16b(&pc[1])];
					stack++;
					pc += 3;
					break;
				case OP_astore:
				case OP_istore:
					var[utils_get_uint16b(&pc[1])] = stack[-1];
					stack--;
					pc += 3;
					break;
				case OP_iinc:
					var[utils_get_uint16b(&pc[1])].intValue += utils_get_int16b(&pc[3]);
					pc += 5;
					break;
				case OP_ret:
					pc = var[utils_get_uint16b(&pc[1])].pc;
					break;
			}
			break;
		case OP_multianewarray:
			{
			unsigned short classIndex;
			UtfString className;
			long ndim;
			char *cstr;

			classIndex = utils_get_uint16b(&pc[1]);
			// since arrays do not have associated classes which could be bound
			// to the class constant, we can safely access the name string in
			// the constant
			className = getUtfString(curwclass, CONS_nameIndex(curwclass, classIndex));
			ndim = (long)pc[3];
			cstr = &className.str[1];
			stack -= ndim;
			stack[0].obj = createMultiArray(ndim, cstr, stack);
			if( stack[0].obj == WOBJECT_NULL )
				goto out_of_objectmem_fatal_error;
			stack++;
			pc += 4;
			break;
			}
		case OP_ifnull:
			if (stack[-1].obj == WOBJECT_NULL)
				pc += utils_get_int16b(&pc[1]);
			else
				pc += 3;
			stack--;
			break;
		case OP_ifnonnull:
			if (stack[-1].obj != WOBJECT_NULL)
				pc += utils_get_int16b(&pc[1]);
			else
				pc += 3;
			stack--;
			break;
		case OP_goto_w:
			pc += utils_get_int32b(&pc[1]);
			break;
		case OP_jsr_w:
			stack[0].pc = pc + 5;
			pc += utils_get_int32b(&pc[1]);
			stack++;
			break;
		case OP_monitorenter: // unsupported
			stack--;
			pc++;
			break;
		case OP_monitorexit: // unsupported
			stack--;
			pc++;
			break;
		case OP_athrow:
			pc++;
			*retValue = stack[-1];
			*retType = RET_TYPE_EXCEPTION;
			break;
		case OP_invokeinterface:
		case OP_invokestatic:
		case OP_invokevirtual:
		case OP_invokespecial:
			{
			unsigned short iparams, classIndex, methodIndex, nameAndTypeIndex;
			WClass *iclass;
			WClassMethod *imethod;
			UtfString methodName, methodDesc;

			methodName.len = 0;
			methodDesc.len = 0;
			methodIndex = utils_get_uint16b(&pc[1]);
			classIndex = CONS_classIndex(curwclass, methodIndex);
			iclass = getClassByIndex(curwclass, classIndex);
			if (iclass == NULL)
				goto method_fatal_error;
			nameAndTypeIndex = CONS_nameAndTypeIndex(curwclass, methodIndex);
			methodName = getUtfString(curwclass, CONS_nameIndex(curwclass, nameAndTypeIndex));
			methodDesc = getUtfString(curwclass, CONS_typeIndex(curwclass, nameAndTypeIndex));

#if 0
			debuglog("[invoke]\n");
			utf = getUtfString(iclass, iclass->classNameIndex);
			debuglog("\tclassName: %s\n", UtfToStaticUChars(utf));
			debuglog("\tmethodName: %s\n", UtfToStaticUChars(methodName));
			if (strcmp("getInstance", UtfToStaticUChars(methodName)) == 0)
				debuglog("match\n");
#endif

			if (*pc == OP_invokevirtual)
			{
				pc += 3;

				imethod = getMethod(iclass, methodName, methodDesc, &iclass);
				if (imethod == NULL)
					goto method_fatal_error;
				iparams = imethod->numParams + 1;
				obj = stack[-(long)iparams].obj;
				if (obj == WOBJECT_NULL)
					goto null_obj_error;

				// get method (and class if virtual)
				imethod = getMethod((WClass *)WOBJ_class(obj), methodName, methodDesc, &iclass);
				if (imethod == NULL)
					goto method_fatal_error; //classes are out of sync/corrupt
			}
			else if (*pc == OP_invokeinterface )
			{
				iparams = pc[3];
				pc += 5;

				obj = stack[-(long)iparams].obj;
				if (obj == WOBJECT_NULL)
					goto null_obj_error;

				// get method (and class if virtual)
				imethod = getMethod((WClass *)WOBJ_class(obj), methodName, methodDesc, &iclass);
				if (imethod == NULL)
					goto method_fatal_error; //classes are out of sync/corrupt
			}
			else if (*pc == OP_invokespecial)
			{
				pc += 3;

				imethod = getMethod(iclass, methodName, methodDesc, &iclass);
				if (imethod == NULL)
					goto method_fatal_error;

				iparams = imethod->numParams + 1;
				obj = stack[-(long)iparams].obj;
				if (obj == WOBJECT_NULL)
					goto null_obj_error;

				if (iclass->numSuperClasses == 0 && method->isInit) {
					stack -= iparams;
					break;
				}
			}
			else{	/* OP_invokestatic */
				pc += 3;

				imethod = getMethod(iclass, methodName, methodDesc, &iclass);
				if (imethod == NULL)
					goto method_fatal_error;
				iparams = imethod->numParams;
			}

			// push return stack frame:
			//
			// program counter pointer
			// local var pointer
			// local stack pointer

			if (METH_isNative(imethod)) {
				if (imethod->code.nativeFunc == NULL)
					goto method_fatal_error;
				// return stack frame plus native method active frame
				if (vmStackPtr + 3 + iparams + 1 + 3 >= vmStackSize)
					goto stack_overflow_fatal_error;
			} else {
				if (imethod->code.codeAttr == NULL)
					goto method_fatal_error;
				// return stack frame plus active frame
				if (vmStackPtr + 3 + METH_maxLocals(imethod) + METH_maxStack(imethod) + 3 >= vmStackSize)
					goto stack_overflow_fatal_error;
			}

			vmStack[vmStackPtr++].pc = pc;
			vmStack[vmStackPtr++].refValue = var;
			vmStack[vmStackPtr++].refValue = stack - iparams;

			// for next method invocation
			// push params into local vars of next frame
			for (i = 0; i < iparams; i++) {
				vmStack[vmStackPtr + (iparams - 1) - i] = stack[-1];
				stack--;
			}

			// switch next invocation
			curwclass = iclass;
			curmethod = imethod;
			numParams = iparams;

			goto method_invoke;

method_fatal_error:
			{
				UtfString utfs[3];
				utfs[0].len = utfs[1].len = utfs[2].len = 0;
				int num = 0;
				if( iclass != NULL )
					utfs[num++] = getUtfString(iclass, iclass->classNameIndex);
				if( methodName.len != 0 )
					utfs[num++] = methodName;
				if( methodDesc.len != 0 )
					utfs[num++] = methodDesc;
				VmSetFatalError(ERR_CantFindMethod, utfs, num );
			}
			goto fatal_error;
			}
		// NOTE: this is the full list of unsupported opcodes. Adding all
		// these cases here does not cause the VM executable code to be any
		// larger, it just makes sure that the compiler uses a jump table
		// with no spaces in it to make sure performance is as good as we
		// can get (tested under Codewarrior for PalmOS).
/*
		case OP_lconst_0:
		case OP_lconst_1:
		case OP_dconst_0:
		case OP_dconst_1:
		case OP_ldc2_w:
		case OP_lload:
		case OP_dload:
		case OP_lload_0:
		case OP_lload_1:
		case OP_lload_2:
		case OP_lload_3:
		case OP_dload_0:
		case OP_dload_1:
		case OP_dload_2:
		case OP_dload_3:
		case OP_laload:
		case OP_daload:
		case OP_lstore:
		case OP_dstore:
		case OP_lstore_0:
		case OP_lstore_1:
		case OP_lstore_2:
		case OP_lstore_3:
		case OP_dstore_0:
		case OP_dstore_1:
		case OP_dstore_2:
		case OP_dstore_3:
		case OP_lastore:
		case OP_dastore:
		case OP_ladd:
		case OP_dadd:
		case OP_lsub:
		case OP_dsub:
		case OP_lmul:
		case OP_dmul:
		case OP_ldiv:
		case OP_ddiv:
		case OP_lrem:
		case OP_drem:
		case OP_lneg:
		case OP_dneg:
		case OP_lshl:
		case OP_lshr:
		case OP_lushr:
		case OP_land:
		case OP_lor:
		case OP_lxor:
		case OP_i2l:
		case OP_i2d:
		case OP_l2i:
		case OP_l2f:
		case OP_l2d:
		case OP_f2l:
		case OP_f2d:
		case OP_d2i:
		case OP_d2l:
		case OP_d2f:
		case OP_lcmp:
		case OP_dcmpl:
		case OP_dcmpg:
		case OP_lreturn:
		case OP_dreturn:
		case OP_fconst_0:
		case OP_fconst_1:
		case OP_fconst_2:
		case OP_faload:
		case OP_fastore:
		case OP_fload:
		case OP_fload_0:
		case OP_fload_1:
		case OP_fload_2:
		case OP_fload_3:
		case OP_fstore:
		case OP_fstore_0:
		case OP_fstore_1:
		case OP_fstore_2:
		case OP_fstore_3:
		case OP_fadd:
		case OP_fsub:
		case OP_fmul:
		case OP_fdiv:
		case OP_frem:
		case OP_fneg:
		case OP_i2f:
		case OP_f2i:
		case OP_fcmpl:
		case OP_fcmpg:
		case OP_freturn:
*/
		default:
			VmSetFatalErrorNum(ERR_BadOpcode);
			goto fatal_error;
		}
	goto step;

array_store_error:
	retValue->obj = CreateRuntimeException(ERR_ArrayStoreException);
	*retType = RET_TYPE_EXCEPTION;
	goto step;
negative_array_size_error:
	retValue->obj = CreateRuntimeException(ERR_NegativeArraySize);
	*retType = RET_TYPE_EXCEPTION;
	goto step;
null_obj_error:
	retValue->obj = CreateRuntimeException(ERR_NullObjectAccess);
	*retType = RET_TYPE_EXCEPTION;
	goto step;
div_by_zero_error:
	retValue->obj = CreateRuntimeException(ERR_DivideByZero);
	*retType = RET_TYPE_EXCEPTION;
	goto step;
index_range_error:
	retValue->obj = CreateRuntimeException(ERR_IndexOutOfRange);
	*retType = RET_TYPE_EXCEPTION;
	goto step;
null_array_error:
	retValue->obj = CreateRuntimeException(ERR_NullArrayAccess);
	*retType = RET_TYPE_EXCEPTION;
	goto step;
class_cast_error:
	retValue->obj = CreateRuntimeException(ERR_ClassCastException);
	*retType = RET_TYPE_EXCEPTION;
	goto step;

out_of_objectmem_fatal_error:
	VmSetFatalErrorNum(ERR_OutOfObjectMem);
	goto fatal_error;
stack_overflow_fatal_error:
	VmSetFatalErrorNum(ERR_StackOverflow);
	goto fatal_error;
bad_class_code_fatal_error:
	VmSetFatalErrorNum(ERR_BadClassCode);
	goto fatal_error;

fatal_error:
	vmStackPtr = baseFramePtr;
	return FT_ERR_FAILED;

method_return:
	// pop frame and restore state
	// skip stack(base), method and class
	vmStackPtr -= 3;
	if (METH_isNative(curmethod))
		vmStackPtr -= vmStack[--vmStackPtr].intValue;
	else
		vmStackPtr -= METH_maxLocals(curmethod) + METH_maxStack(curmethod);

	if (vmStackPtr > baseFramePtr){
		stack = (Var *)vmStack[--vmStackPtr].refValue;
		if (*retType == RET_TYPE_EXCEPTION || *retType == RET_TYPE_RETURN ){
			stack[0] = *retValue;
			stack++;
		}
		var = (Var *)vmStack[--vmStackPtr].refValue;
		pc = vmStack[--vmStackPtr].pc;
		curwclass = (WClass *)vmStack[vmStackPtr - 1].refValue;
		curmethod = (WClassMethod *)vmStack[vmStackPtr - 2].refValue;

		goto step;
	}else if (vmStackPtr == baseFramePtr) {
		// fully completed execution
//		if (*retType == RET_TYPE_EXCEPTION)
//			return 1;
		return FT_ERR_OK;
	} else{
		// vmStackPtr < baseFramePtr
		goto stack_overflow_fatal_error;
	}
}

WObject CreateRuntimeException(unsigned short errNum) {
	UtfString exceptName;
	Var retVar;
	unsigned char retType;
	long ret;

	switch( errNum ){
		case ERR_NullObjectAccess:
		case ERR_NullArrayAccess:
			exceptName = createUtfString( "java/lang/NullPointerException" );
			break;
		case ERR_IndexOutOfRange:
			exceptName = createUtfString( "java/lang/ArrayIndexOutOfBoundsException" );
			break;
		case ERR_DivideByZero:
			exceptName = createUtfString( "java/lang/ArithmeticException" );
			break;
		case ERR_CantFindClass:
			exceptName = createUtfString( "java/lang/ClassNotFoundException" );
			break;
		case ERR_ClassCastException:
			exceptName = createUtfString( "java/lang/ClassCastException" );
			break;
		case ERR_NegativeArraySize:
			exceptName = createUtfString( "java/lang/NegativeArraySizeException" );
			break;
		case ERR_ArrayStoreException:
			exceptName = createUtfString( "java/lang/ArrayStoreException" );
			break;
		default:
			exceptName = createUtfString( "java/lang/RuntimeException" );
			break;
	}
	ret = newClass( exceptName, createUtfString( "java/lang/Exception" ), &retType, &retVar );
	if (ret != 0 || retType != RET_TYPE_NONE)
	{
		VmSetFatalErrorNum(ERR_Unknown);
		return WOBJECT_NULL;
	}

	return retVar.obj;
}

void VmResetError(void){
	vmStatus.errNum = ERR_NoError;
	vmStatus.type = TYPE_NO_ERROR;
}

static void VmPrintStack( unsigned long stackPtr ) {
	WClass *wclass;
	WClassMethod *method;
	UtfString utf;

	// get current class and method off stack
	if (stackPtr > 0) {
		wclass = (WClass *)vmStack[stackPtr - 1].refValue;
		method = (WClassMethod *)vmStack[stackPtr - 2].refValue;
		// output class and method name
		if (wclass != NULL) {
			utf = getUtfString(wclass, wclass->classNameIndex);
			debuglog( "\tclassName: %s\n", UtfToStaticUChars( utf ) );
		}else{
			debuglog( "\tclass: NULL\n" );
		}

		if (method != NULL) {
			utf = getUtfString(wclass, METH_nameIndex(method));
			debuglog( "\tmethodName: %s", UtfToStaticUChars( utf ) );
			utf = getUtfString(wclass, METH_descIndex(method));
			debuglog( "%s", UtfToStaticUChars( utf ) );
			if (METH_isNative(method))
			{
				debuglog(" (native)\n");
				debuglog("\tNumParams=%d\n", vmStack[stackPtr - 4]);
			}
			else
			{
				debuglog("\n\tNumLocals=%d NumStacks=%d\n", METH_maxLocals(method), METH_maxStack(method) );
			}
		}else{
			debuglog( "\tmethod: NULL\n" );
		}
	}else{
		debuglog( "\tstackPtr<=0\n" );
	}
}

void VmPrintStackTrace(void) {
	unsigned long stackPtr;
	WClassMethod *method;
	T_MEMINFO info;

	debuglog("\nVmPrintStackTrace\n");
	debuglog("(Heap Mem=%d Unused=%d)\n", getTotalMemSize(), getUnusedMemSize());

	getMemInfo(&info);

	stackPtr = vmStackPtr;
	while( stackPtr > 0 ){
		debuglog("stackPtr : %d(0x%04x)\n", stackPtr, stackPtr);
		VmPrintStack( stackPtr );

		method = (WClassMethod *)vmStack[stackPtr - 2].refValue;

		stackPtr -= 3;
		if (METH_isNative(method))
			stackPtr -= vmStack[--stackPtr].intValue;
		else
			stackPtr -= METH_maxLocals(method) + METH_maxStack(method);

		if (stackPtr <= 0 )
			break;
		stackPtr -= 3;
	}

	debuglog("\n");
}

void VmSetFatalError(unsigned short errNum, UtfString *utfStrs, unsigned long utfNum ) {
	unsigned long i;

	vmStatus.errNum = errNum;
	vmStatus.type = TYPE_FATAL_ERROR;

	debuglog( "ErrorNum[0x%x]=%s\n", errNum, errorMessages[ errNum & 0x7FFF ] );
	for( i = 0 ; i < utfNum; i++ )
		debuglog( "Desc[%d]=%s\n", i, UtfToStaticUChars( utfStrs[i] ) );

	VmPrintStackTrace();
}
