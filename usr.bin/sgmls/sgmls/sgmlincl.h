/* SGMLINCL.H: Include file for parser core. */
#ifndef SGMLINCL              /* Don't include this file more than once. */
#define SGMLINCL 1
#include "config.h"
#include "std.h"
#include "entity.h"           /* Templates for entity control blocks. */
#include "action.h"           /* Action names for all parsing. */
#include "adl.h"              /* Definitions for attribute list processing. */
#include "error.h"            /* Symbols for error codes. */
#include "etype.h"            /* Definitions for element type processing. */
#include "keyword.h"          /* Definitions for keyword processing. */
#include "lextoke.h"          /* Symbols for tokenization lexical classes. */
#include "source.h"           /* Templates for source entity control blocks. */
#include "synxtrn.h"          /* Declarations for concrete syntax constants. */
#include "sgmlxtrn.h"         /* External variable declarations. */
#include "trace.h"            /* Declarations for internal trace functions. */
#include "sgmlmain.h"
#include "sgmlaux.h"
#include "sgmlfnsm.h"         /* ANSI C: Declarations for SGML functions. */
#endif /* ndef SGMLINCL */
