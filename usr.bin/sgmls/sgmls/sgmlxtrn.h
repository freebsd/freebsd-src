/* SGMLXTRN.H: External declarations for SGML public variables.
               Exceptions: Constants lex????? and del????? are defined in
               LEX?????.C modules; constants pcb????? are defined in PCB?????.c.
*/
#ifndef SGMLXTRN              /* Don't include this file more than once. */
#define SGMLXTRN
extern int badresw;           /* 1=REF_ out of context; 0=valid. */
extern int charmode;          /* >0=in #CHARS; 0=not. */
extern int conactsw;          /* 1=return saved content action 0=get new one.*/
extern int conrefsw;          /* 1=content reference att specified; 0=no. */
extern int contersv;          /* Save contersw while processing pending REF. */
extern int contersw;          /* 1=element or #CHARS out of context; 0=valid. */
extern int datarc;            /* Return code for data: DAF_ or REF_. */
extern int delmscsw;          /* 1=DELMSC must be read on return to es==0. */
extern int didreq;            /* 1=required implied tag processed; 0=no. */
extern int dostag;            /* 1=retry newetd instead of parsing; 0=parse. */
extern int dtdsw;             /* DOCTYPE declaration found: 1=yes; 0=no. */
extern int entdatsw;          /* 2=CDATA entity; 4=SDATA; 8=NDATA; 0=none. */
extern int entpisw;           /* 4=PI entity occurred; 0=not. */
extern int eodsw;             /* 1=eod found in error; 0=not yet. */
extern int eofsw;             /* 1=eof found in body of document; 0=not yet. */
extern int etagimct;          /* Implicitly ended elements left on stack. */
extern int etagimsw;          /* 1=end-tag implied by other end-tag; 0=not. */
extern int etagmin;           /* Minim: NONE NULL NET DATA; implied by S/ETAG*/
extern int etictr;            /* Number of "NET enabled" tags on stack. */
extern int etisw;             /* 1=tag ended with eti; 0=did not. */
extern int indtdsw;           /* Are we in the DTD? 1=yes; 0=no. */
extern int mslevel;           /* Nesting level of marked sections. */
extern int msplevel;          /* Nested MS levels subject to special parse. */
extern int prologsw;          /* 1=in prolog; 0=not. */
extern int pss;               /* SGMLACT: scbsgml stack level. */
extern int sgmlsw;            /* SGML declaration found: 1=yes; 0=no. */
extern int stagmin;           /* Minimization: NONE, NULL tag, implied by STAG*/
extern int tagctr;            /* Tag source chars read. */
extern int ts;                /* Index of current tag in stack. */
extern struct parse *propcb;  /* Current PCB for prolog parse. */
extern int aentctr;           /* Number of ENTITY tokens in this att list. */
extern int conact;            /* Return code from content parse. */
extern int conrefsv;          /* Save conrefsw when doing implied start-tag.*/
extern int dtdrefsw;          /* External DTD? 1=yes; 0=no. */
extern int etiswsv;           /* Save etisw when processing implied start-tag.*/
extern int grplvl;            /* Current level of nested grps in model. */
extern int idrctr;            /* Number of IDREF tokens in this att list. */
extern int mdessv;            /* ES level at start of markup declaration. */
extern int notadn;            /* Position of NOTATION attribute in list. */
extern int parmno;            /* Current markup declaration parameter number. */
extern int pexsw;             /* 1=tag valid solely because of plus exception.*/
extern int rcessv;            /* ES level at start of RCDATA content. */
extern int tagdelsw;          /* 1=tag ended with delimiter; 0=no delimiter. */
extern int tokencnt;          /* Number of tokens found in attribute value. */
extern struct entity *ecbdeflt;  /* #DEFAULT ecb (NULL if no default entity). */
extern struct etd *docetd;    /* The etd for the document as a whole. */
extern struct etd *etagreal;  /* Actual or dummy etd that implied this tag. */
extern struct etd *newetd;    /* The etd for a start- or end-tag recognized. */
extern struct etd *nextetd;   /* ETD that must come next (only one choice). */
extern struct etd *stagreal;  /* Actual or dummy etd that implied this tag. */
extern struct parse *conpcb;  /* Current PCB for content parse. */
extern UNCH *data;            /* Pointer to returned data in buffer. */
extern UNCH *mdname;          /* Name of current markup declaration. */
extern UNCH *ptcon;           /* Current pointer into tbuf. */
extern UNCH *ptpro;           /* Current pointer into tbuf. */
extern UNCH *rbufs;           /* DOS file read area: start position for read. */
extern UNCH *subdcl;          /* Subject of markup declaration (e.g., GI). */
extern int Tstart;	      /* Save starting token for AND group testing. */
extern UNS conradn;           /* 1=CONREF attribute in list (0=no). */
extern UNS datalen;           /* Length of returned data in buffer. */
extern UNS entlen;            /* Length of TAG or EXTERNAL entity text. */
extern UNS idadn;             /* Number of ID attribute (0 if none). */
extern UNS noteadn;           /* Number of NOTATION attribute (0 if none). */
extern UNS reqadn;            /* Num of atts with REQUIRED default (0=none). */
extern int grplongs;	      /* Number of longs for GRPCNT bitvector. */
/* Variable arrays and structures.
*/
extern struct ad *al;        /* Current attribute list work area. */
extern struct dcncb *dcntab[];/* List of data content notation names. */
extern struct entity *etab[]; /* Entity hash table. */
extern struct etd *etdtab[];  /* Element type definition hash table. */
extern struct fpi fpidf;      /* Fpi for #DEFAULT entity. */
extern struct id *itab[];     /* Unique identifier hash table. */
extern struct etd **nmgrp;    /* Element name group */
extern PDCB *nnmgrp;	      /* Notation name group */
extern struct restate *scbsgml;  /* SGMLACT: return action state stack. */
extern struct srh *srhtab[];  /* List of SHORTREF table headers. */
extern struct sgmlstat ds;    /* Document statistics. */
extern struct switches sw;    /* Parser control switches set by text proc. */
extern struct tag *tags;     /* Stack of open elements ("tag stack"). */
extern struct thdr *gbuf;    /* Buffer for creating group. */
extern struct thdr prcon[];   /* 0-2: Model for *DOC content. */
extern struct thdr undechdr;  /* 0: Default model hdr for undeclared content. */
extern UNCH *dtype;           /* Document type name. */
extern UNCH *entbuf;          /* Buffer for entity reference name. */
extern UNCH fce[];            /* String form of FCE char (fce[1] must be EOS).*/
extern UNCH nonchbuf[];       /* Buffer for valid nonchar character reference.*/
extern UNCH *tbuf;           /* Work area for tokenization. */
extern UNCH *lbuf;            /* In tbuf: Literal parse area; TAGLEN limit.*/
extern struct entity *dumpecb; /* SRMNULL points to this. */
extern UNCH *sysibuf;
extern UNCH *pubibuf;
extern UNCH *nmbuf;	       /* Name buffer used by mdentity. */
extern struct mpos *savedpos;

/* Constants.
*/
extern int scbsgmnr;          /* SCBSGML: new record; do not ignore RE. */
extern int scbsgmst;          /* SCBSGML: trailing stag or markup; ignore RE. */
extern struct map dctab[];    /* Keywords for declared content parameter. */
extern struct map deftab[];   /* Default value keywords. */
extern struct map dvtab[];    /* Declared value: keywords and type codes.*/
extern struct map enttab[];   /* Entity declaration second parameter. */
extern struct map exttab[];   /* Keywords for external identifier. */
extern struct map extettab[]; /* Keywords for external entity type. */
extern struct map funtab[];   /* Function character reference names. */
extern struct map mstab[];    /* Marked section keywords. */
extern struct map pubcltab[]; /* Keywords for public text class. */
extern UNCH indefent[];       /* Internal name: default entity name. */
extern UNCH indefetd[];       /* Internal name: default document type. */
extern UNCH indocent[];       /* Internal name: SGML document entity. */
extern UNCH indocetd[];       /* Internal name: etd for document as a whole. */
extern UNCH indtdent[];       /* Internal name: external DTD entity. */
extern char license[];        /* SGML Users' Group free license. */
#endif /* ndef SGMLXTRN */
