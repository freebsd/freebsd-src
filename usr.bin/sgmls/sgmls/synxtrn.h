/* SYNXTRN.H: External declarations for concrete syntax constants.
*/
/* Short References
*/
#define SRCT    32            /* Number of short reference delimiters. */
#define SRMAXLEN 3            /* Maximum length of a SHORTREF delimiter. */
#define SRNPRT   8            /* Number of non-printable SHORTREF delimiters. */
struct srdel {
     struct map dtb[SRCT+2];  /* LEXCNM: Short reference delimiters. */
     char *pdtb[SRNPRT+1];    /* LEXCNM: Printable form of unprintable SRs. */
     int fce;                 /* LEXCNM: Index of first FCE in srdeltab. */
     int hyp2;                /* LEXCNM: Index of "two hyphens" in srdeltab. */
     int data;                /* LEXCNM: Index of first SR with data char. */
     int hyp;                 /* LEXCNM: Index of hyphen in srdeltab. */
     int prtmin;              /* LEXCNM: Index of 1st printable SR. */
     int spc;                 /* LEXCNM: Index of space in srdeltab. */
     int lbr;		      /* LEXCNM: Index of left bracket in srdeltab. */
     int rbr;		      /* LEXCNM: Index of right bracket in srdeltab. */
};
struct delim {
     UNCH genre;              /* LEXCON: Generated RE; cannot be markup. */
     UNCH lit;                /* LEXMARK: Char used as LIT delimiter.*/
     UNCH lita;               /* LEXMARK: Char used as LITA delimiter.*/
     UNCH mdc;                /* LEXLMS: Char used as MDC delimiter.*/
     UNCH msc;                /* LEXCON: Char used as MSC delimiter. */
     UNCH net;                /* LEXCON: Char used as NET when enabled.*/
     UNCH pero;               /* LEXMARK: Char used as PERO delimiter. */
     UNCH pic;                /* LEXCON: Char used as PIC delimiter.*/
     UNCH tago;               /* LEXCON: Char used as TAGO when enabled.*/
};
struct lexcode {
     UNCH fce;                /* LEXCNM: FRE character as entity reference. */
     UNCH fre;                /* LEXCON: Free character not an entity ref. */
     UNCH litc;               /* LEXLMS: Literal close delimiter enabled. */
     UNCH minlitc;	      /* LEXMIN: Literal close delimiter enabled. */
     UNCH msc;                /* LEXLMS: Marked section close delim enabled. */
     UNCH net;                /* LEXCON: Null end-tag delimiter enabled. */
     UNCH nonet;              /* LEXCON: NET disabled; still used as ETI. */
     UNCH spcr;               /* LEXCNM: Space in use as SHORTREF delimiter. */
     UNCH tago;               /* LEXCON: Tag open delimiter enabled. */
     UNCH cde;                /* LEXLMS: CDATA/SDATA delimiters. */
};
struct lexical {
     struct markup m;         /* Markup strings for text processor. */
     struct srdel s;          /* Short reference delimiters. */
     struct delim d;          /* General delimiter characters. */
     struct lexcode l;        /* Lexical table code assignments. */
};
extern struct lexical lex;    /* Delimiter set constants. */
extern UNCH lexcnm[];         /* Lexical table: mixed content. */
extern UNCH lexcon[];         /* Lexical table for content (except mixed). */
extern UNCH lexgrp[];         /* Lexical table for groups. */
extern UNCH lexlms[];         /* Lexical table: literals and marked sections. */
extern UNCH lexmin[];         /* Lexical table: minimum data literal. */
extern UNCH lexmark[];        /* Lexical table for markup. */
extern UNCH lexsd[];          /* Lexical table for SGML declaration. */
extern UNCH lextran[];        /* Case translation table for SGML names. */
extern UNCH lextoke[];        /* Lexical table for tokenization. */
extern UNCH *lextabs[];       /* List of all lexical tables. */
extern struct parse pcbconc;  /* PCB: character data. */
extern struct parse pcbcone;  /* PCB: element content (no data allowed). */
extern struct parse pcbconm;  /* PCB: mixed content (data allowed). */
extern struct parse pcbconr;  /* PCB: replaceable character data. */
extern struct parse pcbetag;  /* PCB: end-tags. */
extern struct parse pcbgrcm;  /* PCB: content model group. */
extern struct parse pcbgrcs;  /* PCB: content model suffix. */
extern struct parse pcbgrnm;  /* PCB: name group. */
extern struct parse pcbgrnt;  /* PCB: name token group. */
extern struct parse pcblitc;  /* PCB: literal with CDATA. */
extern struct parse pcblitp;  /* PCB: literal with CDATA, parm & char refs. */
extern struct parse pcblitr;  /* PCB: attribute value with general refs. */
extern struct parse pcblitt;  /* PCB: tokenized attribute value. */
extern struct parse pcblitv;  /* PCB: literal with CDATA, function char trans.*/
extern struct parse pcbmd;    /* PCB: markup declaration. */
extern struct parse pcbmdc;   /* PCB: comment declaration. */
extern struct parse pcbmdi;   /* PCB: markup declaration (ignored). */
extern struct parse pcbmds;   /* PCB: markup declaration subset. */
extern struct parse pcbmsc;   /* PCB: marked section in CDATA mode. */
extern struct parse pcbmsi;   /* PCB: marked section in IGNORE mode. */
extern struct parse pcbmsrc;  /* PCB: marked section in RCDATA mode. */
extern struct parse pcbpro;   /* PCB: prolog. */
extern struct parse pcbref;   /* PCB: reference. */
extern struct parse pcbstag;  /* PCB: start-tag. */
extern struct parse pcbval;   /* PCB: attribute value. */
extern struct parse pcbeal;   /* PCB: end of attribute list. */
extern struct parse pcbsd;    /* PCB: SGML declaration. */
extern int pcbcnda;           /* PCBCONM: data in buffer. */
extern int pcbcnet;           /* PCBCONM: markup found or data buffer flushed.*/
extern int pcbmdtk;           /* PCBMD: token expected. */
extern int pcbstan;           /* PCBSTAG: attribute name expected. */
extern int pcblittda;	      /* PCBLITT: data character found */

#define KANY 0
#define KATTLIST 1
#define KCDATA 2
#define KCONREF 3
#define KCURRENT 4
#define KDEFAULT 5
#define KDOCTYPE 6
#define KELEMENT 7
#define KEMPTY 8
#define KENDTAG 9
#define KENTITIES 10
#define KENTITY 11
#define KFIXED 12
#define KID 13
#define KIDLINK 14
#define KIDREF 15
#define KIDREFS 16
#define KIGNORE 17
#define KIMPLIED 18
#define KINCLUDE 19
#define KINITIAL 20
#define KLINK 21
#define KLINKTYPE 22
#define KMD 23
#define KMS 24
#define KNAME 25
#define KNAMES 26
#define KNDATA 27
#define KNMTOKEN 28
#define KNMTOKENS 29
#define KNOTATION 30
#define KNUMBER 31
#define KNUMBERS 32
#define KNUTOKEN 33
#define KNUTOKENS 34
#define KO 35
#define KPCDATA 36
#define KPI 37
#define KPOSTLINK 38
#define KPUBLIC 39
#define KRCDATA 40
#define KRE 41
#define KREQUIRED 42
#define KRESTORE 43
#define KRS 44
#define KSDATA 45
#define KSHORTREF 46
#define KSIMPLE 47
#define KSPACE 48
#define KSTARTTAG 49
#define KSUBDOC 50
#define KSYSTEM 51
#define KTEMP 52
#define KUSELINK 53
#define KUSEMAP 54

#define NKEYS (KUSEMAP+1)

extern UNCH key[NKEYS][REFNAMELEN+1];

/* Holds the SGML keyword (not alterable by concrete syntax). */
extern UNCH sgmlkey[];
