/* SGMLMAIN: Main interface to SGML services.

Preprocessor variable names are the only supported interface
to data maintained by SGML.  They are defined in this file or in adl.h.
*/
/* Return control block types (RCBTYPE) from calls to parser (SGML):
   Names and strings follow the convention for the IPBs.
*/
enum sgmlevent {
     SGMLEOD,			/* End of document. */
     SGMLDAF,			/* Data found. */
     SGMLSTG,			/* Start-tag found. */
     SGMLETG,			/* End-tag found. */
     SGMLREF,			/* Record end found. */
     SGMLPIS,			/* Processing instruction (string). */
     SGMLAPP			/* APPINFO (string) */
};

struct rcbdata {              /* Return control block: DAF EOD REF PIS APP. */
     UNS contersw;            /* 1=context error; 2,4,8=data type; 0=not. */
     UNS datalen;             /* Length of data or PI (0=single nonchar). */
     UNCH *data;              /* Data, PI, single nonSGML, or NDATA ecb ptr. */
};

struct rcbtag {               /* Return control block for STG and ETG. */
     UNS contersw;            /* 1=context error; 2=NET enabled; 0/0=not. */
     UNS tagmin;              /* Minim: NONE NULL NET DATA; implied by S/ETAG */
     UNCH *curgi;	      /* Start-tag (or end-tag) GI. */
     union {
          struct ad *al;      /* Start-tag: attribute list. */
          UNCH *oldgi;        /* End-tag: resumed GI. */
     } ru;
     struct ad *lal;          /* Start-tag: link attribute list (UNUSED). */
     UNS format;              /* Format class for default processing. */
     struct etd *tagreal;     /* Dummy etd or ptr to GI that implied this tag.*/
     int etictr;              /* Number of elements on stack with NET enabled.*/
     UNCH *srmnm;             /* Current SHORTREF map name (NULL=#EMPTY). */
};

/* Accessors for rcbdata and rcbtag. */
/* Datatype abbreviations: C=unsigned char  S=string  U=unsigned int L=4 bytes
                           A=array  P=ptr to structure N=name (see sgmlcb.h)
*/
/* Data control block fields: processing instructions (SGMLPIS).
*/
#define PDATA(d) ((d).data)            /*S  PI string. */
#define PDATALEN(d) ((d).datalen)      /*U  Length of PI string. */
#define PIESW(d) (((d).contersw & 4))  /*U  1=PIDATA entity returned. */
/* Data control block fields: other data types.
*/
#define CDATA(d) ((d).data)            /*S  CDATA content string. */
#define CDATALEN(d) ((d).datalen)      /*U  Length of CDATA content string. */
#define CONTERSW(d) (((d).contersw &1))/*U  1=CDATA or TAG out of context. */
#define CDESW(d) (((d).contersw & 2))  /*U  1=CDATA entity returned. */
#define SDESW(d) (((d).contersw & 4))  /*U  1=SDATA entity returned. */
#define NDESW(d) (((d).contersw & 8))  /*U  1=NDATA entity returned. */
#define NEPTR(d) ((PNE)(d).data)       /*P  Ptr to NDATA control block. */
#define MARKUP(d) ((d).data)           /*A  Markup delimiter strings. */
#define DTYPELEN(d) ((d).datalen)      /*U  Length of doc type name +len+EOS. */
#define DOCTYPE(d) ((d).data)          /*S  Document type name (with len+EOS). */
#define ADATA(d) ((d).data)	       /*S  APPINFO */
#define ADATALEN(d) ((d).datalen)      /*U  Length of APPINFO string.  */
/* Tag control block fields.
*/
#define ALPTR(t) ((t).ru.al)           /*P  Ptr to SGML attribute list. */
#define CURGI(t) ((t).curgi+1)         /*N  GI of started or ended element. */
#define OLDGI(t) ((t).ru.oldgi)        /*S  GI of resumed element. */
#define TAGMIN(t) (t).tagmin          /*U  Minimization for current tag. */
#define TAGREAL(t) ((t).tagreal)      /*P  Dummy etd that implied this tag. */
#define TAGRLNM(t) ((UNCH *)(t).tagreal)  /*P GI of tag that implied this tag.*/
#define ETISW(t) (((t).contersw & 2))  /*U  1=NET delimiter enabled by ETI. */
#define PEXSW(t) (((t).contersw & 4))  /*U  1=Element was plus exception. */
#define MTYSW(t) (((t).contersw & 8))  /*U  1=Element is empty. */
#define ETICTR(t) ((t).etictr)         /*U  Number of active NET delimiters. */
#define SRMNM(t) ((t).srmnm)           /*S  Name of current SHORTREF map. */
#define SRMCNT(t) ((t).contersw)       /*U  Number of SHORTREF maps defined. */
#define FORMAT(t) ((t).format)         /*U  Format class.*/

/* These function names are chosen so as to be distinct in the first 6
letters. */

/* Initialize. */
struct markup *sgmlset P((struct switches *));
/* Cleanup and return capacity usage statistics. */
VOID sgmlend P((struct sgmlcap *));
/* Set document entity. */
int sgmlsdoc P((UNIV));
/* Get entity. */
int sgmlgent P((UNCH *, PNE *, UNCH **));
/* Mark an entity.  Return is non-zero if already marked.*/
int sgmlment P((UNCH *));
/* Get the next sgml event. */
enum sgmlevent sgmlnext P((struct rcbdata *, struct rcbtag *));
/* Get the error count. */
int sgmlgcnterr P((void));
/* Get the current location. */
int sgmlloc P((unsigned long *, char **));
/* Write out the SGML declaration. */
VOID sgmlwrsd P((FILE *));
/* Note subdocument capacity usage. */
VOID sgmlsubcap P((long *));
