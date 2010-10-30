/* script-c.h -- C interface for linker scripts in gold.  */

/* This file exists so that both the bison parser and script.cc can
   include it, so that they can communicate back and forth.  */

#ifndef GOLD_SCRIPT_C_H
#define GOLD_SCRIPT_C_H

#ifdef __cplusplus
extern "C" {
#endif

#include "yyscript.h"

/* The bison parser function.  */

extern int
yyparse(void* closure);

/* Called by the bison parser skeleton to return the next token.  */

extern int
yylex(YYSTYPE*, void* closure);

/* Called by the bison parser skeleton to report an error.  */

extern void
yyerror(void* closure, const char*);

/* Called by the bison parser to add a file to the link.  */

extern void
script_add_file(void* closure, const char*);

/* Called by the bison parser to start and stop a group.  */

extern void
script_start_group(void* closure);
extern void
script_end_group(void* closure);

/* Called by the bison parser to start and end an AS_NEEDED list.  */

extern void
script_start_as_needed(void* closure);
extern void
script_end_as_needed(void* closure);

#ifdef __cplusplus
}
#endif

#endif /* !defined(GOLD_SCRIPT_C_H) */
