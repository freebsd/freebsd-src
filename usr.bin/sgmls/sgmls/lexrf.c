/* LEXRF: Lexical tables for reference concrete syntax.
*/

#include "config.h"
#include "entity.h"           /* Templates for entity control blocks. */
#include "synxtrn.h"          /* Declarations for concrete syntax constants. */
#include "action.h"           /* Action names for all parsing. */
#include "lexcode.h"

static UNCH SRTAB[] = { TABCHAR, '\0' };
static UNCH SRRE[] = { RECHAR, '\0' };
static UNCH SRRS[] = { RSCHAR, '\0' };
static UNCH SRRSB[] = { RSCHAR, 'B', '\0' };
static UNCH SRRSRE[] = { RSCHAR, RECHAR, '\0' };
static UNCH SRRSBRE[] = { RSCHAR, 'B', RECHAR, '\0' };
static UNCH SRBRE[] = { 'B', RECHAR, '\0' };

struct lexical lex = {        /* Delimiter set constants for parser use. */
     {                        /* Markup strings for text processor use. */
          (UNCH *)"\4&#",                 /* LEXCON markup string: CRO        */
          (UNCH *)"[",                     /* LEXCON markup string: DSO        */
          (UNCH *)"\3&",                   /* LEXCON markup string: ERO        */
          (UNCH *)"\4</",                  /* LEXMARK markup string: end-tag   */
          (UNCH *)"\3\"",                  /* LEXMARK markup string: LIT       */
          (UNCH *)"\3'",                   /* LEXMARK markup string: LITA      */
          (UNCH *)"\3>",                   /* LEXCON markup string: MDC        */
          (UNCH *)"\4<!",                  /* LEXCON markup string: MDO        */
          (UNCH *)"\5]]>",                 /* LEXCON markup string: mse        */
          (UNCH *)"\5<![",                 /* LEXCON markup string: mss        */
          (UNCH *)"\13<![CDATA[",          /* LEXCON markup string: mss CDATA  */
          (UNCH *)"\14<![RCDATA[",         /* LEXCON markup string: mss RCDATA */
          (UNCH *)"\3>",                   /* LEXCON markup string: PIC        */
          (UNCH *)"\4<?",                  /* LEXCON markup string: PIO        */
          (UNCH *)"\3;",                   /* LEXGRP markup string: ref close. */
          (UNCH *)"\3<",                   /* LEXMARK markup string: start-tag */
          (UNCH *)"\3>",                   /* LEXMARK markup string: TAGC      */
          (UNCH *)"\3=",                   /* LEXMARK markup string: VI        */
          3,                       /* LEXMARK: length of null end-tag. */
          2                        /* LEXMARK: length of null start-tag. */
     },
     {                        /* Short reference delimiters. */
          {                        /* Short reference delimiter table. */
               {(UNCH *)"",       SRCT},        /* Dummy entry to store SR count. */
               {SRTAB,		  1},           /* TAB */
               {SRRE,             2},           /* RE */
               {SRRS,             3},           /* RS */
               {SRRSB,            4},           /* Leading blanks */
               {SRRSRE,           5},           /* Null record */
               {SRRSBRE,          6},           /* Blank record */
               {SRBRE,            7},           /* Trailing blanks */
               {(UNCH *)" ",      8},           /* Space */
               {(UNCH *)"BB",     9},           /* Two or more blanks */
               {(UNCH *)"\"",    10},           /* Quotation mark (first data character) */
               {(UNCH *)"#",     11},           /* Number sign */
               {(UNCH *)"%",     12},           /* FCE CHARACTERS start here */
               {(UNCH *)"'",     13},
               {(UNCH *)"(",     14},
               {(UNCH *)")",     15},
               {(UNCH *)"*",     16},
               {(UNCH *)"+",     17},
               {(UNCH *)",",     18},
               {(UNCH *)"-",     19},           /* Hyphen */
               {(UNCH *)"--",    20},           /* Two hyphens */
               {(UNCH *)":",     21},
               {(UNCH *)";",     22},
               {(UNCH *)"=",     23},
               {(UNCH *)"@",     24},
               {(UNCH *)"[",     25},
               {(UNCH *)"]",     26},
               {(UNCH *)"^",     27},
               {(UNCH *)"_",     28},           /* Low line */
               {(UNCH *)"{",     29},
               {(UNCH *)"|",     30},
               {(UNCH *)"}",     31},
               {(UNCH *)"~",     32},
               {0,     0}
          },
          {                        /* Printable form of unprintable SR delims.*/
               "",                      /* Dummy entry to balance s.dtb. */
               "&#TAB;",                /* TAB */
               "&#RE;",                 /* RE */
               "&#RS;",                 /* RS */
               "&#RS;B",                /* Leading blanks */
               "&#RS;&#RE;",            /* Null record */
               "&#RS;B&#RE;",           /* Blank record */
               "B&#RE;",                /* Trailing blanks */
               "&#SPACE;"               /* Space */
          },
          12,                      /* LEXCNM: Index of first FCE in srdeltab. */
          20,                      /*LEXCNM:Index of "two hyphens" in srdeltab*/
          10,                      /* LEXCNM: Index of first SR with data char. */
          19,                      /* LEXCNM: Index of hyphen in srdeltab. */
          SRNPRT+1,                /* LEXCNM: Index of 1st printable SR. */
          8,                       /* LEXCNM: Index of space in srdeltab. */
	  25,			   /* LEXCNM: Index of left bracket in srdeltab. */
	  26,			   /* LEXCNM: Index of right bracket in srdeltab. */
     },                       /* End of short reference delimiters. */
     {                        /* General delimiter characters. */
          GENRECHAR,               /*LEXCNM:(BS)Generated RE; can't be markup.*/
          '"',                     /* LEXMARK: Char used as LIT delimiter.*/
          '\'',                    /* LEXMARK: Char used as LITA delimiter.*/
          '>',                     /* LEXLMS: Char used as MDC delimiter.*/
          ']',                     /* LEXLMS: Char used as MSC when enabled.*/
          '/',                     /* LEXCON: Char used as NET when enabled.*/
          '%',                     /* LEXMARK: Char used as PERO delimiter. */
          '>',                     /* LEXCON: Char used as PIC delimiter.*/
          '<'                      /* LEXCON: Char used as TAGO when enabled.*/
     },
     {                        /* Lexical table code assignments. */
          FCE,                    /* LEXCNM: FRE char as entity reference.*/
          FRE,                    /* LEXLMS: Free character not an entity ref.*/
          LITC,                   /* LEXLMS: Literal close delimiter enabled. */
          MSC3,                   /* LEXLMS: Marked section close delim enabled. */
          NET,                    /* LEXCON: Null end-tag delimiter enabled. */
          ETI,                    /* LEXCON: NET disabled; still used as ETI. */
          SPCR,                   /* LEXCNM: Space in use as SHORTREF delim. */
          TGO2,                   /* LEXCON: Tag open delimiter enabled. */
          CDE                     /* LEXLMS: CDATA/SDATA delimiters. */
     }
};

UNCH *lextabs[] = {
     lexcnm, lexcon, lexgrp, lexlms, lexmark, lexsd, lextoke, 0
};
