/* config.h.  Generated automatically by configure.  */
/* Portability variables.  -*- C -*- */

/* Define if the system does not support the `const' keyword.  */
/* #undef const */

/* Define if the system supports file names longer than 14 characters.  */
#define HAVE_LONG_FILE_NAMES 

/* Define if the system has pathconf().  */
/* #undef HAVE_PATHCONF */

/* Define if the system has strerror().  */
#define HAVE_STRERROR 1

/* Define if the system has ANSI C header files and library functions.  */
#define STDC_HEADERS

/* Define if the system uses strchr instead of index
   and strrchr instead of rindex.  */
#define HAVE_STRING_H 1

#if defined(STDC_HEADERS) || defined(HAVE_STRING_H)
#define index strchr
#define rindex strrchr
#endif

/* Define if the system has unistd.h.  */
#define HAVE_UNISTD_H 1

/* Define if the system has fcntl.h.  */
#define HAVE_FCNTL_H 1

/* Define as either int or void -- the type that signal handlers return.  */
#define RETSIGTYPE void

#ifndef RETSIGTYPE
#define RETSIGTYPE void
#endif

/*  Which directory library header to use.  */
#define DIRENT 1			/* dirent.h */
/* #undef SYSNDIR */			/* sys/ndir.h */
/* #undef SYSDIR */			/* sys/dir.h */
/* #undef NDIR */			/* ndir.h */
/* #undef NODIR */			/* none -- don't make numbered backup files */

/* Define if the system lets you pass fewer arguments to a function
   than the function actually accepts (in the absence of a prototype).
   Defining it makes I/O calls slightly more efficient.
   You need not bother defining it unless your C preprocessor chokes on
   multi-line arguments to macros.  */
/* #undef CANVARARG */

/* Define Reg* as either `register' or nothing, depending on whether
   the C compiler pays attention to this many register declarations.
   The intent is that you don't have to order your register declarations
   in the order of importance, so you can freely declare register variables
   in sub-blocks of code and as function parameters.
   Do not use Reg<n> more than once per routine.

   These don't really matter a lot, since most modern C compilers ignore
   register declarations and often do a better job of allocating
   registers than people do.  */

#define Reg1 register
#define Reg2 register
#define Reg3 register
#define Reg4 register
#define Reg5 register
#define Reg6 register
#define Reg7
#define Reg8
#define Reg9
#define Reg10
#define Reg11
#define Reg12
#define Reg13
#define Reg14
#define Reg15
#define Reg16
