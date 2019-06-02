#ifndef _WABA_H_
#define _WABA_H_

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

#define SMALLMEM 1
#ifdef SMALLMEM

typedef unsigned short ConsOffsetType;
#define MAX_consOffset 0x7FFF
#define CLASS_HASH_SIZE	63

#else

typedef unsigned long ConsOffsetType;
#define MAX_consOffset 0x7FFFFFFF
#define CLASS_HASH_SIZE	255

#endif

//
// types and accessors
//
typedef unsigned long WObject;

#define WOBJECT_NULL 0

typedef union {
	long intValue;
	unsigned char uint8Value;
//	float32 floatValue;
	void *classRef;
	unsigned char *pc;
	void *refValue;
	WObject obj;
} Var;

//
// more types and accessors
//

Var *objectPtr(WObject obj);

#define WOBJ_class(o) (objectPtr(o))[0].classRef
#define WOBJ_var(o, idx) (objectPtr(o))[idx + 1]
// for faster access
#define WOBJ_varP(objPtr, idx) (objPtr)[idx + 1]

// NOTE: These get various values in objects at defined offsets.
// If the variables in the base classes change, these offsets will
// need to be recomputed. For example, the first (StringCharArray)
// get the character array var offset in a String object.
#define WOBJ_StringCharArrayObj(o) WOBJ_var(o,0).obj

#define WOBJ_StringBufferStrings(o) WOBJ_var(o,0).obj
#define WOBJ_StringBufferCount(o) WOBJ_var(o,1).intValue

//#define WOBJ_arrayType(o) ((unsigned char)(WOBJ_var(o,0).intValue))
#define WOBJ_arrayType(o) WOBJ_var(o,0).uint8Value
#define WOBJ_arrayLen(o) WOBJ_var(o,1).intValue
#define WOBJ_arrayStart(o) (&(WOBJ_var(o,2)))

// for faster access
#define WOBJ_arrayTypeP(objPtr) WOBJ_varP(objPtr,0).intValue
#define WOBJ_arrayLenP(objPtr) WOBJ_varP(objPtr,1).intValue
#define WOBJ_arrayStartP(objPtr) (&(WOBJ_varP(objPtr,2)))

typedef struct {
	Var *ptr;
	unsigned long order;
	unsigned long temp;
} Hos;

typedef struct UtfStringStruct {
	char *str;
	unsigned short len;
} UtfString;

typedef union {
	// FieldVar is either a reference to a static class variable (staticVar)
	// or an offset of a local variable within an object (varOffset)
	Var staticVar;
	unsigned long varOffset; // computed var offset in object
} FieldVar;

typedef struct WClassFieldStruct {
	unsigned char *header;
	FieldVar var;
} WClassField;

typedef struct WClassHandlerStruct {
	unsigned short start_pc;
	unsigned short end_pc;
	unsigned short handler_pc;
	unsigned short catch_type;
} WClassHandler;

//
// TYPES AND METHODS
//

// Access flags
#define ACC_PUBLIC       0x0001
#define ACC_PRIVATE      0x0002
#define ACC_PROTECTED    0x0004
#define ACC_STATIC       0x0008
#define ACC_FINAL        0x0010
#define ACC_SYNCHRONIZED 0x0020
#define ACC_VOLATILE     0x0040
#define ACC_TRANSIENT    0x0080
#define ACC_NATIVE       0x0100
#define ACC_INTERFACE    0x0200
#define ACC_ABSTRACT     0x0400

#define FIELD_accessFlags(f) utils_get_uint16b(f->header)
#define FIELD_nameIndex(f) utils_get_uint16b(&f->header[2])
#define FIELD_descIndex(f) utils_get_uint16b(&f->header[4])
#define FIELD_isStatic(f) ((FIELD_accessFlags(f) & ACC_STATIC) > 0)

typedef long (*NativeFunc)( Var stack[] );

typedef union {
	// Code is either pointer to bytecode or pointer to native function
	// NOTE: If accessFlags(method) & ACCESS_NATIVE then nativeFunc
	// is set, otherwise codeAttr is set. Native methods don't have
	// maxStack, maxLocals so it is OK to merge the codeAttr w/nativeFunc.
	unsigned char *codeAttr;
	NativeFunc nativeFunc;
} Code;

typedef struct WClassMethodStruct {
	unsigned char *header;
	Code code;
	unsigned short numParams:14;
	unsigned short returnsValue:1;
	unsigned short isInit:1;
	unsigned short numHandlers;
	WClassHandler *handlers;
} WClassMethod;

#define METH_accessFlags(m) utils_get_uint16b(m->header)
#define METH_nameIndex(m) utils_get_uint16b(&m->header[2])
#define METH_descIndex(m) utils_get_uint16b(&m->header[4])
#define METH_maxStack(m) utils_get_uint16b(&m->code.codeAttr[6])
#define METH_maxLocals(m) utils_get_uint16b(&m->code.codeAttr[8])
#define METH_codeCount(m) utils_get_uint32b(&m->code.codeAttr[10])
#define METH_code(m) (&m->code.codeAttr[14])
#define METH_isNative(m) ((METH_accessFlags(m) & ACC_NATIVE) != 0)

typedef void (*ObjDestroyFunc)(WObject obj);

// NOTE: In the following structure, a constant offset can either be
// bound (by having boundBit set) in which case it is an offset into
// the classHeap directly or unbound in which case it is an offset into
// the byteRep of the class
typedef struct WClassStruct {
	struct WClassStruct **superClasses; // array of this classes superclasses
	unsigned short numSuperClasses;
	unsigned short classNameIndex;
	unsigned char *byteRep; // pointer to class representation in memory (bytes)
	unsigned char *attrib2; // pointer to area after constant pool (accessFlags)
	unsigned short numConstants;
	ConsOffsetType *constantOffsets;
	unsigned short numFields;
	WClassField *fields;
	unsigned short numMethods;
	WClassMethod *methods;
	unsigned short numVars; // computed number of object variables
	ObjDestroyFunc objDestroyFunc;
	struct WClassStruct *nextClass; // next class in hash table linked list
} WClass;

#define WCLASS_accessFlags(wc) utils_get_uint16b(wc->attrib2)
#define WCLASS_thisClass(wc) utils_get_uint16b(&wc->attrib2[2])
#define WCLASS_superClass(wc) utils_get_uint16b(&wc->attrib2[4])
#define WCLASS_numInterfaces(wc) utils_get_uint16b(&wc->attrib2[6])
#define WCLASS_interfaceIndex(wc, idx) utils_get_uint16b(&wc->attrib2[8 + (idx * 2)])
#define WCLASS_objectSize(wc) ((wc->numVars + 1) * sizeof(Var))
#define WCLASS_isInterface(wc) ((WCLASS_accessFlags(wc) & ACC_INTERFACE) != 0)
#define WCLASS_isAbstract(wc) ((WCLASS_accessFlags(wc) & ACC_ABSTRACT) != 0)

// Constant Pool tags
#define CONSTANT_Reserved			0
#define CONSTANT_Utf8               1
#define CONSTANT_Integer            3
#define CONSTANT_Float              4
#define CONSTANT_Long               5
#define CONSTANT_Double             6
#define CONSTANT_Class              7
#define CONSTANT_String             8
#define CONSTANT_Fieldref           9
#define CONSTANT_Methodref          10
#define CONSTANT_InterfaceMethodref 11
#define CONSTANT_NameAndType        12

#define CONS_offset(wc, idx) wc->constantOffsets[idx]
#define CONS_ptr(wc, idx) (wc->byteRep + CONS_offset(wc, idx))
#define CONS_tag(wc, idx) CONS_ptr(wc, idx)[0]
#define CONS_utfLen(wc, idx) utils_get_uint16b(&CONS_ptr(wc, idx)[1])
#define CONS_utfStr(wc, idx) (&CONS_ptr(wc, idx)[3])
#define CONS_integer(wc, idx) utils_get_int32b(&CONS_ptr(wc, idx)[1])
#define CONS_stringIndex(wc, idx) utils_get_uint16b(&CONS_ptr(wc, idx)[1])
#define CONS_classIndex(wc, idx) utils_get_uint16b(&CONS_ptr(wc, idx)[1])
#define CONS_nameIndex(wc, idx) utils_get_uint16b(&CONS_ptr(wc, idx)[1])
#define CONS_typeIndex(wc, idx) utils_get_uint16b(&CONS_ptr(wc, idx)[3])
#define CONS_nameAndTypeIndex(wc, idx) utils_get_uint16b(&CONS_ptr(wc, idx)[3])
#define CONS_descriptorIndex(wc, idx) utils_get_uint16b(&CONS_ptr(wc, idx)[3])

//
// Native Methods and Hooks
//
typedef struct {
	char *className;
	ObjDestroyFunc destroyFunc;
	unsigned short varsNeeded;
} ClassHook;

typedef struct {
	unsigned long hash;
	NativeFunc func;
} NativeMethod;


long VmInit(unsigned long vmStackSizeInBytes, unsigned long nmStackSizeInBytes,
	unsigned long classHeapSize, unsigned long objectHeapSize );
void VmFree(void);

WObject createObject(WClass *wclass);
WObject createArrayObject(unsigned char type, long len);
int pushObject(WObject obj);
WObject popObject(void);

void gc(void);

long newClass(UtfString className, UtfString baseClassName, unsigned char *retType, Var* retVar);
WClass *getClass(UtfString className);
WClassMethod *getMethod(WClass *wclass, UtfString name, UtfString desc, WClass **vclass);
int compatible(WClass *source, WClass *target);
int compatibleArray(WObject obj, UtfString arrayName);
int arrayRangeCheck(WObject array, long start, long count);
long arrayTypeSize(unsigned char type);
long arraySize(unsigned char type, long len);
long executeMethod(WClass *wclass, WClassMethod *method, Var params[], unsigned short numParams, unsigned char *retType, Var* retValue);

typedef struct _T_MEMINFO
{
	unsigned long totalObjectMem;
	unsigned long unusedObjectMem;
	unsigned long totalClassMem;
	unsigned long unusedClassMem;
	unsigned long vmStackSize;
	unsigned long vmStackPtr;
	unsigned long nmStackSize;
	unsigned long nmStackPtr;

}T_MEMINFO;

long getMemInfo(T_MEMINFO *p_info);

void VmResetError(void);
WObject CreateRuntimeException(unsigned short errNum);
void VmPrintStackTrace(void);
void VmSetFatalError(unsigned short errNum, UtfString *utfStrs, unsigned long utfNum );
#define VmSetFatalErrorNum(n) VmSetFatalError(n, NULL, 0)

#define TYPE_UNKNOWN	0
#define TYPE_OBJECT		1
#define TYPE_ARRAY		2
#define TYPE_BOOLEAN	4
#define TYPE_CHAR		5
#define TYPE_FLOAT		6
#define TYPE_DOUBLE		7
#define TYPE_BYTE		8
#define TYPE_SHORT		9
#define TYPE_INT		10
#define TYPE_LONG		11

#define TYPE_NO_ERROR			((unsigned char)0x00)
#define TYPE_RUNTIME_EXCEPTION	((unsigned char)0x01)
#define TYPE_FATAL_ERROR		((unsigned char)0x02)

typedef struct {
	unsigned long errNum;
	unsigned char type;
} ErrorStatus;

extern ErrorStatus vmStatus;


// no error
#define ERR_NoError					0x0000

// fatal errors
#define ERR_Unknown					0x8000
#define ERR_SanityCheckFailed		0x8001
#define ERR_CantAllocateMemory		0x8002
#define ERR_OutOfClassMem			0x8003
#define ERR_OutOfObjectMem			0x8004
#define ERR_NativeStackOverflow		0x8005
#define ERR_NativeStackUnderflow	0x8006
#define ERR_StackOverflow			0x8007
#define ERR_NativeErrorReturn		0x8008

// runtime errors
#define ERR_BadOpcode				0x8009
#define ERR_CantFindMethod			0x800b
#define ERR_CantFindField			0x800c
#define ERR_ClassTooLarge			0x800d
#define ERR_LoadConst				0x800e
#define ERR_LoadField				0x800f
#define ERR_LoadMethod				0x8010
#define ERR_CLInitMethodError		0x8011
#define ERR_ConstantToVar			0x8012
#define ERR_BadParamNum				0x8013
#define ERR_ParamError				0x8014
#define ERR_CantCreateObject		0x8015
#define ERR_NotArray				0x8016
#define ERR_BadClassName			0x8017
#define ERR_CantFindNative			0x8018
#define ERR_NotMainClass			0x8019
#define ERR_CondNotSatisfied		0x801a

// program errors
#define ERR_CantFindClass			0x800a
#define ERR_NullObjectAccess		0x001a
#define ERR_NullArrayAccess			0x001b
#define ERR_IndexOutOfRange			0x001c
#define ERR_DivideByZero			0x001d
#define ERR_ClassCastException		0x001e
#define ERR_NegativeArraySize		0x001f
#define ERR_ArrayStoreException		0x0020
#define ERR_BadClassCode			0x0021

// return type
#define RET_TYPE_NONE		0
#define RET_TYPE_RETURN		1
#define RET_TYPE_EXCEPTION	2

#ifdef __cplusplus
}
#endif // __cplusplus

#endif


