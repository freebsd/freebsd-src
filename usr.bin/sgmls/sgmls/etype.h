/* ETYPE.H: Definitions for element type and group processing.
*/
#define MCHARS    0x80        /* Model: contains #CHARS. */
#define MGI       0x40        /* Model: contains GI names. */
#define MPHRASE   0x20        /* Model: first token is #CHARS. */
#define MKEYWORD  0x1F        /* Model: defined with single keyword. */
#define MNONE     0x10        /* Model: contains no GIs or #CHARS. */
#define MANY      0x08        /* Model: contains any GIs or #CHARS. */
#define MRCDATA   0x04        /* Model: contains RCDATA. */
#define MCDATA    0x02        /* Model: contains CDATA. */

#define TOREP (TOPT+TREP)     /* 11000000 Optional and repeatable. */
#define TOPT      0x80        /* Token: 1=optional; 0=required. */
#define TREP      0x40        /* Token: 1=repeatable; 0=not. */
#define TXOREP (TXOPT+TXREP)  /* * explicitly specified */
#define TXOPT     0x20	      /* ? explicitly specified */
#define TXREP     0x10	      /* + explicitly specified */
#define TTMASK    0x0F        /* 00001111 Mask for testing token type. */
#define TTETD        4        /* 00000100 Token is an ETD. */
#define TTAND        3        /* 00000011 Token is an AND group. */
#define TTSEQ        2        /* 00000010 Token is a sequence group. */
#define TTOR         1        /* 00000001 Token is an OR group. */
#define TTCHARS      0        /* 00000000 Token is #CHARS. */

struct thdr {                 /* Token header or model header. */
     UNCH ttype;              /* Token type attributes or model content. */
     union {
          int tnum;           /* Group token: tokens in group.
				 Model header: content tokens at any level. */
          struct etd *thetd;  /* GI token: ptr to etd. */
     } tu;
};
#define THSZ (sizeof(struct thdr))

#define ETDHASH   211	      /* Size of element hash table. Must be prime. */
#define SMO       0x40        /* ETDMIN: Start-tag O minimization. */
#define EMO       0x04        /* ETDMIN: End-tag O minimization. */
#define EMM       0x02	      /* ETDMIN: End-tag minimization explicitly
				 specified to be minus */
#define ETDDCL    0x80        /* ETDMIN: Element was declared. */
#define ETDUSED   0x20        /* ETDMIN: Element used in another declaration. */
#define ETDOCC    0x10        /* ETDMIN: Element occurred in document. */

struct etd {                  /* Element type definition. */
     struct etd *etdnext;     /* Next element type definition in hash chain. */
     UNCH *etdgi;	      /* GI preceded by its length, followed by EOS. */
     UNCH etdmin;             /* Flag bits: minimization. */
     UNCH mark;		      /* Mark bit: for ambiguity checking */
     struct thdr *etdmod;     /* Content model. */
     struct etd **etdmex;     /* Minus exceptions. */
     struct etd **etdpex;     /* Plus exceptions. */
     struct ad *adl;          /* Attribute descriptor list. */
     struct entity **etdsrm;  /* Short reference map. */
};
#define ETDSZ (sizeof(struct etd))
typedef struct etd *PETD;
extern struct etd dumetd[];

/* Number of bits in a long must be >= 1<<LONGPOW */
#define LONGPOW 5

#define LONGBITS (1<<LONGPOW)

struct mpos {                 /* Position of current element in model. */
     UNCH g;                  /* Index of this group in the model. */
     UNCH t;                  /* Index of the current token in this group. */
     unsigned long *h;	      /* Hit bits of this group's tokens. */
};

#define HITCLEAR(h) MEMZERO((UNIV)(h), grplongs*sizeof(unsigned long))

#define TAGCONER  0x01        /* 00000001 (contersw) Tag was out of context. */
#define TAGNET    0x02        /* 00000010 (etisw)    Tag has NET enabled. */
#define TAGPEX    0x04        /* 00000100 (pexsw)    Tag was plus exception. */
#define TAGREF    0x08        /* 00001000 (conrefsw) Tag had CONREF or EMPTY.*/
struct tag {                  /* Tag control block. */
     UNCH   status;           /* Status of context check. */
     UNCH   tflags;           /* Flags: TAGCONER TAGNET TAGPEX TAGREF */
     struct etd *tetd;        /* Element type definition for tag. */
     struct entity **tsrm;    /* Current short reference map. */
     struct mpos *tpos;       /* Position of next tag in this model. */
};

#define RCEND    1            /* No more tokens: end element and retry GI. */
#define RCREQ    2            /* Required GI must precede proposed GI. */
#define RCMISS   3            /* GI invalid: not element end; no required GI. */
#define RCHIT    4            /* GI is the one expected next. */
#define RCMEX    5            /* GI invalid: minus exception. */
#define RCHITMEX 6            /* RCMEX with invalid attempted minus exclusion.*/
#define RCPEX    7            /* GI is valid solely because of plus exclusion.*/
#define RCNREQ   8            /* Token is not required; can retry invalid GI. */
