

#ifndef _stdlib_h
#define _stdlib_h 1

#include <_G_config.h>
#include <stddef.h>

extern "C" {

int       abs(int);

#ifdef __GNUG__
void volatile abort(void);
#else
void abort(void);
#endif

double    atof(const char*);
int       atoi(const char*);
long      atol(const char*);

int       atexit(auto void (*p) (void));
void*     bsearch (const void *, const void *, size_t, 
                   size_t, auto int (*ptf)(const void*, const void*));
void*     calloc(size_t, size_t);
void      cfree(void*);

#ifdef __GNUG__
void volatile exit(int);
#else
void      exit(int);
#endif

char*     fcvt(double, int, int*, int*);
void      free(void*);
char*     getenv(const char*);
int       getopt _G_ARGS((int, char * const *, const char*));
int       getpw(int, char*);
char*     gcvt(double, int, char*);
char*     ecvt(double, int, int*, int*);
extern char**   environ;

long      labs(long);
void*     malloc(size_t);
size_t    malloc_usable_size(void*);
int       putenv(const char*);
extern char*    optarg;
extern int      opterr;
extern int      optind;
void      qsort(void*, size_t, size_t, auto int (*ptf)(void*,void*));
int       rand(void);
void*     realloc(void*, size_t);
int       setkey(const char*);
int       srand(unsigned int);
double    strtod(const char*, char**);
long      strtol(const char*, char**, int);
unsigned long strtoul(const char*, char **, int);
int       system(const char*);

long      random(void);
void      srandom(int);
char*     setstate(char*);
char*     initstate(unsigned, char*, int);

double    drand48(void);
void      lcong48(short*);
long      jrand48(short*);
long      lrand48(void);
long      mrand48(void);
long      nrand48(short*);
short*    seed48(short*);
void      srand48(long);

char*     ctermid(char*);
char*     cuserid(char*);
char*     tempnam(const char*, const char*);
char*     tmpnam(char*);

}
#endif
