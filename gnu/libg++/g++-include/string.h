
#ifndef _string_h
#define _string_h 1

#include <_G_config.h>

#ifndef size_t
#define size_t _G_size_t
#endif

#ifndef NULL
#define NULL _G_NULL
#endif

extern "C" {

char*     strcat(char*, const char*);
char*     strchr(const char*, int);
int       strcmp(const char*, const char*);
int       strcoll(const char*, const char*);
char*     strcpy(char*, const char*);
size_t    strcspn(const char*, const char*);
char*     strdup(const char*);
// NOTE: If you get a error message from g++ that this declaration
// conflicts with a <built-in> declaration of strlen(), that usually
// indicates that __SIZE_TYPE__ is incorrectly defined by gcc.
// Fix or add SIZE_TYPE in the appropriate file in gcc/config/*.h.
size_t    strlen(const char*);
char*     strncat(char*, const char*, size_t);
int       strncmp(const char*, const char*, size_t);
char*     strncpy(char*, const char*, size_t);
char*     strpbrk(const char*, const char*);
char*     strrchr(const char*, int);
size_t    strspn(const char*, const char*);
char*     strstr(const char*, const char *);
char*     strtok(char*, const char*);
size_t    strxfrm(char*, const char*, size_t);

char*     index(const char*, int);
char*     rindex(const char*, int);
}

#include <memory.h>

#endif
