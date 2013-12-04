
#ifndef AUTOGEN_PROJECT_H
#define AUTOGEN_PROJECT_H

#include "config.h"

/*
 *  Procedure success codes
 *
 *  USAGE:  define procedures to return "tSuccess".  Test their results
 *          with the SUCCEEDED, FAILED and HADGLITCH macros.
 *
 *  Microsoft sticks its nose into user space here, so for Windows' sake,
 *  make sure all of these are undefined.
 */
#undef  SUCCESS
#undef  FAILURE
#undef  PROBLEM
#undef  SUCCEEDED
#undef  SUCCESSFUL
#undef  FAILED
#undef  HADGLITCH

#define SUCCESS  ((tSuccess) 0)
#define FAILURE  ((tSuccess)-1)
#define PROBLEM  ((tSuccess) 1)

typedef int tSuccess;

#define SUCCEEDED(p)    ((p) == SUCCESS)
#define SUCCESSFUL(p)   SUCCEEDED(p)
#define FAILED(p)       ((p) <  SUCCESS)
#define HADGLITCH(p)    ((p) >  SUCCESS)

#ifndef STR
#  define __STR(s)      #s
#  define STR(s)        __STR(s)
#endif

#define STRSIZE(s)      (sizeof(s)-1)

#ifdef DEFINING
#  define VALUE(s)      = s
#  define MODE
#else
#  define VALUE(s)
#  define MODE extern
#endif

#endif /* AUTOGEN_PROJECT_H */
