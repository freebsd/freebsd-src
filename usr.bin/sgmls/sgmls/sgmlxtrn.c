/*    Standard Generalized Markup Language Users' Group (SGMLUG)
                 SGML Parser Materials (ARCSGML 1.0)

(C) 1983-1988 Charles F. Goldfarb (assigned to IBM Corporation)
(C) 1988-1991 IBM Corporation

Licensed to the SGML Users' Group for distribution under the terms of
the following license:                                                       */

char license[] =
"SGMLUG hereby grants to any user: (1) an irrevocable royalty-free,\n\
worldwide, non-exclusive license to use, execute, reproduce, display,\n\
perform and distribute copies of, and to prepare derivative works\n\
based upon these materials; and (2) the right to authorize others to\n\
do any of the foregoing.\n";

#include "sgmlincl.h"

/* SGMLXTRN: Storage allocation and initialization for all public variables.
             Exceptions: Constants lex????? and del????? are defined in
             LEX?????.C modules; constants pcb????? are defined in PCB?????.c.
*/
int badresw = 0;              /* 1=REF_ out of context; 0=valid. */
int charmode = 0;             /* >0=in #CHARS; 0=not. */
int conactsw = 0;             /* 1=return saved content action 0=get new one.*/
int conrefsw = 0;             /* 1=content reference att specified; 0=no. */
int contersv = 0;             /* Save contersw while processing pending REF. */
int contersw = 0;             /* 1=element or #CHARS out of context; 0=valid. */
int datarc = 0;               /* Return code for data: DAF_ or REF_. */
int delmscsw = 0;             /* 1=DELMSC must be read on return to es==0. */
int didreq = 0;               /* 1=required implied tag processed; 0=no. */
int docelsw = 0;	      /* 1=had document element; 0=no */
int dostag = 0;               /* 1=retry newetd instead of parsing; 0=parse. */
int dtdsw = 0;                /* DOCTYPE declaration found: 1=yes; 0=no. */
int entdatsw = 0;             /* 2=CDATA entity; 4=SDATA; 8=NDATA; 0=none. */
int entpisw = 0;              /* 4=PI entity occurred; 0=not. */
int eodsw = 0;                /* 1=eod found in error; 0=not yet. */
int eofsw = 0;                /* 1=eof found in body of document; 0=not yet. */
int es = -1;                  /* Index of current source in stack. */
int etagimct = 0;             /* Implicitly ended elements left on stack. */
int etagimsw = 0;             /* 1=end-tag implied by other end-tag; 0=not. */
int etagmin = MINNONE;        /* Minim: NONE NULL NET DATA; implied by S/ETAG*/
int etictr = 0;               /* Number of "NET enabled" tags on stack. */
int etisw = 0;                /* 1=tag ended with eti; 0=did not. */
int indtdsw = 0;              /* Are we in the DTD? 1=yes; 0=no. */
int mslevel = 0;              /* Nesting level of marked sections. */
int msplevel = 0;             /* Nested MS levels subject to special parse. */
int prologsw = 1;             /* 1=in prolog; 0=not. */
int pss = 0;                  /* SGMLACT: scbsgml stack level. */
int sgmlsw = 0;               /* SGML declaration found: 1=yes; 0=no. */
int stagmin = MINNONE;        /* Minimization: NONE, NULL tag, implied by STAG*/
int tagctr = 0;               /* Tag source chars read. */
int tages = -1;		      /* ES level at start of tag. */
int ts = -1;                  /* Index of current tag in stack. */
struct parse *propcb = &pcbpro; /* Current PCB for prolog parse. */
int aentctr = 0;              /* Number of ENTITY tokens in this att list. */
int conact = 0;               /* Return code from content parse. */
int conrefsv = 0;             /* Save conrefsw when doing implied start-tag.*/
int dtdrefsw = 0;             /* External DTD? 1=yes; 0=no. */
int etiswsv = 0;              /* Save etisw when processing implied start-tag.*/
int grplvl = 0;               /* Current level of nested grps in model. */
int idrctr = 0;               /* Number of IDREF tokens in this att list. */
int mdessv = 0;               /* ES level at start of markup declaration. */
int notadn = 0;               /* Position of NOTATION attribute in list. */
int parmno = 0;               /* Current markup declaration parameter number. */
int pexsw = 0;                /* 1=tag valid solely because of plus exception.*/
int rcessv = 0;               /* ES level at start of RCDATA content. */
int tagdelsw = 0;             /* 1=tag ended with delimiter; 0=no delimiter. */
int tokencnt = 0;             /* Number of tokens found in attribute value. */
struct entity *ecbdeflt = 0;  /* #DEFAULT ecb (NULL if no default entity). */
struct etd *docetd = 0;       /* The etd for the document as a whole. */
struct etd *etagreal = 0;     /* Actual or dummy etd that implied this tag. */
struct etd *newetd = 0;       /* The etd for a start- or end-tag recognized. */
struct etd *nextetd = 0;      /* ETD that must come next (only one choice). */
struct etd *lastetd = 0;      /* most recently ended ETD. */
struct etd *stagreal = 0;     /* Actual or dummy etd that implied this tag. */
struct parse *conpcb = 0;     /* Current PCB for content parse. */
UNCH *data = 0;               /* Pointer to returned data in buffer. */
UNCH *mdname = 0;             /* Name of current markup declaration. */
UNCH *ptcon = 0;              /* Current pointer into tbuf. */
UNCH *ptpro = 0;              /* Current pointer into tbuf. */
UNCH *rbufs = 0;              /* DOS file read area: start position for read. */
UNCH *subdcl = 0;             /* Subject of markup declaration (e.g., GI). */
UNS conradn = 0;              /* 1=CONREF attribute in list (0=no). */
UNS datalen = 0;              /* Length of returned data in buffer. */
UNS entlen = 0;               /* Length of TAG or EXTERNAL entity text. */
UNS idadn = 0;                /* Number of ID attribute (0 if none). */
UNS noteadn = 0;              /* Number of NOTATION attribute (0 if none). */
UNS reqadn = 0;               /* Num of atts with REQUIRED default (0=none). */
int grplongs;		      /* Number of longs for GRPCNT bitvector. */

/* Variable arrays and structures.
*/
struct ad *al = 0;            /* Current attribute list work area. */
struct dcncb *dcntab[1];      /* List of data content notation names. */
struct entity *etab[ENTHASH]; /* Entity hash table. */
struct etd *etdtab[ETDHASH];  /* Element type definition hash table. */
struct fpi fpidf;             /* Fpi for #DEFAULT entity. */
struct id *itab[IDHASH];      /* Unique identifier hash table. */
struct etd **nmgrp = 0;	      /* Element name group */
PDCB *nnmgrp = 0;	      /* Notation name group */
struct restate *scbsgml = 0;  /* SGMLACT: return action state stack. */
struct source *scbs = 0;      /* Stack of open sources ("SCB stack"). */
struct srh *srhtab[1];        /* List of SHORTREF table headers. */
struct sgmlstat ds;           /* Document statistics. */
struct switches sw;           /* Parser control switches set by text proc. */
struct tag *tags = 0;         /* Stack of open elements ("tag stack"). */
struct thdr *gbuf = 0;        /* Buffer for creating group. */
struct thdr prcon[3];         /* 0-2: Model for *DOC content. */
struct thdr undechdr;         /* 0:Default model hdr for undeclared content.*/
UNCH *dtype = 0;              /* Document type name. */
UNCH *entbuf = 0;             /* Buffer for entity reference name. */
UNCH fce[2];                  /* String form of FCE char.
                                 (fce[1] must be EOS).*/
/* Buffer for non-SGML character reference.*/
UNCH nonchbuf[2] = { DELNONCH };
UNCH *tbuf;		      /* Work area for tokenization. */
UNCH *lbuf = 0;		      /* In tbuf: Literal parse area.*/
UNCH *sysibuf = 0;	      /* Buffer for system identifiers. */
UNCH *pubibuf = 0;	      /* Buffer for public identifiers. */
UNCH *nmbuf = 0;	      /* Name buffer used by mdentity. */
struct mpos *savedpos;

/* Constants.
*/
struct map dctab[] = {        /* Keywords for declared content parameter.*/
     { key[KRCDATA],  MRCDATA+MPHRASE },
     { key[KCDATA],   MCDATA+MPHRASE },
     { key[KANY],     MANY+MCHARS+MGI },
     { key[KEMPTY],   MNONE+MPHRASE },
     { NULL,          0 }
};
struct map deftab[] = {       /* Default value keywords. */
     { key[KIMPLIED],  DNULL },
     { key[KREQUIRED], DREQ  },
     { key[KCURRENT],  DCURR },
     { key[KCONREF],   DCONR },
     { key[KFIXED],    DFIXED},
     { NULL,           0}
};
struct map dvtab[] = {        /* Declared value: keywords and type codes.*/
/*                                TYPE      NUMBER   */
/*   grp             ANMTGRP      Case 1 0  Grp size */
/*   grp member      ANMTGRP      Case   0  Position */
/*   grp             ANOTEGRP     Case 1 1  Grp size */
     { key[KNOTATION], ANOTEGRP}, /* Case   1  Position */
     { key[KCDATA],    ACHARS  }, /* Case   2  Always 0 */
     { key[KENTITY],   AENTITY }, /* Case   3  Normal 1 */
     { key[KID],       AID     }, /* Case   4  Normal 1 */
     { key[KIDREF],    AIDREF  }, /* Case   5  Normal 1 */
     { key[KNAME],     ANAME   }, /* Case   6  Normal 1 */
     { key[KNMTOKEN],  ANMTOKE }, /* Case   7  Normal 1 */
     { key[KNUMBER],   ANUMBER }, /* Case   8  Normal 1 */
     { key[KNUTOKEN],  ANUTOKE }, /* Case   9  Normal 1 */
     { key[KENTITIES], AENTITYS}, /* Case   A  Normal 1 */
     { key[KIDREFS],   AIDREFS }, /* Case   B  # tokens */
     { key[KNAMES],    ANAMES  }, /* Case   C  # tokens */
     { key[KNMTOKENS], ANMTOKES}, /* Case   D  # tokens */
     { key[KNUMBERS],  ANUMBERS}, /* Case   E  # tokens */
     { key[KNUTOKENS], ANUTOKES}, /* Case   F  # tokens */
     { NULL,           0 }         /* Case   0  ERROR    */
};
struct map enttab[] = {       /* Entity declaration second parameter. */
     { key[KCDATA],     ESC },
     { key[KSDATA],     ESX },
     { key[KMS],        ESMS},
     { key[KPI],        ESI },
     { key[KSTARTTAG],  ESS },
     { key[KENDTAG],    ESE },
     { key[KMD],        ESMD},
     { NULL,            0 }
};
struct map exttab[] = {       /* Keywords for external identifier. */
     { key[KSYSTEM],    EDSYSTEM },
     { key[KPUBLIC],    EDPUBLIC },
     { NULL,            0 }
};
struct map extettab[] = {       /* Keywords for external entity type. */
     { key[KCDATA],     ESNCDATA },
     { key[KNDATA],     ESNNDATA },
     { key[KSDATA],     ESNSDATA },
     { key[KSUBDOC],    ESNSUB },
     { NULL,            0 }
};
struct map funtab[] = {       /* Function character reference names. */
     { key[KRE],       RECHAR },
     { key[KRS],       RSCHAR },
     { key[KSPACE],    SPCCHAR },
     /* We should use an extra table for added functions. */
     { (UNCH *)"TAB",  TABCHAR },
     { NULL,           0 }
};
struct map mstab[] = {        /* Marked section keywords. */
     { key[KTEMP],    MSTEMP  },
     { key[KINCLUDE], MSTEMP  }, /* Treat INCLUDE like TEMP; both are NOPs.*/
     { key[KRCDATA],  MSRCDATA},
     { key[KCDATA],   MSCDATA },
     { key[KIGNORE],  MSIGNORE},
     { NULL,          0 }
};
struct map pubcltab[] = {     /* Names for public text class. */
     { (UNCH *)"CAPACITY",  FPICAP  },
     { (UNCH *)"CHARSET",   FPICHARS},
     { (UNCH *)"DOCUMENT",  FPIDOC  },
     { (UNCH *)"DTD",       FPIDTD  },
     { (UNCH *)"ELEMENTS",  FPIELEM },
     { (UNCH *)"ENTITIES",  FPIENT  },
     { (UNCH *)"LPD",       FPILPD  },
     { (UNCH *)"NONSGML",   FPINON  },
     { (UNCH *)"NOTATION",  FPINOT  },
     { (UNCH *)"SHORTREF",  FPISHORT},
     { (UNCH *)"SUBDOC",    FPISUB  },
     { (UNCH *)"SYNTAX",    FPISYN  },
     { (UNCH *)"TEXT",      FPITEXT },
     { NULL,            0 }
};
UNCH indefent[] = "\12#DEFAULT";   /* Internal name: default entity name. */
UNCH indefetd[] = "\12*DOCTYPE";   /* Internal name: default document type. */
UNCH indocent[] = "\12*SGMLDOC";   /* Internal name: SGML document entity. */
UNCH indocetd[]  = "\6*DOC";       /* Internal name: document level etd. */
UNCH indtdent[]  = "\11*DTDENT";   /* Internal name: external DTD entity. */

struct etd dumetd[3];
struct entity *dumpecb;
UNCH sgmlkey[] = "SGML";
