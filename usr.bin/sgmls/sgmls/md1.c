#include "sgmlincl.h"         /* #INCLUDE statements for SGML parser. */
/* MDADL: Process ATTLIST declaration.
*/
VOID mdadl(tbuf)
UNCH *tbuf;                   /* Work area for tokenization (tbuf). */
{
     int i;                   /* Loop counter; temporary variable. */
     int adlim;               /* Number of unused ad slots in al. */
     struct ad *alperm = 0;   /* Attribute definition list. */
     int stored = 0;

     mdname = key[KATTLIST];  /* Identify declaration for messages. */
     subdcl = 0;              /* No subject as yet. */
     parmno = 0;              /* No parameters as yet. */
     mdessv = es;             /* Save es level for entity nesting check. */
     reqadn = noteadn = 0;    /* No required attributes yet. */
     idadn = conradn = 0;     /* No special atts yet.*/
     AN(al) = 0;	      /* Number of attributes defined. */
     ADN(al) = 0;             /* Number of ad's in al (atts + name vals).*/
     /* PARAMETER 1: Element name or a group of them.
     */
     parsemd(tbuf, NAMECASE, &pcblitp, NAMELEN);
     TRACEMD("1: element name or group");
     switch (pcbmd.action) {
     case NAS:
          nmgrp[0] = etddef(tbuf);
          nmgrp[1] = 0;
          break;
     case GRPS:
          parsegrp(nmgrp, &pcbgrnm, tbuf);
          break;
     case RNS:           /* Reserved name started. */
          if (ustrcmp(tbuf+1, key[KNOTATION])) {
               mderr(118, tbuf+1, key[KNOTATION]);
               return;
          }
          mdnadl(tbuf);
          return;
     default:
          mderr(121, (UNCH *)0, (UNCH *)0);
          return;
     }
     /* Save first GI for error msgs. */
     if (nmgrp[0])
	  subdcl = nmgrp[0]->etdgi+1;
     /* PARAMETER 2: Attribute definition list.
     */
     parsemd(tbuf, NAMECASE, &pcblitp, NAMELEN);
     TRACEMD("2: attribute list");
     if (pcbmd.action!=NAS) {
          mderr(120, (UNCH *)0, (UNCH *)0);
          return;
     }
     while (pcbmd.action==NAS) {
	  al[ADN(al)+1].adname = savenm(tbuf);
          if ((adlim = ATTCNT-((int)++ADN(al)))<0) {
	       mderr(111, (UNCH *)0, (UNCH *)0);
	       adlfree(al, 1);
	       return;
	  }
          ++AN(al);
          if (mdattdef(adlim, 0)) {
	       adlfree(al, 1);
	       return;
	  }
          parsemd(tbuf, NAMECASE, &pcblitp, NAMELEN);
     }
     if (AN(al)>0) {   /*  Save list only if 1 or more good atts. */
          if (reqadn)  SET(ADLF(al), ADLREQ);    /* Element must have start-tag. */
          if (noteadn) SET(ADLF(al), ADLNOTE);   /* Element cannot be EMPTY. */
          if (conradn) SET(ADLF(al), ADLCONR);   /* Element cannot be EMPTY. */
          alperm = (struct ad *)rmalloc((1+ADN(al))*ADSZ);
          memcpy((UNIV)alperm, (UNIV)al, (1+ADN(al))*ADSZ );
          ds.attcnt += AN(al);         /* Number of attributes defined. */
          ds.attgcnt += ADN(al) - AN(al);  /* Number of att grp members. */
          TRACEADL(alperm);
     }
     /* Clear attribute list for next declaration. */
     MEMZERO((UNIV)al, (1+ADN(al))*ADSZ);

     /* PARAMETER 3: End of declaration.
     */
     /* Next pcb.action was set during attribute definition loop. */
     TRACEMD(emd);
     if (pcbmd.action!=EMD) {mderr(126, (UNCH *)0, (UNCH *)0); return;}
     if (es!=mdessv) synerr(37, &pcbmd);

     /* EXECUTE: Store the definition for each element name specified.
     */
     TRACEGRP(nmgrp);
     for (i = 0; nmgrp[i]; i++) {
          if (nmgrp[i]->adl) {     /* Error if an ADL exists. */
               mderr(112, (UNCH *)0, (UNCH *)0);
               continue;
          }
          nmgrp[i]->adl = alperm;  /* If virgin, store the adl ptr. */
	  stored = 1;
          if (alperm && nmgrp[i]->etdmod)
	       etdadl(nmgrp[i]); /* Check for conflicts with ETD. */
     }
     if (!stored && alperm) {
	  adlfree(alperm, 1);
	  frem((UNIV)alperm);
     }
}
/* ETDADL: Check compatibility between ETD and ADL.
*/
VOID etdadl(p)
struct etd *p;                /* Pointer to element type definition. */
{
     parmno = 0;
     /* Minimizable element cannot have required attribute. */
     if (GET(p->etdmin, SMO) && GET(p->adl[0].adflags, ADLREQ)) {
          mderr(40, (UNCH *)0, (UNCH *)0);
          RESET(p->etdmin, SMO);
     }
     /* Empty element cannot have NOTATION attribute.
        Attribute is not removed (too much trouble), but we trap
        attempts to specify it on the start-tag in adlval().
     */
     if (GET(p->etdmod->ttype, MNONE)) {
          if (GET(p->adl[0].adflags, ADLNOTE))
               mderr(83, (UNCH *)0, (UNCH *)0);

          /* Empty element cannot have CONREF attribute.
             Attribute is not removed because it just acts
             like IMPLIED anyway.
          */
          if (GET(p->adl[0].adflags, ADLCONR))
               mderr(85, (UNCH *)0, (UNCH *)0);
     }
     /* "-" should not be specified for the end-tag minimization if
	the element has a content reference attribute. */
     if (GET(p->adl[0].adflags, ADLCONR) && BITON(p->etdmin, EMM))
	  mderr(153, (UNCH *)0, (UNCH *)0);
}
/* MDNADL: Process ATTLIST declaration for notation.
           TO DO: Pass deftab and dvtab as parameters so
           that prohibited types can be handled by leaving
           them out of the tables.
*/
VOID mdnadl(tbuf)
UNCH *tbuf;                   /* Work area for tokenization (tbuf). */
{
     int i;                   /* Loop counter; temporary variable. */
     int adlim;               /* Number of unused ad slots in al. */
     struct ad *alperm = 0;   /* Attribute definition list. */
     int stored = 0;

     /* PARAMETER 1: Notation name or a group of them.
     */
     parsemd(tbuf, NAMECASE, &pcblitp, NAMELEN);
     TRACEMD("1: notation name or group");
     switch (pcbmd.action) {
     case NAS:
          nnmgrp[0] = dcndef(tbuf);
          nnmgrp[1] = 0;
          break;
     case GRPS:
          parsngrp(nnmgrp, &pcbgrnm, tbuf);
          break;
     default:
          mderr(121, (UNCH *)0, (UNCH *)0);
          return;
     }
     subdcl = nnmgrp[0]->ename+1;        /* Save first name for error msgs. */
     /* PARAMETER 2: Attribute definition list.
     */
     parsemd(tbuf, NAMECASE, &pcblitp, NAMELEN);
     TRACEMD("2: attribute list");
     if (pcbmd.action!=NAS) {
          mderr(120, (UNCH *)0, (UNCH *)0);
          return;
     }
     while (pcbmd.action==NAS) {
	  al[ADN(al)+1].adname = savenm(tbuf);
          if ((adlim = ATTCNT-((int)ADN(al)++))<0) {
	       mderr(111, (UNCH *)0, (UNCH *)0);
	       adlfree(al, 1);
	       return;
	  }
          ++AN(al);
          if (mdattdef(adlim, 1)) {
	       adlfree(al, 1);
	       return;
	  }
          parsemd(tbuf, NAMECASE, &pcblitp, NAMELEN);
     }
     if (AN(al)>0) {   /*  Save list only if 1 or more good atts. */
          alperm = (struct ad *)rmalloc((1+ADN(al))*ADSZ);
          memcpy((UNIV)alperm, (UNIV)al, (1+ADN(al))*ADSZ );
          ds.attcnt += AN(al);         /* Number of attributes defined. */
          ds.attgcnt += ADN(al) - AN(al);  /* Number of att grp members. */
          TRACEADL(alperm);
     }
     /* Clear attribute list for next declaration. */
     MEMZERO((UNIV)al, (1+ADN(al))*ADSZ);

     /* PARAMETER 3: End of declaration.
     */
     /* Next pcb.action was set during attribute definition loop. */
     TRACEMD(emd);
     if (pcbmd.action!=EMD) {mderr(126, (UNCH *)0, (UNCH *)0); return;}
     if (es!=mdessv) synerr(37, &pcbmd);

     /* EXECUTE: Store the definition for each notation name specified.
     */
     TRACENGR(nnmgrp);
     for (i = 0; nnmgrp[i]; i++) {
          if (nnmgrp[i]->adl) {     /* Error if an ADL exists. */
               mderr(112, (UNCH *)0, (UNCH *)0);
               continue;
          }
          nnmgrp[i]->adl = alperm;  /* If virgin, store the adl ptr. */
	  if (nnmgrp[i]->entsw)
	       fixdatt(nnmgrp[i]);
	  stored = 1;
          TRACEDCN(nnmgrp[i]);
     }
     if (!stored && alperm) {
	  adlfree(alperm, 1);
	  frem((UNIV)alperm);
     }
}

/* Data attributes have been specified for notation p, but entities
have already been declared with notation p.  Fix up the definitions of
all entities with notation p.  Generate an error for any data
attribute that was required. */

VOID fixdatt(p)
struct dcncb *p;
{
     int i;
     for (i = 0; i < ENTHASH; i++) {
	  struct entity *ep;
	  for (ep = etab[i]; ep; ep = ep->enext)
	       if (ep->estore == ESN && ep->etx.n && ep->etx.n->nedcn == p) {
		    int adn;
		    initatt(p->adl);
		    /* Don't use adlval because if there were required
		       attributes the error message wouldn't say what
		       entity was involved. */
		    for (adn = 1; adn <= ADN(al); adn++) {
			 if (GET(ADFLAGS(al,adn), AREQ)) {
			      sgmlerr(218, &pcbstag, ADNAME(al,adn),
				      ep->ename + 1);
			      SET(ADFLAGS(al,adn), AINVALID);
			 }
			 if (BITON(ADFLAGS(al, adn), AGROUP))
			      adn += ADNUM(al, adn);
		    }
		    storedatt(ep->etx.n);
	       }
     }
}

/* MDATTDEF: Process an individual attribute definition.
             The attribute name is parsed by the caller.
             Duplicate attributes are parsed, but removed from list.
             Returns 0 if successful, otherwise returns 1.
*/
int mdattdef(adlim, datt)
int adlim;                    /* Remaining capacity of al (in tokens).*/
int datt;		      /* Non-zero if a data attribute. */
{
     int deftype;             /* Default value type: 0=not keyword. */
     int errsw = 0;           /* 1=semantic error; ignore att. */
     int novalsw = 0;         /* 1=semantic error; treat as IMPLIED. */
     int attadn = (int)ADN(al);   /* Save ad number of this attribute. */
     struct parse *grppcb = NULL; /* PCB for name/token grp parse. */
     int errcode;             /* Error type returned by PARSEVAL, ANMTGRP. */
     UNCH *advalsv;           /* Save area for permanent value ptr. */

     /* PARAMETER 1: Attribute name (parsed by caller).
     */
     TRACEMD("1: attribute name");
     if (anmget((int)ADN(al)-1, al[attadn].adname)) {
          errsw = 1;
          mderr(99, ADNAME(al,attadn), (UNCH *)0);
     }
     ADNUM(al,attadn) = ADFLAGS(al,attadn) = ADLEN(al,attadn) = 0;
     ADVAL(al,attadn) = 0; ADDATA(al,attadn).x = 0; ADTYPE(al,attadn) = ANMTGRP;
     /* PARAMETER 2: Declared value.
     */
     parsemd(lbuf, NAMECASE, &pcblitp, NAMELEN);
     TRACEMD("2: declared value");
     switch (pcbmd.action) {
     case NAS:                /* Keyword for value type. */
          switch (ADTYPE(al,attadn) = (UNCH)mapsrch(dvtab, lbuf+1)) {
          case 0:
               mderr(100, ADNAME(al,attadn), lbuf+1);
               return 1;
          case ANOTEGRP:
	       if (datt) {
		    errsw = 1;
		    mderr(156, (UNCH *)0, (UNCH *)0);
	       }
               else if (!noteadn) noteadn = ADN(al);
               else {
                    errsw = 1;
                    mderr(101, ADNAME(al,attadn), (UNCH *)0);
               }
               grppcb = &pcbgrnm;         /* NOTATION requires name grp. */
               parsemd(lbuf, NAMECASE, &pcblitp, NAMELEN);/* Get GRPO*/
               break;
          case AID:
	       if (datt) {
		    errsw = 1;
		    mderr(144, (UNCH *)0, (UNCH *)0);
	       }
               else if (!idadn)
		    idadn = attadn;
               else {
                    errsw = 1;
                    mderr(102, ADNAME(al,attadn), (UNCH *)0);
               }
               break;
	  case AIDREF:
	  case AIDREFS:
	       if (datt) {
		    errsw = 1;
		    mderr(155, (UNCH *)0, (UNCH *)0);
	       }
	       break;
	  case AENTITY:
	  case AENTITYS:
	       if (datt) {
		    errsw = 1;
		    mderr(154, (UNCH *)0, (UNCH *)0);
	       }
	       break;
          }
          break;
     case GRPS:
          grppcb = &pcbgrnt;           /* Normal grp is name token grp. */
          break;
     case EMD:
          mderr(103, ADNAME(al,attadn), (UNCH *)0);
          return 1;
     default:
          mderr(104, ADNAME(al,attadn), (UNCH *)0);
          return 1;
     }
     /* PARAMETER 2A: Name token group.
     */
     if (grppcb != NULL) {
	  TRACEMD("2A: name group");
          switch (pcbmd.action) {
          case GRPS:               /* Name token list. */
               SET(ADFLAGS(al,attadn), AGROUP);
               /* Call routine to parse group, create ad entries in adl. */
               errcode = anmtgrp(grppcb, al+attadn,
				 (GRPCNT<adlim ? GRPCNT+1 : adlim+1),
				 &al[attadn].adnum, ADN(al));
               if (errcode<=0) {
		    if (adlim < GRPCNT)
			 mderr(111, (UNCH *)0, (UNCH *)0);
		    else
			 mderr(105, ADNAME(al,attadn), (UNCH *)0);
                    return 1;
               }
               ADN(al) += ADNUM(al,attadn);    /* Add grp size to total ad cnt.*/
               break;
          default:
               mderr(106, ADNAME(al,attadn), (UNCH *)0);
               return 1;
          }
     }
     /* PARAMETER 3: Default value keyword.
     */
     parsemd(lbuf, AVALCASE,
	     (ADTYPE(al,attadn)==ACHARS) ? &pcblitr : &pcblitt, LITLEN);
     TRACEMD("3: default keyword");
     switch (pcbmd.action) {
     case RNS:                /* Keyword. */
          deftype = mapsrch(deftab, lbuf+1);
          switch (deftype) {
          case DFIXED:        /* FIXED */
               SET(ADFLAGS(al,attadn), AFIXED);
               parsemd(lbuf, AVALCASE,
		       (ADTYPE(al,attadn)==ACHARS) ? &pcblitr : &pcblitt,
		       LITLEN);  /* Real default. */
               goto parm3x;   /* Go process specified value. */
          case DCURR:         /* CURRENT: If ID, treat as IMPLIED. */
               if (ADTYPE(al,attadn)==AID) {
                    mderr(80, ADNAME(al,attadn), (UNCH *)0);
                    break;
               }
	       if (datt) {
		    mderr(157, (UNCH *)0, (UNCH *)0);
		    break;
	       }
               SET(ADFLAGS(al,attadn), ACURRENT);
               break;
          case DREQ:          /* REQUIRED */
               SET(ADFLAGS(al,attadn), AREQ); ++reqadn;
               break;
          case DCONR:         /* CONREF */
               if (ADTYPE(al,attadn)==AID) {
                    mderr(107, ADNAME(al,attadn), (UNCH *)0);
                    break;
               }
	       if (datt) {
		    mderr(158, (UNCH *)0, (UNCH *)0);
		    break;
	       }
               SET(ADFLAGS(al,attadn), ACONREF); conradn = 1;
          case DNULL:         /* IMPLIED */
               break;
          default:            /* Unknown keyword is an error. */
               mderr(108, ADNAME(al,attadn), lbuf+1);
               errsw = 1;
          }
          if (errsw) {
	       /* Ignore erroneous att. */
	       adlfree(al, attadn);
	       --AN(al);
	       ADN(al) = (UNCH)attadn-1;
	  }
          return(0);
     default:
          break;
     }
     /* PARAMETER 3x: Default value (non-keyword).
     */
     parm3x:
     TRACEMD("3x: default (non-keyword)");
     if (ADTYPE(al,attadn)==AID) { /* If ID, treat as IMPLIED. */
          mderr(81, ADNAME(al,attadn), (UNCH *)0);
          novalsw = 1;	      /* Keep parsing to keep things straight. */
     }
     switch (pcbmd.action) {
     case LIT:                /* Literal. */
     case LITE:               /* Literal. */
          /* Null string (except CDATA) is error: msg and treat as IMPLIED. */
          if (*lbuf == '\0' && ADTYPE(al,attadn)!=ACHARS) {
               mderr(82, ADNAME(al,attadn), (UNCH *)0);
               novalsw = 1;
          }
	  break;
     case NAS:                /* Name character string. */
     case NMT:                /* Name character string. */
     case NUM:                /* Number or number token string. */
	  /* The name won't have a length byte because AVALCASE was specified. */
          break;
     case CDR:
	  parsetkn(lbuf, NMC, LITLEN);
	  break;
     case EMD:
          mderr(109, ADNAME(al,attadn), (UNCH *)0);
          return 1;
     default:
          mderr(110, ADNAME(al,attadn), (UNCH *)0);
          return 1;
     }
     if (errsw) {
	  /* Ignore erroneous att. */
	  adlfree(al, attadn);
	  --AN(al);
	  ADN(al) = (UNCH)attadn-1;
	  return(0);
     }
     if (novalsw) return(0);

     /* PARAMETER 3y: Validate and store default value.
     */
     if (ADTYPE(al,attadn)==ACHARS) {
	  UNS len = vallen(ACHARS, 0, lbuf);
	  if (len > LITLEN) {
	       /* Treat as implied. */
	       sgmlerr(224, &pcbmd, ADNAME(al,attadn), (UNCH *)0);
	       return 0;
	  }
          /* No more checking for CDATA value. */
          ADNUM(al,attadn) = 0;             /* CDATA is 0 tokens. */
          ADVAL(al,attadn) = savestr(lbuf);/* Store default; save ptr. */
          ADLEN(al,attadn) = len;
          ds.attdef += len;
          return 0;
     }
     /* Parse value and save token count (GROUP implies 1 token). */
     advalsv = (UNCH *)rmalloc(ustrlen(lbuf)+2); /* Storage for tokenized value. */
     errcode = parseval(lbuf, (UNS)ADTYPE(al,attadn), advalsv);
     if (BITOFF(ADFLAGS(al,attadn), AGROUP)) ADNUM(al,attadn) = (UNCH)tokencnt;

     /* If value was invalid, or was a group member that was not in the group,
        issue an appropriate message and set the error switch. */
     if (errcode)
          {sgmlerr((UNS)errcode, &pcbmd, ADNAME(al,attadn), lbuf); errsw = 1;}
     else if ( BITON(ADFLAGS(al,attadn), AGROUP)
          && !amemget(&al[attadn], (int)ADNUM(al,attadn), advalsv) ) {
               sgmlerr(79, &pcbmd, ADNAME(al,attadn), advalsv+1);
               errsw = 1;
     }
     ADLEN(al,attadn) = vallen(ADTYPE(al,attadn), ADNUM(al,attadn), advalsv);
     if (ADLEN(al,attadn) > LITLEN) {
	  sgmlerr(224, &pcbmd, ADNAME(al,attadn), (UNCH *)0);
	  ADLEN(al,attadn) = 0;
	  errsw = 1;
     }
     /* For valid tokenized value, save it and update statistics. */
     if (!errsw) {
	  ADVAL(al,attadn) = advalsv;
          ds.attdef += ADLEN(al,attadn);
          return 0;
     }
     /* If value was bad, free the value's storage and treat as
        IMPLIED or REQUIRED. */
     frem((UNIV)advalsv);          /* Release storage for value. */
     ADVAL(al,attadn) = NULL;         /* And make value NULL. */
     return 0;
}
/* ANMTGRP: Parse a name or name token group, create attribute descriptors
            for its members, and add them to the attribute descriptor list.
            The parse either terminates or returns a good token, so no
            switch is needed.
*/
int anmtgrp(pcb, nt, grplim, adn, adsz)
struct parse *pcb;            /* PCB for name or name token grp. */
struct ad nt[];               /* Buffer for creating name token list. */
int grplim;                   /* Maximum size of list (plus 1). */
UNS *adn;		      /* Ptr to number of names or tokens in grp. */
int adsz;                     /* Size of att def list. */
{
     UNCH adtype = (UNCH)(pcb==&pcbgrnt ? ANMTGRP:ANOTEGRP);/*Attribute type.*/
     int essv = es;           /* Entity stack level when grp started. */

     *adn = 0;                /* Group is empty to start. */
     while (parse(pcb)!=GRPE && *adn<grplim) {
          switch (pcb->action) {
          case NAS_:          /* Name or name token (depending on pcb). */
          case NMT_:
               parsenm(lbuf, NAMECASE);
	       nt[*adn+1].adname = savenm(lbuf);
               if (antvget((int)(adsz+*adn), nt[*adn+1].adname, (UNCH **)0))
                    mderr(98, ntoa((int)*adn+1), nt[*adn+1].adname+1);
               nt[++*adn].adtype = adtype;
               nt[*adn].addef    = NULL;
               continue;

          case EE_:           /* Entity ended (correctly or incorrectly). */
               if (es<essv) {synerr(37, pcb); essv = es;}
               continue;

          case PIE_:          /* PI entity reference (invalid). */
               entpisw = 0;   /* Reset PI entity indicator. */
               synerr(59, pcb);
               continue;

          default:
               break;
          }
          break;
     }
     if (es!=essv) synerr(37, pcb);
     if (*adn==grplim) return -1;
     else return *adn;        /* Return number of tokens. */
}
/* MDDTDS: Process start of DOCTYPE declaration (through MSO).
*/
VOID mddtds(tbuf)
UNCH *tbuf;                   /* Work area for tokenization[LITLEN+2]. */
{
     struct fpi fpicb;        /* Formal public identifier structure. */
     union etext etx;         /* Ptr to entity text. */
     UNCH estore = ESD;       /* Entity storage class. */
     int emdsw = 0;           /* 1=end of declaration found; 0=not yet. */

     mdname = key[KDOCTYPE];  /* Identify declaration for messages. */
     subdcl = NULL;           /* No subject as yet. */
     parmno = 0;              /* No parameters as yet. */
     mdessv = es;             /* Save es for checking entity nesting. */
     dtdrefsw = 0;            /* No external DTD entity as yet. */
     /* PARAMETER 1: Document type name.
     */
     pcbmd.newstate = 0;
     parsemd(tbuf, NAMECASE, &pcblitp, NAMELEN);
     TRACEMD("1: doc type name");
     if (pcbmd.action!=NAS) {mderr(120, (UNCH *)0, (UNCH *)0); return;}
     dtype = savenm(tbuf);
     subdcl = dtype+1;        /* Subject of declaration for error msgs. */

     /* PARAMETER 2: External identifier keyword or MDS.
     */
     pcbmd.newstate = 0;
     parsemd(tbuf, NAMECASE, &pcblitp, NAMELEN);
     TRACEMD("2: extid or MDS");
     switch (pcbmd.action) {
     case NAS:
          if (mdextid(tbuf, &fpicb, dtype+1, &estore, (PNE)0)==0) return;
          if ((etx.x = entgen(&fpicb))==0)
	       mderr(146, dtype+1, (UNCH *)0);
	  else
	       dtdrefsw = 1;  /* Signal external DTD entity. */
          break;
     case MDS:
          goto execute;
     default:
          mderr(128, (UNCH *)0, (UNCH *)0);
          return;
     }
     /* PARAMETER 3: MDS or end of declaration.
     */
     TRACEMD("3: MDS or EMD");
     switch (pcbmd.action) {
     default:                      /* Treat as end of declaration. */
          mderr(126, (UNCH *)0, (UNCH *)0);
     case EMD:
          emdsw = 1;
     case MDS:
          break;
     }
     /* EXECUTE: Store entity definition if an external ID was specified.
     */
     execute:
     if (es!=mdessv) synerr(37, &pcbmd);
     propcb = &pcbmds;        /* Prepare to parse doc type definition (MDS). */
     if (dtdrefsw) {
	  /* TO DO: If concurrent DTD's supported, free existing
	     etext for all but first DTD (or reuse it). */
	  entdef(indtdent, estore, &etx);
	  ++ds.ecbcnt; ds.ecbtext += entlen;
          if (emdsw) {
               REPEATCC;                /* Push back the MDC. */
               *FPOS = lex.d.msc;       /* Simulate end of DTD subset. */
               REPEATCC;                /* Back up to read MSC next. */
               delmscsw = 1;            /* Insert MSC after referenced DTD. */
          }
     }
     indtdsw = 1;                       /* Allow "DTD only" parameters. */
     return;
}
/* MDDTDE: Process DOCTYPE declaration end.
*/
VOID mddtde(tbuf)
UNCH *tbuf;                   /* Work area for tokenization. */
{
     mdessv = es;             /* Save es for checking entity nesting. */
     propcb = &pcbpro;        /* Restore normal prolog parse. */
     indtdsw = 0;             /* Prohibit "DTD only" parameters. */

     mdname = key[KDOCTYPE];  /* Identify declaration for messages. */
     subdcl = dtype+1;        /* Subject of declaration for error msgs. */
     parmno = 0;              /* No parameters as yet. */
     /* PARAMETER 4: End of declaration.
     */
     pcbmd.newstate = 0;
     parsemd(tbuf, NAMECASE, &pcblitp, LITLEN);
     TRACEMD(emd);
     if (pcbmd.action!=EMD) mderr(126, (UNCH *)0, (UNCH *)0);
     if (es!=mdessv) synerr(37, &pcbmd);
}
/* MDELEM: Process ELEMENT declaration.
*/
VOID mdelem(tbuf)
UNCH *tbuf;                   /* Work area for tokenization (tbuf). */
{
     UNCH *ranksuff = lbuf;   /* Rank suffix. */
     UNS dctype = 0;          /* Declared content type (from dctab). */
     UNCH fmin = 0;           /* Minimization bit flags. */
     int i;                   /* Loop counter. */
     UNS u;                   /* Temporary variable. */
     struct etd **mexgrp, **pexgrp; /* Ptr to model exceptions array. */
     struct thdr *cmod, *cmodsv;    /* Ptr to content model. */
     UNCH *etdgi;             /* GI of current etd (when going through group).*/
     int minomitted = 0;      /*  Tag minimization parameters omitted. */

     mdname = key[KELEMENT];  /* Identify declaration for messages. */
     subdcl = NULL;           /* No subject as yet. */
     parmno = 0;              /* No parameters as yet. */
     mdessv = es;             /* Save es level for entity nesting check. */
     ranksuff[0] = 0;
     mexgrp = pexgrp = 0;

     /* PARAMETER 1: Element name or a group of them.
     */
     parsemd(tbuf, NAMECASE, &pcblitp, NAMELEN);
     TRACEMD("1: element name or grp");
     switch (pcbmd.action) {
     case NAS:
          nmgrp[0] = etddef(tbuf);
          nmgrp[1] = 0;
          break;
     case GRPS:
          parsegrp(nmgrp, &pcbgrnm, tbuf);
          break;
     default:
          mderr(121, (UNCH *)0, (UNCH *)0);
          return;
     }
     /* Save first GI for trace and error messages. */
     if (nmgrp[0])
	  subdcl = nmgrp[0]->etdgi+1;

     /* PARAMETER 1A: Rank suffix (optional).
     */
     parsemd(tbuf, NAMECASE, &pcblitp, NAMELEN);
     TRACEMD("1A: rank suffix");
     switch (pcbmd.action) {
     case NUM:
          ustrcpy(ranksuff, tbuf);
          parsemd(tbuf, NAMECASE, &pcblitp, NAMELEN);
     default:
          break;
     }
     /* PARAMETER 2A: Start-tag minimization.
     */
     TRACEMD("2A: start min");
     switch (pcbmd.action) {
     case CDR:
          break;
     case NAS:
	  if (!ustrcmp(tbuf+1, key[KO])) {
	       if (OMITTAG==YES) SET(fmin, SMO);
	       break;
	  }
	  /* fall through */
     default:
	  if (OMITTAG==NO) {minomitted=1; break;}
          mderr(129, tbuf+1, (UNCH *)0);
          return;
     }
     /* Must omit omitted end-tag minimization, if omitted
	start-tag minimization was omitted (because OMITTAG == NO). */
     if (!minomitted) {
	  /* PARAMETER 2B: End-tag minimization.
	   */
	  parsemd(tbuf, NAMECASE, &pcblitp, NAMELEN);
	  TRACEMD("2B: end min");
	  switch (pcbmd.action) {
	  case NAS:
	       if (ustrcmp(tbuf+1, key[KO])) {mderr(129, tbuf+1, (UNCH *)0); return;}
	       if (OMITTAG==YES) SET(fmin, EMO);
	       break;
	  case CDR:
	       SET(fmin, EMM);
	       break;
	  default:
	       mderr(129, tbuf+1, (UNCH *)0);
	       return;
	  }
	  /* PARAMETER 3: Declared content.
	   */
	  parsemd(tbuf, NAMECASE, &pcblitp, NAMELEN);
     }
     TRACEMD("3: declared content");
     switch (pcbmd.action) {
     case NAS:
          dctype = mapsrch(dctab, tbuf+1);
          if (!dctype) {mderr(24, tbuf+1, (UNCH *)0); return;}
          /* Eliminate incompatibilities among parameters. */
          if (GET(fmin, SMO) && GET(dctype, MNONE+MCDATA+MRCDATA)) {
               mderr(58, (UNCH *)0, (UNCH *)0);
               RESET(fmin, SMO);
          }
          if (GET(dctype, MNONE) && BITON(fmin, EMM)) {
	       mderr(87, (UNCH *)0, (UNCH *)0);
               SET(fmin, EMO);
          }
          /* If valid, process like a content model. */
     case GRPS:
          cmodsv = parsemod((int)(pcbmd.action==GRPS ? 0 : dctype));
          if (cmodsv==0) return;
	  u = (dctype ? 1 : cmodsv->tu.tnum+2) * THSZ;
          cmod = (struct thdr *)rmalloc(u);
          memcpy((UNIV)cmod  , (UNIV)cmodsv, u );
	  ds.modcnt += cmod->tu.tnum;
          TRACEMOD(cmod);
          break;
     default:
          mderr(130, (UNCH *)0, (UNCH *)0);
          return;
     }
     /* PARAMETERS 3A, 3B: Exceptions or end.
     */
     parsemd(tbuf, NAMECASE, &pcblitp, NAMELEN);
     if (BITOFF(cmod->ttype, MCDATA+MRCDATA+MNONE)) {
          /* PARAMETER 3A: Minus exceptions.
          */
          TRACEMD("3A: -grp");
          switch (pcbmd.action) {
          case MGRP:
	       /* We cheat and use nnmgrp for this. */
               mexgrp = copygrp((PETD *)nnmgrp,
				u = parsegrp((PETD *)nnmgrp, &pcbgrnm, tbuf));
               ++ds.pmexgcnt; ds.pmexcnt += u-1;
               TRACEGRP(mexgrp);
               parsemd(tbuf, NAMECASE, &pcblitp, NAMELEN);
          default:
               break;
          }
          /* PARAMETER 3B: Plus exceptions.
          */
          TRACEMD("3B: +grp");
          switch (pcbmd.action) {
          case PGRP:
               pexgrp = copygrp((PETD *)nnmgrp,
				u = parsegrp((PETD *)nnmgrp, &pcbgrnm, tbuf));
               ++ds.pmexgcnt; ds.pmexcnt += u-1;
               TRACEGRP(pexgrp);
               parsemd(tbuf, NAMECASE, &pcblitp, NAMELEN);
          default:
               break;
          }
     }
     /* PARAMETER 4: End of declaration.
     */
     TRACEMD(emd);
     if (pcbmd.action!=EMD) mderr(126, (UNCH *)0, (UNCH *)0);
     if (es!=mdessv) synerr(37, &pcbmd);

     /* EXECUTE: Store the definition for each element name specified.
     */
     TRACEGRP(nmgrp);
     for (i = -1; nmgrp[++i];) {
          etdgi = nmgrp[i]->etdgi;
          if (*ranksuff) {
               if ((tbuf[0] = *etdgi + ustrlen(ranksuff)) - 2 > NAMELEN) {
                    mderr(131, etdgi+1, ranksuff);
                    continue;
               }
               memcpy(tbuf+1, etdgi+1, *etdgi-1);
               ustrcpy(tbuf+*etdgi-1, ranksuff);
               etdcan(etdgi);
               nmgrp[i] = etddef(tbuf);
          }
          if (nmgrp[i]->etdmod) {mderr(56, etdgi+1, (UNCH *)0); continue;}
          etdset(nmgrp[i], fmin+ETDDCL, cmod, mexgrp, pexgrp, nmgrp[i]->etdsrm);
          ++ds.etdcnt;
          if (nmgrp[i]->adl) etdadl(nmgrp[i]); /* Check ETD conflicts. */
          TRACEETD(nmgrp[i]);
     }
}

VOID adlfree(al, aln)
struct ad *al;
int aln;
{
     for (; aln <= ADN(al); aln++) {
	  frem((UNIV)al[aln].adname);
	  if (ADVAL(al, aln))
	       frem((UNIV)ADVAL(al, aln));
	  if (BITON(ADFLAGS(al, aln), AGROUP)) {
	       int i;
	       for (i = 0; i < ADNUM(al, aln); i++)
		    frem((UNIV)al[aln + i + 1].adname);
	       aln += ADNUM(al, aln);
	  }
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
