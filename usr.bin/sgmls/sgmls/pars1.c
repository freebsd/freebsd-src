#include "sgmlincl.h"         /* #INCLUDE statements for SGML parser. */
#define GI (tags[ts].tetd->etdgi+1)              /* GI of current element. */
#define NEWGI (newetd->etdgi+1)                  /* GI of new tag. */

static VOID doincludes P((void));
static int pentname P((char *));
static struct mpos *newmpos P((void));
static VOID commbufs P((void));
static VOID checkdtd P((void));

/* PARSECON: Parse content of an element.
*/
int parsecon(tbuf, pcb)
UNCH *tbuf;                   /* Work area for tokenization. */
struct parse *pcb;            /* Parse control block for this parse. */
{
     int srn;                 /* SHORTREF delimiter number (1-32). */
     int refrc;               /* Return code from sentref, stagetd, etc. */

     TRACECON(etagimct, dostag, datarc, pcb, conrefsw, didreq);
     if (eodsw) return(EOD_);
     if (didreq && (conrefsw & TAGREF)) {didreq = 0; goto conr;}
     if (etagimct>0) {etagimsw = --etagimct ? 1 : 0; destack(); return(ETG_);}
     if (dostag) {
          conrefsw = conrefsv;
          etisw = etiswsv;
          if (charmode) {dostag = 0; return datarc;}
          return stag(datarc);
     }
     if (conrefsw) {
     conr:
	  destack();
	  conrefsw = 0;
	  return ETG_;
     }
     else if (eofsw) return(EOD_);

     datarc = 0;
     while (1) {
          parse(pcb);
          srn = (int)pcb->action - SRMIN;  /* Just in case it's a SHORTREF. */
          switch (pcb->action) {
          case DCE_:          /* Data character in element content. */
   	       /* The data character might be a non-SGML character so
                  reprocess it using pcbconm. */
	       REPEATCC;
	       pcb = conpcb = &pcbconm;
	       pcb->newstate = pcbcnet;
	       continue;
          case DAS_:          /* Current character begins data. */
               data = FPOS;
               continue;

          case NLF_:          /* NET or SR returns data in lookahead buffer. */
               datalen = (UNS)(ptcon - data); REPEATCC;
               goto rcc;

          case LAF_:          /* Return data in lookahead buffer: mixed. */
               datalen = (UNS)(ptcon+1 - data);
               goto rcc;

          case NON_:          /* Single nonchar in nonchbuf. */
               datalen = 2; data = nonchbuf;
               goto nrcc;

          case DAR_:          /* Return data except for last char. */
               REPEATCC;
          case DAF_:          /* Return data in source entity buffer. */
               datalen = (UNS)(FPOS - data);
          rcc:
	       REPEATCC;
          case DEF_:          /* Return data in data entity. */
          nrcc:
	       datarc = DAF_;
               if (pcb==&pcbcone) {
                    pcbconm.newstate = pcbcnet;
                    conpcb = &pcbconm;
               }
               if (charmode) return(datarc);
               stagmin = MINNONE; stagreal = newetd = ETDCDATA;
               return(stag(datarc));

          case LAS_:          /* Start lookahead buffer with current char. */
               *(ptcon = data = tbuf+1) = *FPOS;
               continue;

          case LAM_:          /* Move character to lookahead buffer. */
               *++ptcon = *FPOS;
               continue;

          case STG_:          /* Process non-null start-tag. */
               CTRSET(tagctr);          /* Start counting tag length. */
	       tages = es;
               parsenm(tbuf, NAMECASE); /* Get the GI. */
               newetd = etdref(tbuf);
               if (newetd && newetd->adl) {
                    parseatt(newetd->adl, tbuf);
                    adlval((int)ADN(al), newetd);
               }
               parsetag(&pcbstag);      /* Parse the tag ending. */
               if ((CTRGET(tagctr)-tagdelsw)>=TAGLEN)
                    sgmlerr(66, &pcbstag, (UNCH *)0, (UNCH *)0);
               if (!newetd) {
                    sgmlerr(132, pcb, tbuf+1, (UNCH *)0);
                    continue;
               }
               return(stagetd(&pcbstag));

          case NST_:          /* Process null start-tag. */
               return nstetd();

          case ETC_:          /* End-tag in CDATA or RCDATA. */
          case ETG_:          /* Process non-null end-tag. */
               newetd = etdref(parsenm(tbuf, NAMECASE));  /* Get the GI. */
               parsetag(&pcbetag);                        /* Parse tag end. */
               if (!newetd)                               /* Error: undefined.*/
                    sgmlerr(11, &pcbetag, tbuf+1, (UNCH *)0);
               else if (etagetd(&pcbetag)>=0) return ETG_;/* Open element. */
               if (pcb->action!=ETC_) continue;
               /* Tag is undefined or not for an open element and we are in
                  a CDATA or RCDATA element; issue message and treat as
                  null end-tag (</>).
               */
               sgmlerr(57, &pcbetag, (UNCH *)0, (UNCH *)0);
          case NET_:          /* Process null end-tag. */
               if ((refrc = netetd(conpcb))!=0) return ETG_;
               continue;

          case NED_:          /* Process null end-tag delimiter. */
               etagmin = MINNET;
               newetd = etagreal = ETDNET;
               etagimct = etag();
               etagimsw = etagimct ? 1 : 0; destack();
               return ETG_;
	  case GTR_:
               if (entget()!=-1) {
		    data = FPOS;
		    continue;
	       }
	       /* fall through */
          case EOD_:          /* End of primary file. */
               if (ts<1) return(EOD_);  /* Normal end: stack is empty. */
               etagimct = ts-1;     /* Treat as end-tag for top tag on stack. */
               etagmin = MINETAG; etagreal = tags[0].tetd;
               destack();
               eofsw = 1;          /* Return EOD_ after destacking all. */
               return ETG_;

          /* Short references ending with blanks:
               If the blank sequence is followed by RE, go do SR7 or SR6.
               If the entity is undefined and we are in mixed content,
               the blanks must be returned as data.  If not, they
               can be ignored.
          */
          case SR9_:          /* Process SR9 (two or more blanks). */
               REPEATCC;      /* Make first blank the CC. */
          case SR4_:          /* Process SR4 (RS, blanks). */
               parseseq(tbuf, BSEQLEN); /* Squeeze out all blanks. */
               if (*FPOS=='\r') {srn = (srn==9) ? 7 : 6; data = tbuf; goto sr6;}
               else REPEATCC;
               if ((refrc = shortref(srn, pcb))==DEF_) goto nrcc;
               if (refrc>0) return refrc;
               if (refrc==ENTUNDEF && pcb==&pcbconm)
                    {data = tbuf; goto nrcc;}
               continue;

          /* Short references ending with RE:
               If the reference is defined, the RE is ignored.
               For RE and RS RE,
               no special action is needed if the reference is undefined,
               as the RE will be processed immediately as the current character.
               For B RE and RS B RE,
               the input is primed with a special character that will
               be treated as an RE that cannot be a short reference.
          */
          case SR7_:          /* Process SR7 (blanks, RE). */
               datalen = (UNS)(FPOS - data);
          case SR2_:          /* Process SR2 (RE). */
          case SR5_:          /* Process SR5 (RS, RE). */
          sr6:                /* Process SR6 (RS, blanks, RE). */
               if ((refrc = shortref(srn, pcb))!=ENTUNDEF) {
                    if (refrc==DEF_) goto nrcc;   /* Defined: data entity. */
                    if (refrc>0) return refrc;    /* Defined: tag entity. */
                    continue;                     /* Defined: not tag. */
               }
               if (pcb!=&pcbconm) continue;       /* Not mixed; ignore chars. */
               if (srn>=6)                        /* Return blanks as data. */
                    {*FPOS = lex.d.genre; REPEATCC; goto nrcc;}
          case REF_:          /* Undefined SR with RE; return record end. */
               datarc = REF_;
               if (charmode) return(datarc);
#if 0
	       /* The standard says this situation can force a tag.
		  See 323:3-6, 412:1-7. */
               /* If RE would be ignored, don't treat it as start-tag
                  because it could force a required tag; but do change
                  state to show that an RE was ignored.
               */
               if (scbsgml[pss].snext==scbsgmst) {
                    scbsgml[pss].snext = scbsgmnr;
		    TRACEGML(scbsgml, pss, conactsw, conact);
                    continue;
               }
#endif
               stagmin = MINNONE; stagreal = newetd = ETDCDATA;
               return(stag(datarc));

          case SR3_:          /* Process SR3 (RS). */
               REPEATCC;
               if ((refrc = shortref(srn, pcb))==DEF_) goto nrcc;
               if (refrc>0) return refrc;
               continue;

          case RBR_:	      /* Two right brackets  */
	       srn = 26;
	       REPEATCC;
	       /* fall through */
          case SR1_:          /* Process SR1 (TAB). */
          case SR8_:          /* Process SR8 (space). */
          case SR19:          /* Process SR19 (-). */
	  case SR26:	      /* Process SR26 (]). */
               REPEATCC;
               goto srproc;

          case FCE_:          /* Process free character (SR11-18, SR21-32). */
               fce[0] = *FPOS;
               srn = mapsrch(&lex.s.dtb[lex.s.fce], fce);
          case SR10:          /* Process SR10 ("). */
          case SR11:          /* Process SR11 (#). */
          case SR20:          /* Process SR20 (-). */
	  case SR25:	      /* Process SR25 ([). */
          srproc:
               if ((refrc = shortref(srn, pcb))==DEF_) goto nrcc;
               if (refrc>0) return refrc;
               if (refrc==ENTUNDEF) {             /* Treat the SR as data. */
                    data = FPOS - (srn==lex.s.hyp2);/* Two data chars if SR20.*/
                    if (pcb!=&pcbconm) {         /* If not in mixed content: */
                         if (srn>=lex.s.data) {  /* Change PCB. */
			      pcb = conpcb = &pcbconm;
			      pcb->newstate = pcbcnda;
			 }
                    }
                    else pcb->newstate = pcbcnda;/* Now in data found state. */
               }
               continue;

          case ERX_:          /* Entity ref in RCDATA: cancel ending delims.*/
               lexcon[lex.d.tago] = lex.l.fre;
               lexcon[lex.d.net] = lex.l.nonet;
               lexlms[lex.d.msc] = lex.l.fre;
               continue;

          case EE_:           /* Entity end in RCDATA: check nesting. */
               if (es<rcessv) {synerr(37, pcb); rcessv = es;}
               /* If back at top level, re-enable the ending delimiters. */
               if (es==rcessv) {
                    lexcon[lex.d.tago] = lex.l.tago;
                    lexcon[lex.d.net] = etictr ? lex.l.net : lex.l.nonet;
                    lexlms[lex.d.msc] = lex.l.msc;
               }
               continue;

          case PIE_:          /* PI entity: same as PIS_. */
               return PIS_;

          case RSR_:               /* Record start: ccnt=0; ++rcnt.*/
               ++RCNT; CTRSET(RSCC);
	       return RSR_;
	  case MSS_:
	       if (ts == 0) synerr(217, pcb);
	       return MSS_;
          default:
               return (int)pcb->action; /* Default (MD_ MDC_ MSS_ MSE_ PIS_). */
          }
     }
}
/* STAGETD: Process start-tag etd.
*/
int stagetd(pcb)
struct parse *pcb;            /* Parse control block for this parse. */
{
     if (!newetd->etdmod) {
          sgmlerr(43, pcb, newetd->etdgi+1, (UNCH *)0);
          ++ds.etdercnt;
          etdset(newetd, (UNCH)SMO+EMO+ETDOCC, &undechdr,
                (PETD *)0, (PETD *)0, (PECB *)0);
          TRACEETD(newetd);
     }
     stagmin = MINNONE; stagreal = newetd;
     return stag(0);
}
/* NSTETD: Process null start-tag etd.
*/
int nstetd()
{
     if (sd.omittag && ts > 0)
	  newetd = tags[ts].tetd;
     else if (!sd.omittag && lastetd != 0)
	  newetd = lastetd;
     else
	  newetd = tags[0].tetd->etdmod[2].tu.thetd;
     stagmin = MINNULL; stagreal = ETDNULL;
     etisw = 0;
     return stag(0);
}
/* ETAGETD: Process end-tag etd.
*/
int etagetd(pcb)
struct parse *pcb;            /* Parse control block for this parse. */
{
     etagmin = MINNONE; etagreal = newetd;
     if ((etagimct = etag())<0) {
          sgmlerr(E_ETAG, pcb, NEWGI, tags[ts].tetd->etdgi+1);
          return etagimct;
     }
     etagimsw = etagimct ? 1 : 0; destack();
     return ETG_;
}
/* NETETD: Process null end-tag etd.
*/
int netetd(pcb)
struct parse *pcb;            /* Parse control block for this parse. */
{
     if (ts<1) {
          sgmlerr(51, pcb, (UNCH *)0, (UNCH *)0);
          return 0;
     }
     etagmin = MINNULL; etagreal = ETDNULL;
     etagimsw = 0; destack();
     return ETG_;
}
/* SHORTREF: Process a short (alternative) reference to an entity.
             Returns ENTUNDEF if entity is not defined, otherwise returns
             the return code from stagetd or etagetd if the entity was
             a tag, or zero if an error occurred somewhere.
*/
int shortref(srn, pcb)
int srn;                      /* Short reference number. */
struct parse *pcb;            /* Parse control block for this parse. */
{
     int rc;                  /* Return code from entopen. */

     if (tags[ts].tsrm==SRMNULL || !tags[ts].tsrm[srn]) return ENTUNDEF;
     rc = entopen(tags[ts].tsrm[srn]);
     if (rc==ENTDATA) return DEF_;
     if (rc==ENTPI) return PIS_;
     return(0);
}
/* PARSEPRO: Parse prolog.
             Note: ptpro cannot overrun tbuf (and therefore needn't be
             tested), as long as the buffer exceeds the longest
             lookahead sequence in the content parse tables.
*/
int parsepro()
{
     struct parse *oldpcb;

     while (1) {
          int rc;                  /* Return code: DAF MSS DCE */
          switch (parse(propcb)) {

          case LAS_:          /* Start lookahead buffer with current char. */
               *(ptpro = data = tbuf+1) = *FPOS;
               continue;
          case LAM_:          /* Move character to lookahead buffer. */
               *++ptpro = *FPOS;
               continue;
          case LAF_:          /* Return data in lookahead buffer. */
               datalen = (UNS)(ptpro+1 - data);
               REPEATCC;
               rc = DAF_;
               break;         /* Prolog ended; data pending. */

          case DTD_:          /* Process document type declaration. */
               parsenm(tbuf, NAMECASE); /* Get declaration name. */
               if (!ustrcmp(tbuf+1, sgmlkey)
		   && !dtdsw && !sgmlsw++) {
#if 0
		    parse(&pcbmdi);
#endif
		    /* If we got some appinfo, return. */
		    if (sgmldecl())
			 return APP_;
		    continue;
	       }
               if (!ustrcmp(tbuf+1, key[KDOCTYPE]) && !dtdsw++) {
		    startdtd();
		    mddtds(tbuf);
		    continue;
	       }
               sgmlerr(E_MDNAME, propcb, tbuf+1, (UNCH *)0);
               continue;
          case DTE_:          /* DOCTYPE declaration (and prolog) ended. */
               REPEATCC;      /* Put back char that followed MSC. */
	       if (es != 0)
		    sgmlerr(143, propcb, (UNCH *)0, (UNCH *)0);
               else if (dtdrefsw) {/* Process referenced DTD before real DTE. */
                    dtdrefsw = 0; /* Keep us from coming through here again. */
                    REPEATCC; /* Put back MSC so it follows referenced DTD. */
                    entref(indtdent);
               }
               else {
		    if (mslevel > 0) {
			 sgmlerr(230, propcb, (UNCH *)0, (UNCH *)0);
			 mslevel = 0;
			 msplevel = 0;
		    }
		    mddtde(tbuf);
	       }
               continue;

          case MD_:
	       /* Process markup declaration within DTD or LPD. */
               parsenm(tbuf, NAMECASE); /* Get declaration name. */
               if (!ustrcmp(tbuf+1, key[KENTITY]))
		    mdentity(tbuf);
               else if (!ustrcmp(tbuf+1, key[KUSEMAP]))
		    mdsrmuse(tbuf);
               else if (!ustrcmp(tbuf+1, key[KATTLIST]))
		    mdadl(tbuf);
               else if (!ustrcmp(tbuf+1, key[KSHORTREF]))
		    mdsrmdef(tbuf);
               else if (!ustrcmp(tbuf+1, key[KELEMENT]))
		    mdelem(tbuf);
               else if (!ustrcmp(tbuf+1, key[KNOTATION]))
		    mdnot(tbuf);
               else
		    sgmlerr(E_MDNAME, propcb, tbuf+1, (UNCH *)0);
               continue;
          case MDC_:          /* Process markup declaration comment. */
	       sgmlsw++;      /* SGML declaration not allowed after comment */
               parsemd(tbuf, NAMECASE, (struct parse *)0, NAMELEN);
               continue;

          case MSS_:	      /* Process marked section start. */
	       oldpcb = propcb;
               propcb = mdms(tbuf, propcb);
               if (propcb==&pcbmsc || propcb==&pcbmsrc) {
		    if (oldpcb == &pcbmds)
			 sgmlerr(135, oldpcb, (UNCH *)0, (UNCH *)0);
		    conpcb = propcb;
		    rc = DCE_;
		    break;
	       }
               continue;
          case MSE_:	      /* Process marked section end. */
               if (mdmse()) propcb = &pcbmds;
               continue;
	  case MSP_:	      /* Marked section start in prolog outside DTD */
	       rc = MSS_;
	       break;
          case PIE_:          /* PI entity: same as PIS_. */
               return(PIS_);

          case EOD_:          /* Return end of primary entity. */
	       if (dtdsw && propcb == &pcbpro) {
		    /* We've had a DTD, so check it. */
		    setdtype();
		    checkdtd();
	       }
	       if (!sw.onlypro || propcb != &pcbpro || !dtdsw)
		    sgmlerr(127, propcb, (UNCH *)0, (UNCH *)0);
               return propcb->action;
          case PIS_:          /* Return processing instruction (string). */
	       sgmlsw++;      /* SGML declaration not allowed after PI */
               return((int)propcb->action);  /* Prolog will continue later. */

          case CIR_:          /* Chars ignored; trying to resume parse. */
               synerr(E_RESTART, propcb);
               REPEATCC;
               continue;
	  case ETE_:	      /* End tag ended prolog */
	       REPEATCC;
	       /* fall through */
	  case STE_:	      /* Start tag ended prolog */
	       REPEATCC;
	       REPEATCC;
	       rc = STE_;
	       break;
          case PEP_:          /* Previous character ended prolog. */
               REPEATCC;
          case DCE_:          /* Data character ended prolog. */
               REPEATCC;
               rc = DCE_;
               break;
	  case EE_:	      /* Illegal entity end in ignored marked section. */
	       /* An error message has already been given. */
	       continue;
	  default:
	       abort();
          } /* switch */
          setdtype();		   /* First pass only: set document type. */
	  checkdtd();
	  if (sw.onlypro)
	       return EOD_;
          TRACESET();              /* Set trace switches. */
	  endprolog();
          /* *DOC is first element; stack it at level 0. */
          stack(newetd = nextetd = stagreal = etagreal = docetd);
          return(rc);
     } /* while */
}

/* Allocate buffers that are used in the DTD. */

VOID startdtd()
{
     nmgrp = (struct etd **)rmalloc((GRPCNT+1)*sizeof(struct etd *));
     nnmgrp = (PDCB *)rmalloc((GRPCNT+1)*sizeof(PDCB));
     gbuf = (struct thdr *)rmalloc((GRPGTCNT+3)*sizeof(struct thdr));
     /* The extra 1 is for parsing the name of a parameter entity in
	mdentity(). */
     nmbuf = (UNCH *)rmalloc(NAMELEN+3);
     pubibuf = (UNCH *)rmalloc(LITLEN+1);
     sysibuf = (UNCH *)rmalloc(LITLEN+1);
     commbufs();
     doincludes();
}

static
VOID checkdtd()
{
     struct dcncb *np;
     struct srh *sp;

     if (sw.swundef) {
	  int i;
	  struct etd *ep;

	  for (i = 0; i < ETDHASH; i++)
	       for (ep = etdtab[i]; ep; ep = ep->etdnext)
		    if (!ep->etdmod)
			 sgmlerr(140, (struct parse *)0, ep->etdgi + 1,
				 (UNCH *)0);
     }
     for (sp = srhtab[0]; sp; sp = sp->enext)
	  if (sp->srhsrm[0] == 0)
	       sgmlerr(152, (struct parse *)0, sp->ename + 1, (UNCH *)0);
	  else {
	       int i;
	       for (i = 1; i < lex.s.dtb[0].mapdata + 1; i++) {
		    struct entity *ecb = sp->srhsrm[i];
		    if (ecb && !ecb->estore) {
			 sgmlerr(93, (struct parse *)0,
				 ecb->ename + 1,
				 sp->srhsrm[0]->ename + 1);
			 sp->srhsrm[i] = 0;
		    }
	       }
	  }
     for (np = dcntab[0]; np; np = np->enext)
	  if (!np->defined)
	       sgmlerr(192, (struct parse *)0, np->ename + 1, (UNCH *)0);
}

/* Return non-zero if s is a valid parameter entity name.
If so put a transformed name in entbuf. */

static
int pentname(s)
char *s;
{
     int i;
     if (lextoke[(UNCH)*s] != NMS)
	  return 0;
     entbuf[2] = ENTCASE ? lextran[(UNCH)*s] : (UNCH)*s;
     for (i = 1; s[i]; i++) {
	  if (i > NAMELEN - 1)
	       return 0;
	  if (lextoke[(UNCH)s[i]] < NMC || s[i] == EOBCHAR)
	       return 0;
	  entbuf[i + 2] = ENTCASE ? lextran[(UNCH)s[i]] : (UNCH)s[i];
     }
     entbuf[1] = lex.d.pero;
     entbuf[i + 2] = '\0';
     entbuf[0] = (UNCH)(i + 3);	/* length byte, PERO and '\0' */
     return 1;
}

/* Handle sw.includes. */

static
VOID doincludes()
{
     char **p;
     if (!sw.includes)
	  return;
     for (p = sw.includes; *p; p++) {
	  if (pentname(*p)) {
	       if (!entfind(entbuf)) {
		    union etext etx;
		    etx.c = savestr(key[KINCLUDE]);
		    entdef(entbuf, ESM, &etx);
		    ++ds.ecbcnt;
		    ds.ecbtext += ustrlen(key[KINCLUDE]);
	       }
	  }
	  else
	       sgmlerr(138, (struct parse *)0, (UNCH *)*p, (UNCH *)0);
     }
}

/* Allocate buffers that are use both in the DTD and the instance. */

static
VOID commbufs()
{
     al = (struct ad *)rmalloc((ATTCNT+2)*sizeof(struct ad));
     lbuf = (UNCH *)rmalloc(LITLEN + 1);
}

static
struct mpos *newmpos()
{
     int j;
     unsigned long *h;
     struct mpos *p = (struct mpos *)rmalloc((GRPLVL+2)*sizeof(struct mpos));

     assert(grplongs > 0);
     h = (unsigned long *)rmalloc((GRPLVL+2)*grplongs*sizeof(unsigned long));
     for (j = 0; j < GRPLVL+2; j++) {
	  p[j].h = h;
	  h += grplongs;
     }
     return p;
}

/* Perform end of prolog buffer allocation. */

VOID endprolog()
{
     int i;
     
     ambigfree();
     if (dtdsw) {
	  frem((UNIV)nmgrp);
	  frem((UNIV)nnmgrp);
	  frem((UNIV)gbuf);
	  frem((UNIV)nmbuf);
	  frem((UNIV)sysibuf);
	  frem((UNIV)pubibuf);
     }
     else {
	  commbufs();
	  doincludes();
     }
     scbsgml = (struct restate *)rmalloc((TAGLVL+1)*sizeof(struct restate));
     tags = (struct tag *)rmalloc((TAGLVL+1)*sizeof(struct tag));
     grplongs = (GRPCNT + LONGBITS - 1)/LONGBITS;
     for (i = 0; i < TAGLVL+1; i++)
	  tags[i].tpos = newmpos();
     savedpos = newmpos();
}

/* SETDTYPE: Establish specified or default document type.
*/
VOID setdtype()
{
     /* Initialize default model hdr for declared content. */
     undechdr.ttype = MANY+MCHARS+MGI;  /* Declared content is ANY. */
     undechdr.tu.tnum = 0;              /* No content model. */

     /* Initialize content model and etd for *DOC. */
     prcon[0].ttype = MGI;    /* Model is an element model. */
     prcon[0].tu.tnum = 2;    /* A single group with a single GI in it. */
     prcon[1].ttype = TTSEQ;  /* Non-repeatable SEQ group. */
     prcon[1].tu.tnum = 1;    /* Only one token in group. */
     prcon[2].ttype = TTETD;  /* Token is an etd. */
     docetd = etddef(indocetd);  /* etd for document as a whole. */
     etdset(docetd, ETDOCC, prcon, (PETD *)0, (PETD *)0, SRMNULL);

     /* Put specified or default document type etd in *DOC model. */
     if (!dtype) {
          sgmlerr(E_DOCTYPE, propcb, (UNCH *)0, (UNCH *)0);
	  dtype = indefetd;
     }
     prcon[2].tu.thetd = etddef(dtype);
     if (!prcon[2].tu.thetd->etdmod) {
	  if (dtype != indefetd)
	       sgmlerr(52, propcb, dtype+1, (UNCH *)0);
          ++ds.etdercnt;
          etdset(prcon[2].tu.thetd, (UNCH)SMO+EMO+ETDUSED+ETDOCC, &undechdr,
                (PETD *)0, (PETD *)0, (PECB *)0);
     }
     TRACEETD(docetd);
     TRACEMOD(prcon);
     TRACEETD(prcon[2].tu.thetd);
     return;
}
/* PARSETAG: Tag end parser for SGML documents.
             For start-tags, it
             sets etisw to TAGNET if tag ended with ETI; otherwise to 0.
*/
VOID parsetag(pcb)
struct parse *pcb;            /* Parse control block: pcbstag or pcbetag. */
{
     tagdelsw = 1;            /* Assume tag had an ETI or TAGC. */
     switch (parse(pcb)) {
     case ETIC:               /* Tag closed with ETI. */
          if (!sd.shorttag) synerr(194, pcb);
          etisw = TAGNET;     /* Set switch for stack entry flag. */
          return;
     case DSC:
	  synerr(9, pcb);
	  REPEATCC;
	  etisw = 0;
	  return;
     case NVS:                /* Att name or value token found. */
     case NTV:                /* Name token value found. */
          synerr(E_POSSATT, pcb);
          pcb->newstate = 0;  /* Reset parse state. */
          REPEATCC;           /* Put it back for next read. */
          tagdelsw = 0;       /* Tag had no closing delimiter. */
          etisw = 0;          /* Don't flag stack entry. */
	  return;
     case TAGO:               /* Tag closing implied by TAGO. */
	  if (!sd.shorttag) synerr(193, pcb);
          REPEATCC;           /* Put it back for next read. */
          tagdelsw = 0;       /* Tag had no closing delimiter. */
     case TAGC:               /* Normal close. */
     default:                 /* Invalid character (msg was sent). */
          etisw = 0;          /* Don't flag stack entry. */
          return;
     }
}
/* STAG: Check whether a start-tag is valid at this point in the document
         structure, or whether other tags must precede it.
         Special case processing is done for the fake tag, #CDATA, as
         it is never stacked.
*/
int stag(dataret)
int dataret;                  /* Data pending: DAF_ REF_ 0=not #PCDATA. */
{
     int rc, realrc;          /* Return code from context or other test. */
     int mexts = 0;           /* >0=stack level of minus grp; -1=plus; 0=none.*/

     badresw = pexsw = 0;
     /* If real element (i.e., not #PCDATA) set mexts and test if empty. */
     if (dataret==0) {
          mexts = pexmex(newetd);
          /* If element is declared empty, it is same as a conref. */
          if (GET(newetd->etdmod->ttype, MNONE)) conrefsw = TAGREF;
     }
     if (GET(tags[ts].tetd->etdmod->ttype, MANY))
          rc = mexts>0 ? RCMEX : RCHIT;
     else rc = context(newetd, tags[ts].tetd->etdmod, tags[ts].tpos,
                       &tags[ts].status, mexts);
     TRACESTG(newetd, dataret, rc, nextetd, mexts);

     switch (rc) {
     case RCEND:         /* End current element, then retry start-tag. */
          if (ts<1) realrc = RCMISS;
          else      realrc = RCEND;
          break;
     case RCREQ:         /* Stack compulsory GI, then retry start-tag. */
          realrc = RCREQ;
          break;
     case RCMISS:        /* Start-tag invalid (#PCDATA or real). */
          if (ts>0 && GET(tags[ts].tetd->etdmod->ttype, MANY))
               realrc = RCEND;
          else realrc = RCMISS;
          break;
     case RCMEX:         /* Start-tag invalid (minus exception). */
          etagimct = ts - mexts;
          realrc = RCEND;
          break;
     case RCHITMEX:      /* Invalid minus exclusion for required element. */
          sgmlerr(216, &pcbstag, NEWGI, tags[mexts].tetd->etdgi+1);
	  /* fall through */
     case RCHIT:         /* Start-tag was valid. */
          realrc = RCHIT;
          break;
     case RCPEX:         /* Start-tag valid only because of plus exception. */
          pexsw = TAGPEX;
          realrc = RCHIT;
          break;
     default:
	  abort();
     }

     switch (realrc) {
     case RCEND:         /* End current element, then retry start-tag. */
          if (didreq) sgmlerr(07, &pcbstag, nextetd->etdgi+1, (UNCH *)0);
          didreq = 0;                   /* No required start-tag done. */
          dostag = 1; etiswsv = etisw;  /* Save real start-tag status. */
          conrefsv = conrefsw;          /* Save real start-tag conref. */
          conrefsw = 0;                 /* Current element is not empty. */
          etagmin = MINSTAG; destack(); /* Process omitted end-tag. */
          return ETG_;
     case RCREQ:         /* Stack compulsory GI, then retry start-tag. */
          if (!BADPTR(nextetd)) {
               if ((mexts = pexmex(nextetd))>0)
		    sgmlerr(E_MEXERR, &pcbstag, nextetd->etdgi+1,
			    tags[mexts].tetd->etdgi+1);
               if (!nextetd->etdmod) {
                    sgmlerr(53, &pcbstag, nextetd->etdgi+1, (UNCH *)0);
                    etdset(nextetd, (UNCH)SMO+EMO+ETDOCC, &undechdr,
                          (PETD *)0, (PETD *)0, (PECB *)0);
                    ++ds.etdercnt;
                    TRACEETD(nextetd);
               }
          }
          if (BITOFF(nextetd->etdmin, SMO)) {
               if (!BADPTR(stagreal))
                    sgmlerr(21, &pcbstag, nextetd->etdgi+1, stagreal->etdgi+1);
               else if (stagreal==ETDCDATA)
                    sgmlerr(49, &pcbstag, nextetd->etdgi+1, (UNCH *)0);
               else sgmlerr(50, &pcbstag, nextetd->etdgi+1, (UNCH *)0);
          }
          didreq = 1;                   /* Required start-tag done. */
          dostag = 1; etiswsv = etisw;  /* Save real start-tag status. */
          etisw = 0; conrefsv = conrefsw;  /* Save real start-tag conref. */
          /* If element is declared empty, it is same as a conref. */
          conrefsw = (GET(nextetd->etdmod->ttype, MNONE)) ? TAGREF : 0;
          stack(nextetd);               /* Process omitted start-tag. */
          return STG_;
     case RCMISS:        /* Start-tag invalid (#PCDATA or actual). */
          dostag = 0; contersw |= 1; didreq = 0;
          if (dataret) {
               if (dataret==REF_) badresw = 1;
               else sgmlerr(E_CHARS, conpcb, tags[ts].tetd->etdgi+1, (UNCH *)0);
               return dataret;
          }
          sgmlerr(E_CONTEXT, &pcbstag, NEWGI, tags[ts].tetd->etdgi+1);
          if (stagmin!=MINNULL) stagmin = MINNONE; stack(newetd);
          return STG_;
     case RCHIT:         /* Start-tag was valid. */
          dostag = 0; didreq = 0;
          if (dataret) return dataret;
          stack(newetd);
          return STG_;
     }
     return NOP_;        /* To avoid Borland C++ warning */
}
/* PEXMEX: See if a GI is in a plus or minus exception group on the stack.
           If in a minus, returns stack level of minus group; otherwise,
           returns -1 if in a plus and not a minus, and zero if in neither.
*/
int pexmex(curetd)
struct etd *curetd;           /* The etd for this GI. */
{
     int tsl;                 /* Temporary stack level for looping. */
     int pex = 0;             /* 1=found in plus grp; 0=not. */

     for (tsl = ts; tsl>0; --tsl) {
          if (tags[tsl].tetd->etdmex && ingrp(tags[tsl].tetd->etdmex, curetd))
               return(tsl);
          if (tags[tsl].tetd->etdpex && ingrp(tags[tsl].tetd->etdpex, curetd))
               pex = -1;
     }
     return(pex);
}
/* STACK: Add a new entry to the tag stack.
          If there is no room, issue a message and reuse last position.
*/
VOID stack(curetd)
struct etd *curetd;           /* The etd for this entry. */
{
     /* Stack the new element type definition (error if no room). */
     if (++ts>TAGLVL)
          sgmlerr(E_STAGMAX, conpcb, curetd->etdgi+1, tags[--ts].tetd->etdgi+1);
     tags[ts].tetd = curetd;

     /* Set flags: plus exception + tag had ETI + context error + empty. */
     tags[ts].tflags = (UNCH)pexsw + etisw + contersw + conrefsw; contersw = 0;

     /* If tag had ETI, update ETI counter and enable NET if first ETI. */
     if (etisw && ++etictr==1) lexcon[lex.d.net] = lexcnm[lex.d.net] = lex.l.net;

     /* If etd has ALT table, use it; otherwise, use last element's ALT. */
     if (curetd->etdsrm) {
          if (curetd->etdsrm != SRMNULL && curetd->etdsrm[0] == NULL) {
	       /* Map hasn't been defined.  Ignore it.
		  We already gave an error. */
	       curetd->etdsrm = 0;
	       tags[ts].tsrm = tags[ts-1].tsrm;
	  }
	  else
    	       tags[ts].tsrm = curetd->etdsrm;
     }
     else
          tags[ts].tsrm = tags[ts-1].tsrm;

     /* Initialize rest of stack entry. */
     tags[ts].status = 0;
     tags[ts].tpos[0].g = 1;       /* M: Index in model of next token to test.*/
     tags[ts].tpos[0].t = 1;       /* P: Index in tpos of current group. */
     HITCLEAR(tags[ts].tpos[0].h);
     tags[ts].tpos[1].g = 1;       /* Index of group in model (dummy grp). */
     tags[ts].tpos[1].t = 1;       /* 1st token is next in grp to be tested. */
     HITCLEAR(tags[ts].tpos[1].h); /* No hits yet as yet. */
     TRACESTK(&tags[ts], ts, etictr);
     return;
}
/* ETAG: Check validity of an end-tag by seeing if it matches any tag
         on the stack.  If so, return the offset of the match from the
         current entry (0=current).  If there is no match, issue a message
         and return an error code (-1).
         If the newetd is ETDNET, a NET delimiter was found, so check for
         a tag that ended with ETI instead of a matching GI.
*/
int etag()
{
     int tsl = ts+1;          /* Temporary stack level for looping. */

     /* See if end-tag is anywhere on stack, starting at current entry. */
     while (--tsl) {
          if (newetd!=ETDNET ? newetd==tags[tsl].tetd : tags[tsl].tflags) {
               TRACEETG(&tags[ts], newetd, tsl, ts-tsl);
               return(ts-tsl);
          }
     }
     return (-1);             /* End-tag didn't match any start-tag. */
}
/* DESTACK:
            Call ECONTEXT to see if element can be ended at this point.
            and issue message if there are required tags left.
            Remove the current entry from the tag stack.
            Issue an error if the destacked element was not minimizable
            and its end-tag was omitted.
*/
VOID destack()
{
     register int ecode = 0;  /* Error code (0=o.k.). */
     UNCH *eparm2 = NULL;     /* Second parameter of error message. */
     register int minmsgsw;   /* 1=message if tag omitted; 0=no message. */

     /* If element has a content model (i.e., not a keyword) and there
        are required tags left, and no CONREF attribute was specified,
        issue an error message.
     */
     lastetd = tags[ts].tetd;
     if (!GET(tags[ts].tetd->etdmod->ttype, MKEYWORD)
	 && !conrefsw
	 && !econtext(tags[ts].tetd->etdmod, tags[ts].tpos, &tags[ts].status)) {
          if (BADPTR(nextetd))
               sgmlerr(54, conpcb, tags[ts].tetd->etdgi+1, (UNCH *)0);
          else
	       sgmlerr(30, conpcb, tags[ts].tetd->etdgi+1, nextetd->etdgi+1);
     }
     /* If the current tag ended with ETI, decrement the etictr.
        If etictr is now zero, disable the NET delimiter.
     */
     if (GET(tags[ts--].tflags, TAGNET) && --etictr==0)
          lexcon[lex.d.net] = lexcnm[lex.d.net] = lex.l.nonet;

     minmsgsw = BITOFF(tags[ts+1].tetd->etdmin, EMO);
     if (!conrefsw && minmsgsw && (etagimsw || etagmin==MINETAG)) {
          /* Minimization caused by NET delimiter. */
          if (BADPTR(etagreal)) ecode = 46;
          /* Minimization caused by a containing end-tag. */
          else {ecode = 20; eparm2 = etagreal->etdgi+1;}
     }
     else if (!conrefsw && etagmin==MINSTAG && (minmsgsw || ts<=0)) {
          /* Minimization caused by out-of-context start-tag. */
          if (!BADPTR(stagreal)) {
               ecode = ts>0 ? 39 : 89;
               eparm2 = stagreal->etdgi+1;
          }
          /* Minimization caused by out-of-context data. */
          else if (stagreal==ETDCDATA) ecode = ts>0 ? 47 : 95;
          /* Minimization caused by out-of-context short start-tag. */
          else ecode = ts>0 ? 48 : 96;
          if (ts<=0 && ecode) eodsw = 1;
     }
     if (ecode) sgmlerr((UNS)ecode, conpcb, tags[ts+1].tetd->etdgi+1, eparm2);
     /* TEMP: See if parser bug caused stack to go below zero. */
     else if (ts<0) {sgmlerr(64, conpcb, (UNCH *)0, (UNCH *)0); ts = 0;}
     TRACEDSK(&tags[ts], &tags[ts+1], ts, etictr);
     if (ts == 0) {
	  docelsw = 1;	      /* Finished document element. */
	  if (es > 0) sgmlerr(231, conpcb, (UNCH *)0, (UNCH *)0);
     }
}
/*
Local Variables:
c-indent-level: 5
c-continued-statement-offset: 5
c-brace-offset: -5
c-argdecl-indent: 0
c-label-offset: -5
comment-column: 30
End:
*/
