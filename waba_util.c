#include "utils.h"
#include "waba.h"
#include "waba_utf.h"
#include "waba_util.h"

long callStaticMethod(WClass* wclass, UtfString name, UtfString desc, Var params[], unsigned short numParams, unsigned char *retType, Var* retVar){
	WClassMethod* staticMethod;
	WClass* vclass;

	staticMethod = getMethod(wclass, name, desc, &vclass);
	if (staticMethod == NULL)
		return FT_ERR_NOTFOUND;

	return executeMethod(vclass, staticMethod, params, numParams, retType, retVar);
}

long startStaticMain(const char *className, const char *param, unsigned char *retType, Var* retVar) {
	long ret;
	WClass *wclass;
	Var params[1];
	WObject* strArray;

	wclass = getClass(createUtfString(className));
	if (wclass == NULL)
		return FT_ERR_NOTFOUND;

	params[0].obj = createArrayObject(TYPE_OBJECT, 1);
	if (pushObject(params[0].obj) != FT_ERR_OK)
		return FT_ERR_UNKNOWN;
	strArray = (WObject *)WOBJ_arrayStart(params[0].obj);
	strArray[0] = createString(param);
	if (pushObject(strArray[0]) != FT_ERR_OK)
	{
		popObject();
		return FT_ERR_UNKNOWN;
	}

	ret = callStaticMethod(wclass, createUtfString("main"), createUtfString("([Ljava/lang/String;)V"), params, 1, retType, retVar);

	popObject();
	popObject();

	return ret;
}

