/* Struct dcncb: attribute list added to support data attributes.             */
#ifndef ENTITY_H              /* Don't include this file more than once. */
#define ENTITY_H
/* ENTITY.H: Definitions and control block templates for entity management.
*/
#include "tools.h"            /* Definitions for type declarations, etc. */
#include "msgcat.h"

#define STDINNAME "-"	      /* File name that refers to standard input. */

#define EOS         '\0'      /* NONCHAR: EE (entity end: strings). */

#define AVALCASE      2       /* 2=untranslated string of name characters. */

#define REFNAMELEN 8          /* reference quantity set NAMELEN */
#define REFLITLEN 240         /* reference quantity set LITLEN */

/* Minimization status of returned tags.
*/
#define MINNONE 0             /* Minimization: tag not minimized. */
#define MINNULL 1             /* Minimization: tag was null. */
#define MINNET  2             /* Minimization: end-tag was NET delimiter. */
#define MINDATA 3             /* Minimization: end-tag was data tag. */
#define MINSTAG 4             /* Minimization: tag implied by start-tag. */
#define MINETAG 5             /* Minimization: end-tag implied by end-tag. */

/* Formal public identifier public text classes.
*/
#define FPICAP         1
#define FPICHARS       2
#define FPINOT         3
#define FPISYN         4
#define FPICMINV       5      /* Minimum fpic value for versionable text. */
#define FPIDOC         5
#define FPIDTD         6
#define FPIELEM        7
#define FPIENT         8
#define FPILPD         9
#define FPINON        10
#define FPISHORT      11
#define FPISUB        12
#define FPITEXT       13
struct fpi {                  /* Formal public identifier. */
     UNCH fpiot;              /* Owner type: + or - or ! (for ISO). */
     UNS fpiol;		      /* Length of owner identifier. */
     UNS fpio;                /* Offset in pubis of owner identifier (no EOS).*/
     int fpic;                /* Public text class. */
     UNCH fpitt;              /* Text type: - or + (for available). */
     UNS fpitl;		      /* Length of text identifier. */
     UNS fpit;                /* Offset in pubis of text identifier (no EOS). */
     UNS fpill;		      /* Language/designating sequence length. */
     UNS fpil;                /* Offset in pubis of language. */
     UNS fpivl;		      /* Length of display version . */
     UNS fpiv;                /* Offset in pubis of display version (no EOS). */
     int fpiversw;	      /* 1=use best ver; 0=use stated ver; -1=error. */
     UNCH *fpinm;	      /* Entity/DCN name (EOS, no length). */
     UNCH fpistore;           /* 1=NDATA 2=general 3=parm 4=DTD 5=LPD 6=DCN. */
     /* Name of the entity's DCN.  Valid only when fpistore == 1.
	NULL if it's a SUBDOC. */
     UNCH *fpinedcn;
     UNCH *fpipubis; /* Public ID string (EOS). */
     UNCH *fpisysis; /* System ID string (EOS). */
};
#define FPISZ sizeof(struct fpi)
typedef struct fpi *PFPI;     /* Ptr to FPI control block. */

/* General control blocks.
*/
#define NONONCH 1             /* Character references to non-chars invalid. */
#define OKNONCH 0             /* Character references to non-chars allowed. */
struct parse {                /* Parse control block. */
     char   *pname;           /* Parse name; content, tag, etc. */
     UNCH   *plex;            /* Lexical analysis table. */
     UNCH   **ptab;           /* State and action table. */
     UNS    state;            /* State. */
     UNS    input;            /* Input. */
     UNS    action;           /* Action. */
     UNS    newstate;         /* Next state. */
};
struct restate {
     UNS   sstate;            /* State. */
     UNS   sinput;            /* Input. */
     UNS   saction;           /* Action. */
     UNS   snext;             /* Next state. */
};
struct map {
     UNCH   *mapnm;           /* Name followed by EOS. */
     int    mapdata;          /* Data associated with that name. */
};
struct hash {                 /* Dummy structure for function arguments. */
     struct hash *enext;      /* Next entry in chain. */
     UNCH *ename;	      /* Entry name with size and EOS. */
};
typedef struct hash *PHASH;   /* Ptr to hash table entry. */
typedef struct hash **THASH;  /* Ptr to hash table. */

struct fwdref {               /* A forward id reference. */
     struct fwdref *next;     /* Pt to next reference in chain. */
     UNIV msg;		      /* Ptr to saved error messsage. */
};
#define FWDREFSZ sizeof(struct fwdref)

struct dcncb {                /* Data content notation control block. */
     struct dcncb *enext;     /* Next DCN in chain. */
     UNCH *ename;	      /* Notation name followed by EOS. */
     UNCH mark;		      /* For use by application. */
     UNCH entsw;	      /* Entity defined with this notation? */
     UNCH defined;            /* Has this notation been defined. */
     UNCH *sysid;             /* System identifier of notation. */
     UNCH *pubid;             /* Public identifier of notation. */
     struct ad *adl;          /* Data attribute list (NULL if none). */
};
#define DCBSZ sizeof(struct dcncb)
#define DCNMARK(p) ((p)->mark ? 1 : ((p)->mark = 1, 0))

typedef struct dcncb *PDCB;   /* Ptr to DCN control block. */

/* Number of capacities in a capacity set. */

#define NCAPACITY 17

struct sgmlcap {
     char **name;
     UNCH *points;
     long *number;
     long *limit;
};

struct sgmlstat {             /* Document statistics. */
     UNS dcncnt;              /* Number of data content notations defined. */
     UNS pmexgcnt;            /* Number of plus or minus exception groups. */
     UNS etdcnt;              /* Number of element types declared. */
     UNS etdercnt;            /* Number of element types defined by default. */
     UNS pmexcnt;             /* Number of plus/minus exception grp members. */
     UNS modcnt;              /* Number of content model tokens defined. */
     UNS attcnt;              /* Number of attributes defined. */
     UNS attdef;              /* Characters of attribute defaults defined. */
     UNS attgcnt;             /* Number of att value grp members (incl dcn). */
     UNS idcnt;               /* Number of ID attributes specified. */
     UNS idrcnt;              /* Number of ID references specified. */
     UNS ecbcnt;              /* Number of entities declared. */
     UNS ecbtext;             /* Characters of entity text defined. */
     UNS srcnt;               /* Number of short reference tables defined. */
     UNS dcntext;             /* Characters of notation identifiers defined. */
};
struct switches {             /* Parser control switches (1=non-standard). */
     int swdupent;            /* 1=msg if duplicate ENTITY def attempted;0=no.*/
     int swcommnt;            /* 1=return comment declarations as data; 0=no. */
     int swrefmsg;            /* 1=msg if undeclared ref is defaulted; 0=no. */
     UNS swbufsz;             /* Size of source file buffer for READ(). */
     int swenttr;             /* 1=trace entity stack in error messages; 0=no.*/
     int sweltr;	      /* 1=trace element stack in error messages; 0=no. */
     int swambig;	      /* 1=check content model ambiguity */
     int swundef;	      /* 1=warn about undefined elements and notations. */
     char *prog;              /* Program name for error messages. */
#ifdef TRACE
     char *trace;	      /* What to trace in the body. */
     char *ptrace;	      /* What to trace in the prolog. */
#endif /* TRACE */
     nl_catd catd;	      /* Message catalog descriptor. */
     long nopen;	      /* Number of open document entities */
     int onlypro;	      /* Parse only the prolog. */
     char **includes;	      /* List of parameter entities to be defined
			         as "INCLUDE"; NULL terminated.*/
     VOID (*die) P((void));   /* Function to call on fatal error. */
};
struct markup {               /* Delimiter strings for text processor. */
     UNCH *cro;               /* LEXCON markup string: CRO        */
     UNCH *dso;               /* LEXCON markup string: DSO        */
     UNCH *ero;               /* LEXCON markup string: ERO        */
     UNCH *etag;              /* LEXMARK markup string: end-tag   */
     UNCH *lit;               /* LEXMARK markup string: LIT       */
     UNCH *lita;              /* LEXMARK markup string: LITA      */
     UNCH *mdc;               /* LEXCON markup string: MDC       */
     UNCH *mdo;               /* LEXCON markup string: MDO       */
     UNCH *mse;               /* LEXCON markup string: mse        */
     UNCH *mss;               /* LEXCON markup string: mss        */
     UNCH *mssc;              /* LEXCON markup string: mss CDATA  */
     UNCH *mssr;              /* LEXCON markup string: mss RCDATA */
     UNCH *pic;               /* LEXCON markup string: PIC        */
     UNCH *pio;               /* LEXCON markup string: PIO        */
     UNCH *refc;              /* LEXGRP markup string: REFC       */
     UNCH *stag;              /* LEXMARK markup string: start-tag */
     UNCH *tagc;              /* LEXMARK markup string: TAGC      */
     UNCH *vi;                /* LEXMARK markup string: VI        */
     int lennet;              /* LEXMARK markup string length: null end-tag. */
     int lennst;              /* LEXMARK markup string length: null start-tag.*/
};
#endif /* ndef ENTITY_H */
