#include "sgmlincl.h"         /* #INCLUDE statements for SGML parser. */
/* MDENTITY: Process ENTITY declaration.
*/
VOID mdentity(tbuf)
UNCH *tbuf;                   /* Work area for tokenization[LITLEN+2]. */
{
     struct fpi fpicb;        /* Formal public identifier structure. */
     struct fpi *fpis = &fpicb;  /* Ptr to current or #DEFAULT fpi. */
     union etext etx;         /* Ptr to entity text. */
     UNCH estore = ESM;       /* Entity storage class. */
     struct entity *ecb;      /* Ptr to entity control block. */
     int parmsw = 0;          /* 1=parameter entity declaration; 0 = not. */
     int defltsw = 0;         /* 1=#DEFAULT declaration; 0=not. */
     PNE pne = 0;             /* Ptr to N/C/SDATA entity control block. */

     mdname = key[KENTITY];  /* Declaration name for messages. */
     subdcl = NULL;           /* No subject as yet. */
     parmno = 0;              /* No parameters as yet. */
     mdessv = es;             /* Save es for checking entity nesting. */
     /* PARAMETER 1: Entity name.
     */
     pcbmd.newstate = 0;
     parsemd(nmbuf, ENTCASE, &pcblitp, NAMELEN);
     TRACEMD("1: entity nm");
     switch (pcbmd.action) {
     case PEN:
          parsemd(nmbuf + 1, ENTCASE, &pcblitp, NAMELEN);
          if (pcbmd.action!=NAS) {mderr(120, (UNCH *)0, (UNCH *)0); return;}
	  if (nmbuf[1] == NAMELEN + 2) {
	       /* It was too long. */
	       nmbuf[0] = NAMELEN + 2;
	       nmbuf[NAMELEN + 1] = '\0';
	       mderr(65, (UNCH *)0, (UNCH *)0);
	  }
	  else
	       nmbuf[0] = nmbuf[1] + 1;	/* Increment length for PERO. */
          nmbuf[1] = lex.d.pero;        /* Prefix PERO to name. */
          parmsw = 1;                   /* Indicate parameter entity. */
     case NAS:
          break;
     case RNS:           /* Reserved name started. */
          if (ustrcmp(nmbuf+1, key[KDEFAULT])) {
               mderr(118, nmbuf+1, key[KDEFAULT]);
               return;
          }
          memcpy(nmbuf, indefent, *indefent);/* Copy #DEFAULT to name buffer. */
          fpis = &fpidf;                /* Use #DEFAULT fpi if external. */
          defltsw = 1;                  /* Indicate #DEFAULT is being defined.*/
          break;
     default:
          mderr(122, (UNCH *)0, (UNCH *)0);
          return;
     }
     subdcl = nmbuf+1;                  /* Subject name for error messages. */
     /* PARAMETER 2: Entity text keyword (optional).
     */
     parsemd(tbuf, NAMECASE, &pcblitp, LITLEN);
     TRACEMD("2: keyword");
     switch (pcbmd.action) {
     case NAS:
          if ((estore = (UNCH)mapsrch(enttab, tbuf+1))==0) {
	       estore = parmsw ? ESP : ESF;
               pne = (PNE)rmalloc(NESZ);
               if (mdextid(tbuf, fpis, nmbuf+1+parmsw, &estore, pne)==0)
		    return;
               if (defltsw) etx.x = NULL;
               else if ((etx.x = entgen(&fpicb))==0) {
		    if (parmsw)
		         mderr(148, nmbuf+2, (UNCH *)0);
		    else
		         mderr(147, nmbuf+1, (UNCH *)0);
	       }
               goto parm4;
          }
          if (parmsw && (estore==ESX || estore==ESC)) {
               mderr(38, tbuf+1, (UNCH *)0);
               estore = ESM;
          }
          parsemd(tbuf, NAMECASE, &pcblitp, LITLEN);
          break;
     default:
          estore = ESM;
          break;
     }
     /* PARAMETER 3: Parameter literal.
     */
     TRACEMD("3: literal");
     switch (pcbmd.action) {
     case LITE:
     case LIT:
          switch (estore) {
          case ESM:           /* LITERAL: parameter literal required. */
          case ESC:           /* CDATA: parameter literal required. */
          case ESX:           /* SDATA: parameter literal required. */
          case ESI:           /* PI: parameter literal required. */
               etx.c = savestr(tbuf);
               break;
          case ESMD:          /* MD: parameter literal required. */
               etx.c = sandwich(tbuf, lex.m.mdo, lex.m.mdc); 
	       goto bcheck;
          case ESMS:          /* MS: parameter literal required. */
               etx.c = sandwich(tbuf, lex.m.mss, lex.m.mse);
	       goto bcheck;
          case ESS:           /* STARTTAG: parameter literal required. */
	       etx.c = sandwich(tbuf, lex.m.stag, lex.m.tagc);
	       goto bcheck;
          case ESE:           /* ENDTAG: parameter literal required. */
	       etx.c = sandwich(tbuf, lex.m.etag, lex.m.tagc);
	  bcheck:
	       if (etx.c == 0) {
		    mderr(225, (UNCH *)0, (UNCH *)0);
		    return;
	       }
               break;
          }
          break;
     default:
          mderr(123, (UNCH *)0, (UNCH *)0);
          return;
     }
     /* PARAMETER 4: End of declaration.
     */
     parsemd(tbuf, NAMECASE, &pcblitp, LITLEN);
     parm4:
     TRACEMD(emd);
     if (pcbmd.action!=EMD) mderr(126, (UNCH *)0, (UNCH *)0);
     if (es!=mdessv) synerr(37, &pcbmd);

     /* EXECUTE: If the entity already exists, ignore the new definition.
                 If it is a new entity, store the definition.
     */
     if ((ecb = entfind(nmbuf))!=0 && ecb->estore) {
	  if (ecb->dflt) {
	       mderr(228, nmbuf + 1, (UNCH *)0);
	       hout((THASH)etab, nmbuf, hash(nmbuf, ENTHASH));
	       if (ecb->estore == ESN) {
		    frem((UNIV)NEID(ecb->etx.n));
		    frem((UNIV)ecb->etx.n);
	       }
	       else if (ecb->estore >= ESFM)
		    frem((UNIV)ecb->etx.x);
	       frem((UNIV)ecb);
	  }
	  else {
	       /* Duplicate definition: not an error. */
	       if (sw.swdupent) mderr(68, nmbuf+1, (UNCH *)0);
	       if (estore<ESFM) frem((UNIV)etx.c);
	       return;
	  }
     }
     ++ds.ecbcnt;                       /* Do capacity before NOTATION. */
     ds.ecbtext += estore<ESFM ? ustrlen(etx.c) : entlen;
     ecb = entdef(nmbuf, estore, &etx); /* Define the entity. */
     if (estore==ESN) {                 /* If entity is external: */
          NEENAME(pne) = ecb->ename;    /* Store entity name in ne. */
          NEID(pne) = etx.x;            /* Store system fileid in ne. */
	  NESYSID(pne) = fpis->fpisysis ? savestr(fpis->fpisysis) : 0;
	  NEPUBID(pne) = fpis->fpipubis ? savestr(fpis->fpipubis) : 0;
          ecb->etx.n = pne;             /* Store ne control block in etx. */
          TRACEESN(pne);
     }
     else if (pne)
	  frem((UNIV)pne);
     if (defltsw) {
	  ecbdeflt = ecb;     /* If #DEFAULT save ecb. */
	  if (fpidf.fpipubis)
	       fpidf.fpipubis = savestr(fpidf.fpipubis);
	  if (fpidf.fpisysis)
	       fpidf.fpisysis = savestr(fpidf.fpisysis);
     }
}
/* SANDWICH: Catenate a prefix and suffix to a string.
   The result has an EOS but no length.
   Return 0 if the result if longer than LITLEN.
*/
UNCH *sandwich(s, pref, suff)
UNCH *s;                      /* String, with EOS. */
UNCH *pref;                   /* Prefix, with length and EOS. */
UNCH *suff;                   /* Suffix, with length and EOS. */
{
     UNCH *pt;
     UNS slen, tlen;

     slen = ustrlen(s);
     tlen = slen + (*pref - 2) + (*suff - 2);
     if (tlen > LITLEN)
	  return 0;
     pt = (UNCH *)rmalloc(tlen + 1);
     memcpy(pt, pref + 1, *pref - 2);
     memcpy(pt + (*pref - 2), s, slen);
     memcpy(pt + (*pref - 2) + slen, suff + 1, *suff - 1);
     return pt;
}
/* MDEXTID: Process external identifier parameter of a markup declaration.
            On entry, tbuf contains SYSTEM or PUBLIC if all is well.
            NULL is returned if an error, otherwise fpis.  If it is a
            valid external data entity, the caller's estore is set to ESN
            and its nxetype is set to the code for the external entity type.
            The event that terminated the parse is preserved in pcb.action,
            so the caller should process it before further parsing.
*/
struct fpi *mdextid(tbuf, fpis, ename, estore, pne)
UNCH *tbuf;                   /* Work area for tokenization[2*(LITLEN+2)]. */
struct fpi *fpis;             /* FPI structure. */
UNCH *ename;                  /* Entity or notation name, with EOS, no length.*/
                              /* NOTE: No PERO on parameter entity name. */
UNCH *estore;                 /* DTD, general or parameter entity, DCN. */
PNE pne;                      /* Caller's external entity ptr. */
{
     PDCB dcb;                /* Ptr to DCN control block. */
     int exidtype;            /* External ID type: 0=none 1=system 2=public. */
     int exetype;             /* External entity type. */

     MEMZERO((UNIV)fpis, (UNS)FPISZ);     /* Initialize fpi structure. */
     /* Move entity name into fpi (any PERO was stripped by caller). */
     fpis->fpinm = ename;
     entlen = 0;              /* Initialize external ID length. */

     /* PARAMETER 1: External identifier keyword or error.
     */
     TRACEMD("1: extid keyword");
     if ((exidtype = mapsrch(exttab, tbuf+1))==0) {
          mderr(29, (UNCH *)0, (UNCH *)0);
          return (struct fpi *)0;
     }
     if (exidtype==EDSYSTEM) goto parm3;

     /* PARAMETER 2: Public ID literal.
     */
     /* The length of a minimum literal cannot exceed the value of LITLEN
	in the reference quantity set. */
     parsemd(pubibuf, NAMECASE, &pcblitv, REFLITLEN);
     TRACEMD("2: pub ID literal");
     switch (pcbmd.action) {
     case LITE:               /* Use alternative literal delimiter. */
     case LIT:                /* Save literal as public ID string. */
	  entlen = ustrlen(pubibuf);
	  fpis->fpipubis = pubibuf;
          break;
     default:
          mderr(117, (UNCH *)0, (UNCH *)0);
          return (struct fpi *)0;        /* Signal error to caller. */
     }
     /* PARAMETER 3: System ID literal.
     */
     parm3:
     parsemd(sysibuf, NAMECASE, &pcblitc, LITLEN);
     TRACEMD("3: sys ID literal");
     if (pcbmd.action==LIT || pcbmd.action==LITE) {
          entlen += ustrlen(sysibuf);
	  fpis->fpisysis = sysibuf;
          parsemd(tbuf, NAMECASE, &pcblitp, LITLEN);
     }
     else memcpy(tbuf, sysibuf, *sysibuf);
     if (*estore!=ESF || pcbmd.action!=NAS) goto genfpi;

     /* PARAMETER 4: Entity type keyword.
     */
     TRACEMD("4: Entity type");
     if ((exetype = mapsrch(extettab, tbuf+1))==0) {
          mderr(24, tbuf+1, (UNCH *)0);
          return (struct fpi *)0;
     }
     if (exetype==ESNSUB && SUBDOC == NO) {
	  mderr(90, tbuf+1, (UNCH *)0);
	  return (struct fpi *)0;
     }

     NEXTYPE(pne) = (UNCH)exetype; /* Save entity type in caller's ne. */
     *estore = ESN;                /* Signal that entity is a data entity. */

     if (exetype==ESNSUB) {
          pne->nedcn = 0;
	  parsemd(tbuf, NAMECASE, &pcblitp, LITLEN);
	  goto genfpi;
     }
     /* PARAMETER 5: Notation name.
     */
     parsemd(lbuf, NAMECASE, &pcblitp, NAMELEN);
     TRACEMD("5: notation");
     if (pcbmd.action!=NAS) {mderr(119, tbuf+1, (UNCH *)0); return (struct fpi *)0;}
     /* Locate the data content notation. */
     pne->nedcn = dcb = dcndef(lbuf);
     /* Note that we have defined an entity with this notation.
	If attributes are later defined for this notation, we'll
	have to fix up this entity. */
     dcb->entsw = 1;

     /* PARAMETER 6: Data attribute specification.
     */
     parsemd(lbuf, NAMECASE, &pcblitp, NAMELEN);
     TRACEMD("6: [att list]");
     if (pcbmd.action!=MDS) {     /* No attributes specified. */
	  if (dcb->adl == 0)
	       NEAL(pne) = 0;
	  else {
	       initatt(dcb->adl);
	       adlval((int)ADN(al), (struct etd *)0);
	       storedatt(pne);
	  }
          goto genfpi;
     }
     if (dcb->adl==0) {            /* Atts specified, but none defined. */
          mderr(22, (UNCH *)0, (UNCH *)0);
          return (struct fpi *)0;
     }
     pcbstag.newstate = pcbstan;   /* First separator is optional. */
     if ((parseatt(dcb->adl, tbuf))==0)/* Empty list. */
          mderr(91, (UNCH *)0, (UNCH *)0);
     else {
	  adlval((int)ADN(al), (struct etd *)0);
	  storedatt(pne);
     }
     parse(&pcbeal);               /* Parse the list ending. */
     parsemd(tbuf, NAMECASE, &pcblitp, LITLEN);

     /* GENFPI: Builds a formal public identifier structure, including the
                entity name, offsets of the components of the public ID, and
                other data a system might use to identify the actual file.
     */
 genfpi:
     TRACEMD("7: generate fpi");
     fpis->fpistore = *estore - ESFM + 1;    /* External entity type: 1-6. */
     if (*estore == ESN) {
          if (NEXTYPE(pne) == ESNSUB)
	       fpis->fpinedcn = 0;
	  else
	       fpis->fpinedcn = NEDCN(pne) + 1;
     }
     /* Analyze public ID and make structure entries. */
     if (exidtype==EDPUBLIC) {
	  if (parsefpi(fpis)>0) {
	       if (FORMAL==YES)
		    mderr(88, fpis->fpipubis, (UNCH *)0);
               fpis->fpiversw = -1; /* Signal bad formal public ID. */
	  }
     }
     return fpis;
}

/* Store a data attribute. */

VOID storedatt(pne)
PNE pne;
{
     int i;
     
     NEAL(pne) = (struct ad *)rmalloc((1+ADN(al))*ADSZ);
     memcpy((UNIV)NEAL(pne), (UNIV)al, (1+ADN(al))*ADSZ);
     for (i = 1; i <= (int)ADN(al); i++) {
	  if (GET(ADFLAGS(al, i), ASPEC))
	       ds.attdef += ADLEN(al, i);
	  if (NEAL(pne)[i].addef != 0)
	       NEAL(pne)[i].addef = savestr(NEAL(pne)[i].addef);
     }
     ds.attcnt += AN(al);     /* Number of attributes defined. */
#if 0
     /* I can't see any reason to increase AVGRPCNT here. */
     ds.attgcnt += ADN(al) - AN(al); /* Number of att grp members. */
#endif
}

/* PARSEFPI: Parses a formal public identifier and builds a control block.
             PARSEFPI returns a positive error code (1-10), or 0 if no errors.
             It set fpiversw if no version was specified in the ID and the
             public text is in a class that permits display versions.
             Note: An empty version ("//") can be specified (usually it is
             the non-device-specific form, such as a definitional entity set).
*/
int parsefpi(f)
PFPI f;                       /* Ptr to formal public identifier structure. */
{
     UNCH *l;                 /* Pointer to EOS of public identifier. */
     UNCH *p, *q;             /* Ptrs to current field in public identifier. */
     UNS len;		      /* Field length */

     p = f->fpipubis;                   /* Point to start of identifier. */
     l = p + ustrlen(p);		/* Point to EOS of identifier. */
     if ((*p=='+' || *p=='-')
	 && p[1] == '/' && p[2] == '/') { /* If owner registered,
					     unregistered. */
          f->fpiot = *p;                /* Save owner type. */
	  p += 3;
     }
     else f->fpiot = '!';               /* Indicate ISO owner identifier. */
     if ((q = pubfield(p, l, '/', &len))==0)  /* Find end of owner ID field. */
          return 2;
     f->fpiol = len;		        /* Save owner ID length. */
     f->fpio = p - f->fpipubis;		/* Save offset in pubis to owner ID. */

     if ((p = pubfield(q, l, ' ', &len))==0)  /* Find end of text class field. */
          return 3;
     *(--p) = EOS;                      /* Temporarily make class a string. */
     f->fpic = mapsrch(pubcltab, q);    /* Check for valid uc class name.*/
     *p++ = ' ';                        /* Restore the SPACE delimiter. */
     if (f->fpic==0) return 4;          /* Error if not valid uc class name.*/

     /* The public text class in a notation identifier must be NOTATION. */
     if (f->fpistore == ESK - ESFM + 1 && f->fpic != FPINOT) return 10;

     if (*p=='-' && p[1] == '/' && p[2] == '/') { /* If text is unavailable
						     public text.*/
          f->fpitt = *p;                /* Save text type. */
	  p += 3;
     }
     else f->fpitt = '+';               /* Indicate available public text. */
     if ((q = pubfield(p, l, '/', &len))==0)  /* Find end of text description. */
          return 6;
     f->fpitl = len;		        /* Save text description length. */
     f->fpit = p - f->fpipubis;         /* Save ptr to description.*/

     p = pubfield(q, l, '/', &len);     /* Bound language field. */
     if (f->fpic != FPICHARS) {
          int i;
          /* Language must be all upper-case letters. */
          /* The standard only says that it *should* be two letters, so
	     don't enforce that. */
	  /* Language must be a name, which means it can't be empty. */
	  if (len == 0)
	       return 7;
          for (i = 0; i < len; i++) {
	      /* Don't assume ASCII. */  
	       if (!strchr("ABCDEFGHIJKLMNOPQRSTUVWXYZ", q[i]))
	            return 7;
	  }
     }
     f->fpill = len;
     f->fpil = q - f->fpipubis;
     if (p!=0) {                        /* If there is a version field: */
          if (f->fpic<FPICMINV)         /* Error if class prohibits versions. */
               return 8;
          if ((pubfield(p, l, '/', &len))!=0) /* Bound version field. */
               return 9;                /* Error if yet another field. */
          f->fpivl = len;		/* Save version length. */
          f->fpiv = p - f->fpipubis;	/* Save ptr (in pubis) to version. */
     }
     else if (f->fpic>=FPICMINV) f->fpiversw = 1;/* No version: get the best. */
     return(0);
}
/* PUBFIELD: Returns ptr to next field, or NULL if ID has ended.
*/
#ifdef USE_PROTOTYPES
UNCH *pubfield(UNCH *p, UNCH *l, UNCH d, UNS *lenp)
#else
UNCH *pubfield(p, l, d, lenp)
UNCH *p;                      /* Public identifier field (no length or EOS). */
UNCH *l;                      /* Pointer to EOS of public identifier. */
UNCH d;                       /* Field delimiter: ' ' or '/'. */
UNS *lenp;		      /* Gets field length */
#endif
{
     UNCH *psv = p+1;         /* Save starting value of p. */

     while (p<l) {
          if (*p++==d) {              /* Test for delimiter character. */
               *lenp = p - psv;	      /* Save field length (no len or EOS). */
               if (d=='/' && *p++!=d) /* Solidus requires a second one. */
                    continue;
               return(p);             /* Return ptr to next field. */
          }
     }
     *lenp = p - --psv;      /* Save field length (no len or EOS). */
     return NULL;
}
/* MDMS: Process marked section start.
         If already in special parse, bump the level counters and return
         without parsing the declaration.
*/
struct parse *mdms(tbuf, pcb)
UNCH *tbuf;                   /* Work area for tokenization [NAMELEN+2]. */
struct parse *pcb;            /* Parse control block for this parse. */
{
     int key;                 /* Index of keyword in mslist. */
     int ptype;               /* Parameter token type. */
     int pcbcode = 0;         /* Parse code: 0=same; 2-4 per defines. */

     if (++mslevel>TAGLVL) {
          --mslevel;
          sgmlerr(27, (struct parse *)0, ntoa(TAGLVL), (UNCH *)0);
     }

     /* If already in IGNORE mode, return without parsing parameters. */
     if (msplevel) {++msplevel; return(pcb);}

     parmno = 0;                   /* No parameters as yet. */
     mdessv = es;                  /* Save es for checking entity nesting. */
     pcbmd.newstate = pcbmdtk;     /* First separator is optional. */

     /* PARAMETERS: TEMP, RCDATA, CDATA, IGNORE, INCLUDE, or MDS. */
     while ((ptype = parsemd(tbuf, NAMECASE, &pcblitp, NAMELEN))==NAS){
          if ((key = mapsrch(mstab, tbuf+1))==0) {
               sgmlerr(64, (struct parse *)0, ntoa(parmno), tbuf+1);
               continue;
          }
          if (key==MSTEMP) continue;       /* TEMP: for documentation. */
          msplevel = 1;                    /* Special parse required. */
          if (key>pcbcode) pcbcode = key;  /* Update if higher priority. */
     }
     if (ptype!=MDS) {
          NEWCC;                           /* Syntax error did REPEATCC. */
          sgmlerr(97, (struct parse *)0, lex.m.dso, (UNCH *)0);
          REPEATCC;                        /* 1st char of marked section. */
     }
     if (es!=mdessv) synerr(37, pcb);
     TRACEMS(1, pcbcode, mslevel, msplevel);
     if (pcbcode==MSIGNORE) pcb = &pcbmsi;
     else if (pcbcode) {
          pcb = pcbcode==MSCDATA  ? &pcbmsc : (rcessv = es, &pcbmsrc);
     }
     return(pcb);              /* Tell caller whether to change the parse. */
}
/* MDMSE: Process marked section end.
          Issue an error if no marked section had started.
*/
int mdmse()
{
     int retcode = 0;         /* Return code: 0=same parse; 1=cancel special. */

     if (mslevel) --mslevel;
     else sgmlerr(26, (struct parse *)0, (UNCH *)0, (UNCH *)0);

     if (msplevel) if (--msplevel==0) retcode = 1;
     TRACEMS(0, retcode, mslevel, msplevel);
     return retcode;
}
/* MDNOT: Process NOTATION declaration.
*/
VOID mdnot(tbuf)
UNCH *tbuf;                   /* Work area for tokenization[LITLEN+2]. */
{
     struct fpi fpicb;        /* Formal public identifier structure. */
     PDCB dcb;                /* Ptr to notation entity in dcntab. */
     UNCH estore = ESK;       /* Entity storage class. */

     mdname = key[KNOTATION]; /* Identify declaration for messages. */
     subdcl = NULL;           /* No subject as yet. */
     parmno = 0;              /* No parameters as yet. */
     mdessv = es;             /* Save es for checking entity nesting. */

     /* PARAMETER 1: Notation name.
     */
     pcbmd.newstate = 0;
     parsemd(lbuf, NAMECASE, &pcblitp, NAMELEN);
     TRACEMD("1: name");
     if (pcbmd.action!=NAS) {mderr(120, (UNCH *)0, (UNCH *)0); return;}
     subdcl = lbuf+1;         /* Save notation name for error msgs. */

     /* PARAMETER 2: External identifier keyword.
     */
     parsemd(tbuf, NAMECASE, &pcblitp, NAMELEN);
     TRACEMD("2: extid");
     if (pcbmd.action!=NAS) {mderr(29, (UNCH *)0, (UNCH *)0); return;}
     if (mdextid(tbuf, &fpicb, lbuf+1, &estore, (PNE)0)==0) return;

     /* PARAMETER 3: End of declaration.
                     Token was parsed by MDEXTID.
     */
     TRACEMD(emd);
     if (pcbmd.action!=EMD) mderr(126, (UNCH *)0, (UNCH *)0);
     if (es!=mdessv) synerr(37, &pcbmd);

     /* EXECUTE: Store notation name.
     */
     if ((dcb = dcnfind(lbuf)) != 0 && dcb->defined) {
          mderr(56, lbuf+1, (UNCH *)0);
	  return;
     }
     /* else */
     dcb = dcndef(lbuf);
     dcb->defined = 1;
     dcb->sysid = fpicb.fpisysis ? savestr(fpicb.fpisysis) : 0;
     dcb->pubid = fpicb.fpipubis ? savestr(fpicb.fpipubis) : 0;
     ++ds.dcncnt;
     ds.dcntext += entlen;
     TRACEDCN(dcb);
     return;
}
/* DCNDEF: Define a notation and return its DCNCB.
           If caller does not care if it already exists,
           he should specify NULL for the notation text
           so we don't clobber the existing text (if any).
*/
struct dcncb *dcndef(nname)
UNCH *nname;                  /* Notation name (with length and EOS). */
{
     return((PDCB)hin((THASH)dcntab, nname, 0, DCBSZ));
}
/* DCNFIND: If a notation was declared, return its DCNCB.
            Return NULL if it is not defined.
*/
struct dcncb *dcnfind(nname)
UNCH *nname;                  /* Notation name (with length and EOS). */
{
     return((PDCB)hfind((THASH)dcntab, nname, 0));
}
#define SRM(i) (srhptr->srhsrm[i]) /* Current entry in SHORTREF map. */
/* MDSRMDEF: Process short reference mapping declaration.
*/
VOID mdsrmdef(tbuf)
UNCH *tbuf;                   /* Work area for tokenization[LITLEN+2]. */
{
     struct entity *entcb;    /* Ptr to defined entity. */
     PSRH srhptr;             /* Ptr to short reference map hdr (in srhtab).*/
     int srn;                 /* Short reference delimiter number in srdeltab.*/
     int mapused = 0;	      /* Has map already been used? */

     mdname = key[KSHORTREF]; /* Identify declaration for messages. */
     subdcl = NULL;           /* No subject as yet. */
     parmno = 0;              /* No parameters as yet. */
     if (!sd.shortref) {mderr(198, (UNCH *)0, (UNCH *)0); return;}
     mdessv = es;             /* Save es for checking entity nesting. */
     /* PARAMETER 1: SHORTREF map name.
     */
     pcbmd.newstate = 0;
     parsemd(tbuf, NAMECASE, &pcblitp, NAMELEN);
     TRACEMD("1: map nm");
     if (pcbmd.action!=NAS) {mderr(120, (UNCH *)0, (UNCH *)0); return;}
     if ((srhptr = srhfind(tbuf))!=0) {
	  mapused = 1;
	  /* Error if map was declared (not just used). */
	  if (SRM(0)) {mderr(56, tbuf+1, (UNCH *)0); return;}
     }
     else srhptr = srhdef(tbuf);  /* Create map with SRs mapped to NULL.*/
     SRM(0) = (PECB)srhptr;       /* Indicate map was actually declared.*/
     subdcl = srhptr->ename+1;    /* Save map name for error msgs. */

     while (parsemd(tbuf, NAMECASE, &pcblitp, SRMAXLEN) == LIT
	    || pcbmd.action==LITE ) {
          /* PARAMETER 2: Delimiter string.
          */
          TRACEMD("2: SR string");
          if ((srn = mapsrch(lex.s.dtb, tbuf))==0) {
               mderr(124, tbuf, (UNCH *)0);
               goto cleanup;
          }
          /* PARAMETER 3: Entity name.
          */
          parsemd(tbuf, ENTCASE, &pcblitp, NAMELEN);
          TRACEMD("3: entity");
          if (pcbmd.action!=NAS) {mderr(120, (UNCH *)0, (UNCH *)0); goto cleanup;}
          if ((entcb = entfind(tbuf))==0) {
	       union etext etx;
	       etx.x = 0;
               entcb = entdef(tbuf, '\0', &etx);
	  }
          if (SRM(srn)) {
               mderr(56, (srn<lex.s.prtmin ? (UNCH *)lex.s.pdtb[srn]
                                       : lex.s.dtb[srn].mapnm), (UNCH *)0);
               continue;
          }
          SRM(srn) = entcb;
          if (srn>=lex.s.fce && srn!=lex.s.hyp && srn!=lex.s.hyp2
	      && srn!=lex.s.lbr && srn!=lex.s.rbr)
               lexcnm[*lex.s.dtb[srn].mapnm] = lex.l.fce;
          else if (srn==lex.s.spc) lexcnm[' '] = lex.l.spcr;
     }
     /* PARAMETER 4: End of declaration.
     */
     TRACEMD(emd);
     if (parmno==2)
          {mderr((UNS)(pcbmd.action==EMD ? 28:123), (UNCH *)0, (UNCH *)0); goto cleanup;}
     if (pcbmd.action!=EMD) mderr(126, (UNCH *)0, (UNCH *)0);
     if (es!=mdessv) synerr(37, &pcbmd);
     ++ds.srcnt;
     TRACESRM("SHORTREF", srhptr->srhsrm, (UNCH *)0);
     return;

 cleanup:
     /* Don't free the map if the map was in use (because of a USEMAP
	declaration) before this declaration. */
     if (mapused)
	  MEMZERO((UNIV)srhptr->srhsrm, sizeof(PECB)*(lex.s.dtb[0].mapdata+1));
     else {
          frem((UNIV)srhptr->srhsrm);
          hout((THASH)srhtab, srhptr->ename, 0);
          frem((UNIV)srhptr);
     }
}
/* MDSRMUSE: Activate a short reference map.
*/
VOID mdsrmuse(tbuf)
UNCH *tbuf;                   /* Work area for tokenization[LITLEN+2]. */
{
     PSRH srhptr;             /* Ptr to short reference map hdr (in srhtab).*/
     TECB srmptr;             /* Ptr to short reference map (in header). */
     int i;                   /* Loop counter; temporary variable. */

     mdname = key[KUSEMAP];   /* Identify declaration for messages. */
     subdcl = NULL;           /* No subject as yet. */
     parmno = 0;              /* No parameters as yet. */
     mdessv = es;             /* Save es for checking entity nesting. */
     /* PARAMETER 1: SHORTREF map name or "#EMPTY".
     */
     pcbmd.newstate = 0;
     parsemd(lbuf, NAMECASE, &pcblitp, NAMELEN);
     TRACEMD("1: map nm");
     subdcl = lbuf+1;		/* Subject name for error messages. */
     switch (pcbmd.action) {
     case RNS:                /* Empty SHORTREF map requested. */
          if (ustrcmp(lbuf+1, key[KEMPTY])) {
               mderr(118, lbuf+1, key[KEMPTY]);
               return;
          }
          srmptr = SRMNULL;
          break;
     case NAS:                /* Map name specified; save if undefined. */
          if ((srhptr = srhfind(lbuf))==0) {
               if (!indtdsw) {mderr(125, (UNCH *)0, (UNCH *)0); return;}
	       srmptr = NULL;
          }
          else
	       srmptr = srhptr->srhsrm;
          break;
     default:
          mderr(120, (UNCH *)0, (UNCH *)0);
          return;
     }
     /* PARAMETER 2: Element name or a group of them. (In DTD only.)
     */
     parsemd(tbuf, NAMECASE, &pcblitp, NAMELEN);
     TRACEMD("2: GI or grp");
     switch (pcbmd.action) {
     case NAS:
	  if (!indtdsw) {mderr(142, (UNCH *)0, (UNCH *)0); return;}
	  nmgrp[0] = etddef(tbuf);
	  nmgrp[1] = (PETD)NULL;
	  break;
     case GRPS:
	  if (!indtdsw) {mderr(142, (UNCH *)0, (UNCH *)0); return;}
	  parsegrp(nmgrp, &pcbgrnm, tbuf);
	  break;
     case EMD:
	  if (indtdsw) {mderr(28, (UNCH *)0, (UNCH *)0); return;}
	  if (docelsw) {mderr(233, (UNCH *)0, (UNCH *)0); return;}
	  tags[ts].tsrm = srmptr;
	  TRACESRM("USEMAP", tags[ts].tsrm, tags[ts].tetd->etdgi+1);
	  goto realemd;
     default:
	  mderr(indtdsw ? 121 : 126, (UNCH *)0, (UNCH *)0);
	  return;
     }
     /* PARAMETER 3: End of declaration.
     */
     parsemd(tbuf, NAMECASE, &pcblitp, NAMELEN);
     TRACEMD(emd);
     if (pcbmd.action!=EMD) mderr(126, (UNCH *)0, (UNCH *)0);
     /* If map has not yet been defined, do it and get map pointer. */
     if (!srmptr) srmptr = (srhdef(lbuf))->srhsrm;

     /* Store the map pointer for each element name specified.
     */
     TRACEGRP(nmgrp);
     for (i = -1; nmgrp[++i];) {
          if (!nmgrp[i]->etdsrm) nmgrp[i]->etdsrm = srmptr;
          else if (sw.swdupent) mderr(68, nmgrp[i]->etdgi+1, (UNCH *)0);
     }
     realemd:
     if (es!=mdessv) synerr(37, &pcbmd);
}
/* SRHDEF: Define a SHORTREF map and return ptr to its header.
           All entries in map are mapped to NULL.
           Caller must determine whether it already exists.
*/
PSRH srhdef(sname)
UNCH *sname;                  /* SHORTREF map name (with length and EOS). */
{
     PSRH srh;                /* Ptr to SHORTREF map hdr in srhtab. */

     (srh = (PSRH)hin((THASH)srhtab, sname, 0, SRHSZ))->srhsrm =
          (TECB)rmalloc((UNS)(lex.s.dtb[0].mapdata+1)*sizeof(PECB));
     return(srh);
}
/* SRHFIND: If a SHORTREF map was declared, return the ptr to its header.
            Return NULL if it is not defined.
*/
PSRH srhfind(sname)
UNCH *sname;                  /* SHORTREF map name (with length and EOS). */
{
     return((PSRH)hfind((THASH)srhtab, sname, 0));
}
#undef SRM

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
