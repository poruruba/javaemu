#ifndef _ALLOC_CLASS_H_
#define _ALLOC_CLASS_H_

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

#define CLASS_FILES

#ifdef CLASS_FILES
#define PRECLASS_DIR	"pre_classes"
#endif

extern char *baseClassDir;

unsigned char *getClassCode( const char *className );
long initClassBlock(void);
long closeClassBlock(void);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif

