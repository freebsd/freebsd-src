#include "sgmlincl.h"         /* #INCLUDE statements for SGML parser. */

#define ETDCON (tags[ts].tetd->etdmod->ttype)     /* ETD content flags. */

/* SGML: Main SGML driver routine.
*/
enum sgmlevent sgmlnext(rcbdafp, rcbtagp)
struct rcbdata *rcbdafp;
struct rcbtag *rcbtagp;
{
     while (prologsw && !conactsw) {
	  int oconact;
          conact = parsepro();
          conactsw = 0;       /* Assume sgmlact() will not be skipped. */
          switch(conact) {

          case PIS_:
          case EOD_:
	  case APP_:	       /* APPINFO */
               conactsw = 1;   /* We can skip sgmlact() in opening state. */
               break;

          case DAF_:
               newetd = stagreal = ETDCDATA;
               conact = stag(datarc = DAF_);
               conactsw = 1;   /* We can skip sgmlact() in opening state. */
               prologsw = 0;   /* End the prolog. */
               break;
	  case DCE_:
	  case MSS_:
	       /* prcon[2].tu.thetd holds the etd for the document element. */
	       newetd = stagreal = prcon[2].tu.thetd;
	       stagmin = MINSTAG; /* This tag was minimized. */
	       /* It's an error if the start tag of the document element
		  is not minimizable. */
	       if (BITOFF(newetd->etdmin, SMO))
		    sgmlerr(226, conpcb, (UNCH *)0, (UNCH *)0);
	       oconact = conact; /* Save conact. */
	       conact = stag(0); /* Start the document element. */
	       conactsw = 1;	/* conact needs processing. */
	       prologsw = 0;	/* The prolog is finished. */
	       if (oconact == MSS_) {
		    if (msplevel==0) conpcb = getpcb((int)ETDCON);
		    conpcb = mdms(tbuf, conpcb); /* Parse the marked section
						    start. */
	       }
	       break;
          default:             /* STE_: not defined in SGMLACT.H. */
               if (msplevel==0) conpcb = getpcb((int)ETDCON);
               prologsw = 0;   /* End the prolog. */
               break;
          }
     }
     for (;;) {
	  unsigned swact;  /* Switch action: saved conact, new, or sgmlact.*/

          if (conactsw) {
	       conactsw = 0;
	       swact = conact;
	       contersw = contersv;
	  }
          else {
	       conact = parsecon(tbuf, conpcb);
	       swact = sgmlact((UNCH)(conact != EOD_ ? conact : LOP_));
	  }

          switch (swact) {

          case MD_:           /* Process markup declaration. */
               parsenm(tbuf, NAMECASE); /* Get declaration name. */
               if (!ustrcmp(tbuf+1, key[KUSEMAP])) mdsrmuse(tbuf);
               else sgmlerr(E_MDNAME, conpcb, tbuf+1, (UNCH *)0);
               continue;
          case MDC_:           /* Process markup declaration comment. */
               if (*FPOS!=lex.d.mdc)
                    parsemd(tbuf, NAMECASE, (struct parse *)0, NAMELEN);
               continue;

          case MSS_:           /* Process marked section start. */
               conpcb = mdms(tbuf, conpcb);
               continue;
          case MSE_:           /* Process marked section end (drop to LOP_). */
               if (mdmse()) conpcb = getpcb((int)ETDCON);
               continue;

          case PIS_:           /* Return processing instruction (string). */
               if (entpisw) rcbdafp->data = data;
               else {
                    parselit(tbuf, &pcblitc, PILEN, lex.d.pic);
                    rcbdafp->data = tbuf;
               }
               rcbdafp->datalen = datalen;
               rcbdafp->contersw = entpisw;
	       entpisw = 0;             /* Reset for next time.*/
               scbset();                /* Update location in current scb. */
               return SGMLPIS;

	  case APP_:
	       rcbdafp->data = tbuf;
	       rcbdafp->datalen = ustrlen(tbuf);
	       rcbdafp->contersw = 0;
	       scbset();
	       return SGMLAPP;
          case ETG_:               /* Return end-tag. */
               charmode = 0;       /* Not in char mode unless CDATA or RCDATA.*/
               if (msplevel==0) conpcb = getpcb((int)ETDCON);
               rcbtagp->contersw = tags[ts+1].tflags;
               rcbtagp->tagmin = etagimsw ? MINETAG : etagmin;
               rcbtagp->curgi = tags[ts+1].tetd->etdgi;
               rcbtagp->ru.oldgi = tags[ts].tetd->etdgi;
               if (etagmin==MINSTAG) rcbtagp->tagreal =
                     BADPTR(stagreal) ? stagreal : (PETD)stagreal->etdgi;
               else rcbtagp->tagreal =
                     BADPTR(etagreal) ? etagreal : (PETD)etagreal->etdgi;
               rcbtagp->etictr = etictr;
               rcbtagp->srmnm = tags[ts].tsrm!=SRMNULL ? tags[ts].tsrm[0]->ename
                                                      : 0;
               scbset();                /* Update location in current scb. */
               return SGMLETG;

          case STG_:               /* Return start-tag. */
               charmode = 0;       /* Not in char mode unless CDATA or RCDATA.*/
               if (!conrefsw && msplevel==0) conpcb = getpcb((int)ETDCON);
               rcbtagp->contersw = tags[ts].tflags;
               rcbtagp->tagmin = dostag ? MINSTAG : stagmin;
               rcbtagp->curgi = tags[ts].tetd->etdgi;
               /* Get attribute list if one was defined for this element. */
               rcbtagp->ru.al = !tags[ts].tetd->adl ? 0 :
                    rcbtagp->tagmin==MINNONE  ? al : tags[ts].tetd->adl;
               rcbtagp->tagreal = BADPTR(stagreal)?stagreal:(PETD)stagreal->etdgi;
               rcbtagp->etictr = etictr;
               rcbtagp->srmnm = tags[ts].tsrm!=SRMNULL ? tags[ts].tsrm[0]->ename
                                                      : 0;
               scbset();                /* Update location in current scb. */
               return SGMLSTG;

          case DAF_:               /* Return data in source entity buffer. */
               charmode = 1;
               rcbdafp->datalen = datalen;
               rcbdafp->data = data;
               rcbdafp->contersw = contersw | entdatsw;
                               contersw = entdatsw = 0;/* Reset for next time.*/
               scbset();                /* Update location in current scb. */
               return SGMLDAF;

          case CON_:               /* Process conact after returning REF_. */
               conactsw = 1;
               contersv = contersw;
          case REF_:               /* Return RE found. */
               if (badresw) {
                    badresw = 0;
                    sgmlerr(E_CHARS, &pcbconm, tags[ts].tetd->etdgi+1, (UNCH *)0);
                    continue;
               }
               charmode = 1;
               rcbdafp->contersw = contersw;
               contersw = 0;        /* Reset for next time.*/
               scbset();                /* Update location in current scb. */
               return SGMLREF;

          case EOD_:               /* End of source document entity. */
               if (mslevel != 0) sgmlerr(139, conpcb, (UNCH *)0, (UNCH *)0);
	       idrck();		        /* Check idrefs. */
               scbset();                /* Update location in current scb. */
               return SGMLEOD;

          default:             /* LOP_: Loop again with no action. */
               continue;
          }
     }
}
/* PCBSGML: State and action table for action codes returned to text processor
            by SGML.C.
            Columns are based on SGMLACT.H values minus DAF_, except that end
            of document has input code LOP_, regardless of its action code.
*/
/* Symbols for state names (end with a number). */
#define ST1     0   /* Just had a start tag. */
#define NR1     2   /* Just had an RS or RE. */
#define DA1     4   /* Just had some data. */
#define NR2     6   /* Just had an RE; RE pending. */
#define ST2     8   /* Had only markup since last RE/RS; RE pending. */

static UNCH sgmltab[][11] = {
/*daf_ etg_ md_  mdc_ mss_ mse_ pis_ ref_ stg_ rsr_ eod  */
 {DA1 ,DA1 ,ST1 ,ST1 ,ST1 ,ST1 ,ST1 ,NR1 ,ST1 ,NR1 ,ST1 },/*st1*/
 {DAF_,ETG_,MD_ ,MDC_,MSS_,MSE_,PIS_,LOP_,STG_,LOP_,EOD_},

 {DA1 ,DA1 ,ST1 ,ST1 ,ST1 ,ST1 ,ST1 ,NR2 ,ST1 ,NR1 ,ST1 },/*nr1*/
 {DAF_,ETG_,MD_ ,MDC_,MSS_,MSE_,PIS_,LOP_,STG_,LOP_,EOD_},

 {DA1 ,DA1 ,DA1 ,DA1 ,DA1 ,DA1 ,DA1 ,NR2 ,ST1 ,NR1 ,ST1 },/*da1*/
 {DAF_,ETG_,MD_ ,MDC_,MSS_,MSE_,PIS_,LOP_,STG_,LOP_,EOD_},

 {DA1 ,DA1 ,ST2 ,ST2 ,ST2 ,ST2 ,ST2 ,NR2 ,ST1 ,NR2 ,ST1 },/*nr2*/
 {CON_,ETG_,MD_ ,MDC_,MSS_,MSE_,PIS_,REF_,CON_,LOP_,EOD_},

 {DA1 ,DA1 ,ST2 ,ST2 ,ST2 ,ST2 ,ST2 ,NR1 ,ST1 ,NR2 ,ST1 },/*st2*/
 {CON_,ETG_,MD_ ,MDC_,MSS_,MSE_,PIS_,REF_,CON_,LOP_,EOD_},
};
int scbsgmst = ST1;           /* SCBSGML: trailing stag or markup; ignore RE. */
int scbsgmnr = NR1;           /* SCBSGML: new record; do not ignore RE. */
/* SGMLACT: Determine action to be taken by SGML.C based on current state and
            specified input.
            For start or end of a plus exception element, push or pop the
            pcbsgml stack.
            Return to caller with action code.
*/
#ifdef USE_PROTOTYPES
int sgmlact(UNCH conret)
#else
int sgmlact(conret)
UNCH conret;                  /* Action returned to SGML.C by content parse. */
#endif
{
     int action;

     if (conret==STG_ && GET(tags[ts].tflags, TAGPEX))
          {++pss; scbsgml[pss].snext = ST1;}
     scbsgml[pss].sstate = scbsgml[pss].snext;
     scbsgml[pss].snext = sgmltab[scbsgml[pss].sstate]
                                    [scbsgml[pss].sinput = conret-DAF_];
     scbsgml[pss].saction = sgmltab[scbsgml[pss].sstate+1][scbsgml[pss].sinput];
     TRACEGML(scbsgml, pss, conactsw, conact);
     action = scbsgml[pss].saction;
     if (conret==ETG_ && GET(tags[ts+1].tflags, TAGPEX)) {
	  pss--;
	  /* An included subelement affects the enclosing state like a
	     processing instruction (or MDC_ or MD_),
	     that is to say NR1 is changed to ST1 and NR2 to ST2. */
	  scbsgml[pss].sstate = scbsgml[pss].snext;
	  scbsgml[pss].snext = sgmltab[scbsgml[pss].sstate][PIS_ - DAF_];
     }
     return action;
}
/* GETPCB: Choose pcb for new or resumed element.
*/
struct parse *getpcb(etdcon)
int etdcon;                   /* Content type of new or resumed element. */
{
     if (BITON(etdcon, MGI)) {
          return(BITON(etdcon, MCHARS) ? &pcbconm : &pcbcone);
     }
     if (BITON(etdcon, MCDATA) || BITON(etdcon, MRCDATA)) {
         charmode = 1;
         return(BITON(etdcon, MCDATA) ? &pcbconc : (rcessv = es, &pcbconr));
     }
     return(&pcbconm);
}

struct markup *sgmlset(swp)
struct switches *swp;
{
     /* Initialize variables based on switches structure members. */
     sw = *swp;
     rbufs = (UNCH *)rmalloc((UNS)3+sw.swbufsz) + 3; /* DOS file read area. */
     TRACEPRO();         /* Set trace switches for prolog. */
     msginit(swp);
     ioinit(swp);
     sdinit();
     return &lex.m;
}

/* Points for each capacity, indexed by *CAP in sgmldecl.h.  We'll replace
2 with the real NAMELEN at run time. */

static UNCH cappoints[] = {
     1,
     2,
     1,
     2,
     2,
     2,
     2,
     2,
     1,
     2,
     2,
     1,
     2,
     2,
     2,
     2,
     2
};

static long capnumber[NCAPACITY];
static long maxsubcap[NCAPACITY];

VOID sgmlend(p)
struct sgmlcap *p;
{
     int i;
     for (; es >= 0; --es)
	  if (FILESW)
	       fileclos();

     capnumber[NOTCAP] = ds.dcncnt;
     capnumber[EXGRPCAP] = ds.pmexgcnt;
     capnumber[ELEMCAP] = ds.etdcnt+ds.etdercnt;
     capnumber[EXNMCAP] = ds.pmexcnt;
     capnumber[GRPCAP] = ds.modcnt;
     capnumber[ATTCAP] = ds.attcnt;
     capnumber[ATTCHCAP] = ds.attdef;
     capnumber[AVGRPCAP] = ds.attgcnt;
     capnumber[IDCAP] = ds.idcnt;
     capnumber[IDREFCAP] = ds.idrcnt;
     capnumber[ENTCAP] = ds.ecbcnt;
     capnumber[ENTCHCAP] = ds.ecbtext;
     capnumber[MAPCAP] = ds.srcnt + ds.srcnt*lex.s.dtb[0].mapdata;
     capnumber[NOTCHCAP] = ds.dcntext;

     capnumber[TOTALCAP] = 0;

     for (i = 1; i < NCAPACITY; i++) {
	  if (cappoints[i] > 1)
	       cappoints[i] = NAMELEN;
	  capnumber[i] += maxsubcap[i]/cappoints[i];
	  capnumber[TOTALCAP] += (long)capnumber[i] * cappoints[i];
     }
     p->number = capnumber;
     p->points = cappoints;
     p->limit = sd.capacity;
     p->name = captab;

     for (i = 0; i < NCAPACITY; i++) {
	  long excess = capnumber[i]*cappoints[i] - sd.capacity[i];
	  if (excess > 0) {
	       char buf[sizeof(long)*3 + 1];
	       sprintf(buf, "%ld", excess);
	       sgmlerr(162, (struct parse *)0,
		       (UNCH *)captab[i], (UNCH *)buf);
	  }
     }
}

VOID sgmlsubcap(v)
long *v;
{
     int i;
     for (i = 0; i < NCAPACITY; i++)
	  if (v[i] > maxsubcap[i])
	       maxsubcap[i] = v[i];
}

int sgmlsdoc(ptr)
UNIV ptr;
{
     struct entity *e;
     union etext etx;
     etx.x = ptr;

     e = entdef(indocent, ESF, &etx);
     if (!e)
	  return -1;
     return entopen(e);
}

/* SGMLGENT:  Get a data entity.
              Returns:
	      -1 if the entity does not exist
	      -2 if it is not a data entity
	      1 if it is an external entity
	      2 if it is an internal cdata entity
	      3 if it is an internal sdata entity
*/
int sgmlgent(iname, np, tp)
UNCH *iname;
PNE *np;
UNCH **tp;
{
     PECB ep;                 /* Pointer to an entity control block. */

     ep = entfind(iname);
     if (!ep)
	  return -1;
     switch (ep->estore) {
     case ESN:
	  if (np)
	       *np = ep->etx.n;
	  return 1;
     case ESC:
	  if (tp)
	       *tp = ep->etx.c;
	  return 2;
     case ESX:
	  if (tp)
	       *tp = ep->etx.c;
	  return 3;
     }
     return -2;
}

/* Mark an entity. */

int sgmlment(iname)
UNCH *iname;
{
     PECB ep;
     int rc;

     ep = entfind(iname);
     if (!ep)
	  return -1;
     rc = ep->mark;
     ep->mark = 1;
     return rc;
}

int sgmlgcnterr()
{
     return msgcnterr();
}

/* This is for error handling functions that want to print a gi backtrace. */

UNCH *getgi(i)
int i;
{
     return i >= 0 && i <= ts ? tags[i].tetd->etdgi + 1 : NULL;
}

/* Returns the value of prologsw for the use by error handling functions. */

int inprolog()
{
     return prologsw;
}

/* Used by the error handling functions to access scbs. */

int getlocation(level, locp)
int level;
struct location *locp;
{
     if (level < 0 || level > es)
	  return 0;
     if (locp) {
	  int es = level;
	  /* source macros access a variable called `es' */

	  locp->filesw = FILESW;
	  locp->rcnt = RCNT;
	  locp->ccnt = CCNT;
	  locp->ename = ENTITY + 1;
	  locp->fcb = SCBFCB;
	  locp->curchar = CC;
	  locp->nextchar = NEXTC;
     }
     return 1;
}

int sgmlloc(linenop, filenamep)
unsigned long *linenop;
char **filenamep;
{
     int level = es;
     int es;

     for (es = level; es >= 0 && !FILESW; es--)
	  ;
     if (es < 0)
	  return 0;
     *linenop = RCNT;
     *filenamep = ioflid(SCBFCB);
     return 1;
}

/*
Local Variables:
c-indent-level: 5
c-continued-statement-offset: 5
c-brace-offset: -5
c-argdecl-indent: 0
c-label-offset: -5
End:
*/
