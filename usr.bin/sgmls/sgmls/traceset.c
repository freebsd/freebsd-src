#include "sgmlincl.h"         /* #INCLUDE statements for SGML parser. */

#ifdef TRACE

#include "context.h"

/* Token status: RCHIT RCMISS RCEND RCREQ RCNREQ */
#define STATUX tags[ts].status

/* Trace variables.
*/
int  trace = 0;               /* Switch: 1=trace state transitions; 0=don't. */
int atrace = 0;               /* Switch: 1=trace attribute activity; 0=don't. */
int ctrace = 0;               /* Switch: 1=trace context checking; 0=don't. */
int dtrace = 0;               /* Switch: 1=trace declaration parsing; 0=don't.*/
int etrace = 0;               /* Switch: 1=trace entity activity; 0=don't.*/
int gtrace = 0;               /* Switch: 1=trace group creations; 0=don't. */
int itrace = 0;               /* Switch: 1=trace ID activity; 0=don't. */
int mtrace = 0;               /* Switch: 1=trace MS activity; 0=don't. */
int ntrace = 0;               /* Switch: 1=trace notation activity; 0=don't. */
char emd[] = "EMD";           /* For "EMD" parameter type in dtrace calls. */

/* Return a printable representation of c.
*/
static
char *printable(c)
int c;
{
     static char buf[5];
     if (c >= 040 && c < 0177) {
	  buf[0] = c;
	  buf[1] = '\0';
     }
     else
	  sprintf(buf, "\\%03o", (UNCH)c);
     return buf;
}

static
VOID dotrace(s)
char *s;
{
     trace = (s && strchr(s, 't') != 0);
     atrace = (s && strchr(s, 'a') != 0);
     ctrace = (s && strchr(s, 'c') != 0);
     dtrace = (s && strchr(s, 'd') != 0);
     etrace = (s && strchr(s, 'e') != 0);
     gtrace = (s && strchr(s, 'g') != 0);
     itrace = (s && strchr(s, 'i') != 0);
     mtrace = (s && strchr(s, 'm') != 0);
     ntrace = (s && strchr(s, 'n') != 0);
}
/* TRACESET: Set switches for tracing body of document.
*/
VOID traceset()
{
     dotrace(sw.trace);

     if (trace||atrace||ctrace||dtrace||etrace||gtrace||itrace||mtrace||ntrace)
          fprintf(stderr,
"TRACESET: state=%d;att=%d;con=%d;dcl=%d;ent=%d;grp=%d;id=%d;ms=%d;dcn=%d.\n",
		  trace, atrace, ctrace, dtrace, etrace, gtrace, itrace,
		  mtrace, ntrace);
}
/* TRACEPRO: Set switches for tracing prolog.
 */
VOID tracepro()
{
     dotrace(sw.ptrace);

     if (trace||atrace||dtrace||etrace||gtrace||mtrace||ntrace)
	  fprintf(stderr,
		  "TRACEPRO: state=%d; att=%d; dcl=%d; ent=%d; grp=%d; ms=%d; dcn=%d.\n",
		  trace, atrace, dtrace, etrace, gtrace, mtrace, ntrace);
}
/* TRACEPCB: Trace character just parsed and other pcb data.
 */
VOID tracepcb(pcb)
struct parse *pcb;
{
     fprintf(stderr, "%-8s %2u-%2u-%2u-%2u from %s [%3d] in %s, %d:%d.\n",
	     pcb->pname, pcb->state, pcb->input, pcb->action,
	     pcb->newstate, printable(*FPOS), *FPOS, ENTITY+1, RCNT,
	     RSCC+FPOS+1-FBUF);
}
/* TRACETKN: Trace character just read during token parse.
 */
VOID tracetkn(scope, lextoke)
int scope;
UNCH lextoke[];               /* Lexical table for token and name parses. */
{
     fprintf(stderr, "TOKEN    %2d-%2d       from %s [%3d] in %s, %d:%d.\n",
	     scope, lextoke[*FPOS],
	     printable(*FPOS), *FPOS, ENTITY+1, RCNT,
	     RSCC+FPOS+1-FBUF);
}
/* TRACEGML: Trace state of main SGML driver routine.
 */
VOID tracegml(scb, pss, conactsw, conact)
struct restate *scb;
int pss, conactsw, conact;
{
     fprintf(stderr,
	     "SGML%02d   %2d-%2d-%2d-%2d in main driver; conactsw=%d; conact=%d.\n",
	     pss, scb[pss].sstate, scb[pss].sinput, scb[pss].saction,
	     scb[pss].snext, conactsw, conact);
}
/* TRACEVAL: Trace parse of an attribute value that is a token list.
 */
VOID traceval(pcb, atype, aval, tokencnt)
struct parse *pcb;
UNS atype;                    /* Type of token list expected. */
UNCH *aval;                   /* Value string to be parsed as token list. */
int tokencnt;                 /* Number of tokens found in attribute value. */
{
     fprintf(stderr,
	     "%-8s %2d-%2d-%2d-%2d at %p, atype=%02x, tokencnt=%d: ",
	     pcb->pname, pcb->state, pcb->input, pcb->action,
	     pcb->newstate, (UNIV)aval, atype, tokencnt);
     fprintf(stderr, "%s\n", aval);
}
/* TRACESTK: Trace entry just placed on tag stack.
 */
VOID tracestk(pts, ts2, etictr)
struct tag *pts;              /* Stack entry for this tag. */
int ts2;                      /* Stack depth. */
int etictr;                   /* Number of "netok" tags on stack. */
{
     fprintf(stderr,
	     "STACK    %s begun; stack depth %d; tflag=%02x; etictr=%d",
	     pts->tetd->etdgi+1, ts2, pts->tflags, etictr);
     fprintf(stderr, " srm=%s.\n",
	     pts->tsrm!=SRMNULL ? (char *)(pts->tsrm[0]->ename+1) : "#EMPTY");
}
/* TRACEDSK: Trace entry just removed from tag stack.
 */
VOID tracedsk(pts, ptso, ts3, etictr)
struct tag *pts;              /* Stack entry for new open tag. */
struct tag *ptso;             /* Stack entry for tag just ended. */
int ts3;                      /* Stack depth. */
int etictr;                   /* Number of "netok" tags on stack. */
{
     fprintf(stderr,
	     "DESTACK  %s ended; otflag=%02x; %s resumed; depth=%d; tflag=%02x; etictr=%d",
	     ptso->tetd->etdgi+1, ptso->tflags,
	     pts->tetd->etdgi+1, ts3, pts->tflags, etictr);
     fprintf(stderr, " srm=%s.\n",
	     pts->tsrm!=SRMNULL ? (char *)(pts->tsrm[0]->ename+1) : "#EMPTY");
}
/* TRACECON: Trace interactions between content parse and stag/context
   processing.
   */
VOID tracecon(etagimct, dostag, datarc, pcb, conrefsw, didreq)
int etagimct;                 /* Implicitly ended elements left on stack. */
int dostag;                   /* 1=retry newetd instead of parsing; 0=parse. */
int datarc;                   /* Return code for data: DAF_ or REF_ or zero. */
struct parse *pcb;            /* Parse control block for this parse. */
int conrefsw;                 /* 1=content reference att specified; 0=no. */
int didreq;                   /* 1=required implied empty tag processed; 0=no.*/
{
     fprintf(stderr,
	     "CONTENT  etagimct=%d dostag=%d datarc=%d pname=%s action=%d \
conrefsw=%d didreq=%d\n",
	     etagimct, dostag, datarc, pcb->pname, pcb->action,
	     conrefsw, didreq);
}
/* TRACESTG: Trace start-tag context validation input and results.
 */
VOID tracestg(curetd, dataret, rc, nextetd, mexts)
struct etd *curetd;           /* The etd for this tag. */
int dataret;                  /* Data pending: DAF_ REF_ 0=not #PCDATA. */
int rc;                       /* Return code from context or other test. */
struct etd *nextetd;          /* The etd for a forced start-tag (if rc==2). */
int mexts;                    /* >0=stack level of minus grp; -1=plus; 0=none.*/
{
     fprintf(stderr,
	     "STARTTAG newetd=%p; dataret=%d; rc=%d; nextetd=%p; mexts=%d.\n",
	     (UNIV)curetd, dataret, rc, (UNIV)nextetd, mexts);
}
/* TRACEETG: Trace end-tag matching test on stack.
 */
VOID traceetg(pts, curetd, tsl, etagimct)
struct tag *pts;              /* Stack entry for this tag. */
struct etd *curetd;           /* The etd for this tag. */
int tsl;                      /* Temporary stack level for looping. */
int etagimct;                 /* Num of implicitly ended tags left on stack. */
{
     fprintf(stderr,
	     "ENDTAG   tsl=%d; newetd=%p; stacketd=%p; tflags=%02x; etagimct=%d.\n",
	     tsl, (UNIV)curetd, (UNIV)pts->tetd, pts->tflags, etagimct);
}
/* TRACEECB: Trace entity control block activity.
 */
VOID traceecb(action, p)
char *action;
struct entity *p;
{
     static char estype1[] = " TMMMSEIXCNFPDLK";
     static char estype2[] = "  DS            ";
     if (!p)
	  return;
     fprintf(stderr,
	     "%-8s (es=%d) type %c%c entity %s at %p containing ",
	     action, es, estype1[p->estore], estype2[p->estore], p->ename+1,
	     (UNIV)p);
     if (p->estore==ESN && strcmp(action, "ENTDEF"))
	  traceesn(p->etx.n);
     else if (p->etx.x==0)
	  fprintf(stderr, "[NOTHING]");
     else
	  fprintf(stderr, "%s",
		  p->etx.c[0] ? (char *)p->etx.c : "[EMPTY]");
     putc('\n', stderr);
}
/* TRACEDCN: Trace data content notation activity.
 */
VOID tracedcn(p)
struct dcncb *p;
{
     fprintf(stderr,
	    "DCN      dcn=%p; adl=%p; notation is %s\n",
	     (UNIV)p, (UNIV)p->adl, p->ename+1);
     if (p->adl)
	  traceadl(p->adl);
}
/* TRACEESN: Print a data entity control block.
 */
VOID traceesn(p)
PNE p;
{
     fprintf(stderr, "ESN      Entity name is %s; entity type is %s.\n",
            (NEENAME(p)!=0) ? ((char *)NEENAME(p))+1 : "[UNDEFINED]",
	    /* NEXTYPE(p)); */
            (NEXTYPE(p)==1 ? "CDATA" : (NEXTYPE(p)==2 ? "NDATA" : "SDATA")));
     fprintf(stderr, "         System ID is %s\n",
            (NEID(p)!=0) ? (char *)NEID(p) : "[UNDEFINED]");
     if (p->nedcn!=0)
	  tracedcn(p->nedcn);
}
/* TRACESRM: Print the members of a short reference map.
 */
VOID tracesrm(action, pg, gi)
char *action;
TECB pg;
UNCH *gi;
{
     int i = 0;               /* Loop counter. */

     if (pg==SRMNULL)
	  fprintf(stderr, "%-8s SHORTREF table empty for %s.\n", action, gi);
     else {
          fprintf(stderr, "%-8s %s at %p mapped for %s.\n",
		  action, pg[0]->ename+1, (UNIV)pg,
		  gi ? (char *)gi : "definition");
          while (++i<=lex.s.dtb[0].mapdata)
	       if (pg[i])
		    fprintf(stderr, "%14s%02u %p %s\n",
			    "SR", i, (UNIV)pg[i], pg[i]->ename+1);
     }
}
/* TRACEADL: Print an attribute definition list.
 */
VOID traceadl(al)
struct ad al[];
{
     int i=0;

     fprintf(stderr, "ADLIST   %p %d membe%s; %d attribut%s\n",
	     (UNIV)al, ADN(al), ADN(al)==1 ? "r" : "rs", AN(al),
	     AN(al)==1 ? "e" : "es");
     while (++i<=ADN(al)) {
          fprintf(stderr,
		  (BITOFF(ADFLAGS(al,i), AGROUP) && ADTYPE(al,i)<=ANOTEGRP)
		  ? "          %p %-8s %02x %02x %2d %2d %p %p\n"
		  : "    %p %-8s %02x %02x %2d %2d %p %p\n",
		  &al[i], ADNAME(al,i), ADFLAGS(al,i), ADTYPE(al,i), ADNUM(al,i),
		  ADLEN(al,i), ADVAL(al,i), ADDATA(al,i).x);
          if (ADVAL(al,i)) {
               fprintf(stderr, "%s", ADVAL(al,i));
               if (ADTYPE(al,i)==AENTITY && ADDATA(al,i).n!=0) {
                    fprintf(stderr, "=>");
                    traceesn(ADDATA(al,i).n);
               }
               else if (ADTYPE(al,i)==ANOTEGRP)
                    fprintf(stderr, "=>%s",
			    (ADDATA(al,i).x->dcnid!=0)
			    ? (char *)ADDATA(al,i).x->dcnid
			    : "[UNDEFINED]");
          }
          else
	       fprintf(stderr, "[%s]",
		       GET(ADFLAGS(al,i), AREQ)
		       ? "REQUIRED"
		       : (GET(ADFLAGS(al,i), ACURRENT) ? "CURRENT"  : "NULL"));
     }
     fprintf(stderr, "\n");
}
/* TRACEMOD: Print the members of a model.
 */
VOID tracemod(pg)
struct thdr pg[];
{
     fprintf(stderr, "MODEL    %p %02x %d\n",
	     (UNIV)&pg[0], pg[0].ttype, pg[0].tu.tnum);
     if ((pg[0].ttype & MKEYWORD) == 0) {
	  int i;

	  for (i = 1; i < pg[0].tu.tnum + 2; i++) {
	       if (GET(pg[i].ttype, TTMASK) == TTETD)
		    fprintf(stderr, "                      %p %02x %s\n",
			    (UNIV)&pg[i], pg[i].ttype, pg[i].tu.thetd->etdgi+1);
	       else if (GET(pg[i].ttype, TTMASK) == TTCHARS)
		    fprintf(stderr, "                      %p %02x %s\n",
			    (UNIV)&pg[i], pg[i].ttype, "#PCDATA");
	       else
		    fprintf(stderr, "         %p %02x %d\n",
			    (UNIV)&pg[i], pg[i].ttype, pg[i].tu.tnum);
	  }
     }
     fprintf(stderr, "\n");
}
/* TRACEGRP: Print the members of a name (i.e., etd) group.
 */
VOID tracegrp(pg)
struct etd *pg[];
{
     int i = -1;              /* Loop counter. */

     fprintf(stderr, "ETDGRP   %p\n", (UNIV)pg);
     while (pg[++i]!=0)
	  fprintf(stderr, "         %p %s\n", (UNIV)pg[i], pg[i]->etdgi+1);
}
/* TRACENGR: Print the members of a notation (i.e., dcncb) group.
 */
VOID tracengr(pg)
struct dcncb *pg[];
{
     int i = -1;              /* Loop counter. */

     fprintf(stderr, "DCNGRP   %p\n", (UNIV)pg);
     while (pg[++i]!=0)
	  fprintf(stderr, "         %p %s\n", (UNIV)pg[i], pg[i]->ename+1);
}
/* TRACEETD: Print an element type definition.
 */
VOID traceetd(p)
struct etd *p;                /* Pointer to an etd. */
{
     fprintf(stderr,
"ETD      etd=%p %s min=%02x cmod=%p ttype=%02x mex=%p, pex=%p, ",
	     (UNIV)p, p->etdgi+1, p->etdmin, (UNIV)p->etdmod,
	     p->etdmod->ttype, (UNIV)p->etdmex, (UNIV)p->etdpex);
     fprintf(stderr, "adl=%p, srm=%s.\n",
	     (UNIV)p->adl,
	     (p->etdsrm==SRMNULL)
	     ? "#EMPTY"
	     : (p->etdsrm) ? (char *)(p->etdsrm[0]->ename+1) : "#CURRENT");
}
/* TRACEID: Print an ID control block.
 */
VOID traceid(action, p)
char *action;
struct id *p;                 /* Pointer to an ID. */
{
     fprintf(stderr, "%-8s %s at %p is %s; ", action, p->idname+1, (UNIV)p,
	     p->iddefed ? "defined" : "undefined");
     fprintf(stderr, "last ref=%p\n", (UNIV)p->idrl);
}
/* TRACEMD: Trace a markup declaration parameter.
 */
VOID tracemd(parmid)
char *parmid;                 /* Parameter identifier. */
{
     fprintf(stderr, "MDPARM   %-8s for %-8s, token %02d, type %02u, %s.\n",
	     mdname, subdcl ? (char *)subdcl : "[NONE]", parmno, pcbmd.action, parmid);
}
/* TRACEMS: Trace marked section activity.
 */
VOID tracems(action, code, mslevel, msplevel)
int action;                   /* 1=began new level; 0=resumed previous. */
int code;
int mslevel;                  /* Nesting level of marked sections. */
int msplevel;                 /* Nested MS levels subject to special parse. */
{
     fprintf(stderr,
	     "MS%c      %2d                 %s nesting level %d (msp %d).\n",
	     (action ? ' ' : 'E'), code, (action ? "began" : "resumed"),
	     mslevel, msplevel);
}

static
VOID tracehits(h)
unsigned long *h;
{
     int i;
     fprintf(stderr, " H=");
     for (i = grplongs - 1; i >= 0; --i)
	  fprintf(stderr, "%0*lx", LONGBITS/4, h[i]);
}

/* TRACEGI: Trace GI testing stages in CONTEXT.C processing.
 */
VOID tracegi(stagenm, gi, mod, pos, Tstart)
char *stagenm;
struct etd *gi;               /* ETD of new GI. */
struct thdr mod[];            /* Model of current open element. */
struct mpos pos[];            /* Position in open element's model. */
int Tstart;                   /* Initial T for this group. */
{
     int i = 0;               /* Loop counter. */

     fprintf(stderr, "%-10s %d:", stagenm, P);
     while (++i<=P)
	  fprintf(stderr, " %d-%d", pos[i].g, pos[i].t);
     fprintf(stderr, " (%u) gocc=%02x gtype=%02x gnum=%d",
	     M, GOCC, GTYPE, GNUM);
     tracehits(H);
     fprintf(stderr, " status=%d Tstart=%d\n", STATUX, Tstart);
     fprintf(stderr,
	     "=>%-8s tocc=%02x ttype=%02x thetd=%p (%s) gietd=%p (%s)\n",
	     tags[ts].tetd->etdgi+1, TOCC, TTYPE, (UNIV)TOKEN.tu.thetd,
	     (TTYPE
	      ? (TTYPE==TTETD ? (char *)(TOKEN.tu.thetd->etdgi+1) : "#GROUP")
	      : "#PCDATA"),
	     (UNIV)gi,
	     (gi==ETDCDATA ?  "#PCDATA" : (char *)(gi->etdgi+1)));
}
/* TRACEEND: Trace testing for end of group in CONTEXT.C processing.
 */
VOID traceend(stagenm, mod, pos, rc, opt, Tstart)
char *stagenm;
struct thdr mod[];            /* Model of current open element. */
struct mpos pos[];            /* Position in open element's model. */
int rc;                       /* Return code: RCNREQ RCHIT RCMISS RCEND */
int opt;                      /* ALLHIT parm: 1=test optionals; 0=ignore. */
int Tstart;                   /* Initial T for this group. */
{
     int i = 0;               /* Loop counter. */

     fprintf(stderr, "%-10s %d:", stagenm, P);
     while (++i<=P)
	  fprintf(stderr, " %d-%d", pos[i].g, pos[i].t);
     fprintf(stderr, " (%u) gocc=%02x gtype=%02x gnum=%d",
	     M, GOCC, GTYPE, GNUM);
     tracehits(H);
     fprintf(stderr, " status=%d Tstart=%d\n", STATUX, Tstart);
     fprintf(stderr, "=>%-8s tocc=%02x ttype=%02x thetd=%p (%s)",
	     tags[ts].tetd->etdgi+1, TOCC, TTYPE, (UNIV)TOKEN.tu.thetd,
	     (TTYPE
	      ? (TTYPE==TTETD ? (char *)(TOKEN.tu.thetd->etdgi+1) : "#GROUP")
	      : "#PCDATA"));
     fprintf(stderr, " rc=%d offbitT=%d allhit=%d\n",
	     rc, offbit(H, (int)T, GNUM), allhit(&GHDR, H, 0, opt));
}

#endif /* TRACE */
/*
Local Variables:
c-indent-level: 5
c-continued-statement-offset: 5
c-brace-offset: -5
c-argdecl-indent: 0
c-label-offset: -5
End:
*/
