#include "utils.h"
#include "mem_alloc.h"
#include "alloc_class.h"
#include "debuglog.h"
#include <stdio.h>
#include <string.h>

extern const unsigned char classRom[];
extern const unsigned long classRomSize;

extern unsigned char *classRom_ext;
extern unsigned long classRomSize_ext;

#define CLASS_FILES

#ifdef CLASS_FILES
char *baseClassDir = PRECLASS_DIR;

static long file_read(const char *p_basedir, const char *p_fname, unsigned char **pp_bin, long *p_size)
{
	FILE *fp;
	long fsize;
	char path[255];

	strcpy(path, p_basedir);
	strcat(path, "/");
	strcat(path, p_fname);
	strcat(path, ".class");

	fp = fopen(path, "rb");
	if (fp == NULL)
		return FT_ERR_NOTFOUND;

	fseek(fp, 0, SEEK_END);
	fsize = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	*pp_bin = (unsigned char*)mem_alloc(fsize);
	if (*pp_bin == NULL)
		return FT_ERR_NOTENOUGH;

	if (fread(*pp_bin, 1, fsize, fp) != fsize)
	{
		fclose(fp);
		mem_free(*pp_bin);
		return FT_ERR_UNKNOWN;
	}
	fclose(fp);

	*p_size = fsize;
	
	return FT_ERR_OK;
}

static unsigned char* loadClassCode_local( const char* className )
{
//	debuglog("loadClassCode_local: %s\n", className);

	long ret;
	long len;
	unsigned char *p_bin;

	ret = file_read(baseClassDir, className, &p_bin, &len);
	if( ret != FT_ERR_OK )
		return NULL;

	return p_bin;
}
#endif

static unsigned char* loadClassCode(const char* className)
{
//	debuglog("loadClassCode: %s\n", className);

	unsigned short nameSize;
	unsigned long allSize;
	unsigned char *ptr;

	if (classRom_ext != NULL) {
		ptr = (unsigned char*)classRom_ext;
		while ((unsigned long)(ptr - classRom_ext) < classRomSize_ext) {
			allSize = utils_get_uint32b(ptr);
			ptr += sizeof(unsigned long);
			nameSize = utils_get_uint16b(ptr);
			ptr += sizeof(unsigned short);
			if ((strlen(className) == nameSize) && (strncmp(className, (const char*)ptr, nameSize) == 0))
				return ptr + nameSize + sizeof(unsigned long);
			ptr += allSize - sizeof(unsigned short);
		}
	}

#ifdef CLASS_FILES
	return loadClassCode_local(className);
#else
	return NULL;
#endif
}

unsigned char *getClassCode( const char *className ){
	unsigned short nameSize;
	unsigned long allSize;
	unsigned char *ptr;

	ptr = (unsigned char*)classRom;
	while( (unsigned long)( ptr - classRom ) < classRomSize ){
		allSize = utils_get_uint32b( ptr );
		ptr += sizeof( unsigned long );
		nameSize = utils_get_uint16b( ptr );
		ptr += sizeof(unsigned short);
		if( ( strlen( className ) == nameSize ) && ( strncmp( className, (const char*)ptr, nameSize ) == 0 ) )
			return ptr + nameSize + sizeof(unsigned long);
		ptr += allSize - sizeof(unsigned short);
	}

	return loadClassCode( className );
}

long initClassBlock(void){
	return closeClassBlock();
}

long closeClassBlock(void){
	return FT_ERR_OK;
}
