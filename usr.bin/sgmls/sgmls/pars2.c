#include "sgmlincl.h"         /* #INCLUDE statements for SGML parser. */
/* PARSE: Parse a source input stream with specified lexical and state tables.
          Return to caller with action code.
*/
int parse(pcb)
struct parse *pcb;            /* Current parse control block. */
{
     int rc;                  /* Return code from ENTREF. */

     while (1) {
          NEWCC;
          pcb->input = pcb->plex[*FPOS];
          pcb->state = pcb->newstate;
          pcb->newstate = (*(pcb->ptab + pcb->state)) [pcb->input];
          pcb->action = (*(pcb->ptab + pcb->state + 1)) [pcb->input];
          TRACEPCB(pcb);
          switch (pcb->action) {
          case RC2_:          /* Back up two characters. */
               REPEATCC;
          case RCC_:          /* Repeat current character. */
               REPEATCC;
          case NOP_:          /* No action necessary.*/
               continue;

          case RS_:           /* Record start: ccnt=0; ++rcnt.*/
               ++RCNT; CTRSET(RSCC);
               continue;

          case GET_:          /* EOB or dull EOS or EE found: keep going.*/
               if (entget()==-1) {pcb->action = EOD_; break;}/* Signal if EOD.*/
               continue;

          case EOF_:          /* Illegal entity end; return EE_. */
               synerr(E_EOF, pcb);
               pcb->action = EE_;
          case EE_:           /* Important EOS or EE found: return to caller.*/
               if (entget()==-1) pcb->action = EOD_;   /* Signal if EOD. */
               break;

          case PER_:          /* Parameter entity reference. */
               REPEATCC;           /* Use PERO as 1st char of entity name. */
               parsenm(entbuf, ENTCASE);
               parse(&pcbref);     /* Handle REFC or other terminator. */
               rc = entref(entbuf);
               if (rc==ENTPI) {pcb->action = PIE_; break;}
               continue;

          case ER_:           /* General entity reference; continue. */
               parsenm(entbuf, ENTCASE);
               parse(&pcbref);     /* Handle REFC or other terminator. */
	       rc = entref(entbuf);
               if (rc==ENTDATA) {pcb->action = DEF_; break;}
               if (rc==ENTPI) {pcb->action = PIE_; break;}
               continue;


          case PEX_:          /* Parameter entity reference; return. */
               REPEATCC;           /* Use PERO as 1st char of entity name. */
          case ERX_:          /* General entity reference; return. */
               parsenm(entbuf, ENTCASE);
               parse(&pcbref);     /* Handle REFC or other terminator. */
               rc = entref(entbuf);
               if (rc == ENTDATA){
		    /* Reference to external data/subdoc entity in replaceable
		       character data. */
		    if (BITON(entdatsw, NDECONT)) {
			 switch (((PNE)data)->nextype) {
			 case ESNCDATA:
			 case ESNSDATA:
			      /* The standard says `non-SGML data entity'
				 but the amendment should have changed it
				 to `external data entity'. */
			      synerr(145, pcb);
			      break;
			 case ESNNDATA:
			 case ESNSUB:
			      /* This is definitely illegal. */
			      synerr(141, pcb);
			      break;
			 }
			 entdatsw = 0;
			 continue;
		    }
		    pcb->action = DEF_;
	       }
               else if (rc == ENTPI) {
		    /* Reference to PI entity not allowed in replaceable
		       character data. */
		    synerr(59, pcb);
		    entpisw = 0;
		    continue;
	       }
               else if (rc) pcb->action = EE_;
               break;

          case CRN_:          /* Character reference: numeric. */
               parsetkn(entbuf, NU, NAMELEN);
               parse(&pcbref);     /* Handle reference terminator. */
               pcb->action = charrefn(entbuf, pcb);
               if (pcb->action==CRN_) continue;   /* Invalid reference */
               break;

          case CRA_:           /* Character reference: alphabetic. */
               parsenm(entbuf, NAMECASE);
               parse(&pcbref);     /* Handle reference terminator. */
               charrefa(entbuf);
               continue;

          case SYS_:          /* Invalid NONCHAR: send msg and ignore. */
               synerr(E_SYS, pcb);
	       if (*FPOS == DELNONCH) NEWCC;
               continue;

          case NON_:	      /* Valid NONCHAR: prefix and shift encoding. */
               synerr(60, pcb);
	       pcb->action = datachar(*FPOS, pcb);
               break;
	  case NSC_:
               synerr(60, pcb);
	       NEWCC;
	       nonchbuf[1] = *FPOS;
	       pcb->action = NON_;
	       break;
          case PCI_:          /* Previous character was invalid (INV_). */
               REPEATCC;
          case INV_:          /* Markup ended by invalid char; repeat char. */
               synerr(9, pcb);
               REPEATCC;
               break;

          case LNR_:          /* Previous char exceeded len; back up to it. */
               REPEATCC;
          case LEN_:          /* Token too long; ignore excess character. */
               synerr(3, pcb);
               continue;

          case RCR_:          /* Repeat current char and return to caller. */
               REPEATCC;
          default:            /* Actions for specific parse. */
               break;
          }
          return (int)pcb->action;
     }
}
/* CHARREFA: Resolve an alphabetical reference to a function character
             and put the character in the read buffer.
             If reference is bad, issue an error message.
*/
VOID charrefa(r)
UNCH *r;                      /* Undelimited char ref (with length and EOS). */
{
     UNCH thechar;

     thechar = mapsrch(funtab, r+1);
     if (thechar == 0)
	  synerr(62, &pcbref);
     else {
          /* This isn't ideal, because the character position will still
	     be wrong for one line. */
	  if (thechar == RSCHAR) RCNT--;
	  setcurchar(thechar);
          REPEATCC;
     }
}

/* Make the current character ch. */

VOID setcurchar(ch)
int ch;
{
     /* If we're reading directly from an internal entity, we can't
	change the entity, since the entity might be referenced again.
	So in this case we copy the entity.  This is inefficient, but
	it will only happen in a case like this:

	<!entity % amp "&">
	<!entity e "x%amp;#SPACE;">

	Usually character references will have been processed while the
	entity was being defined.  */
     if (*FPOS != ch) {
	  if (!FILESW && !COPIEDSW) {
	       UNCH *s = savestr(FBUF + 1);
	       FPOS = s + (FPOS - FBUF - 1);
	       FBUF = s - 1;
	       COPIEDSW = 1;
	  }
	  *FPOS = ch;
     }
}

/* CHARREFN: Resolve a numeric character reference.
             If reference is bad, issue an error message.
*/

int charrefn(r, pcb)
UNCH *r;                      /* Undelimited character reference. */
struct parse *pcb;            /* Current parse control block. */
{
     int thechar;

     thechar = atoi((char *)r);
     if (thechar<0 || thechar>255) {
          synerr(61, &pcbref);
          return((int)pcb->action);
     }
     return datachar(thechar, pcb);
}

/* Return ch as a datachar.  If this a non-SGML character which might
confuse the parser, shift it to a code that won't and place it in a
special buffer which has DELNONCH in the preceding byte.  Otherwise
put it the read buffer. */

int datachar(ch, pcb)
int ch;
struct parse *pcb;
{
     switch (ch) {
     case EOS:
     case EOFCHAR:
     case EOBCHAR:
     case GENRECHAR:
     case DELCDATA:
     case DELSDATA:
     case DELNONCH:
	  /* A potentially confusing character which must be prefixed
	     with DELNONCH. */
          nonchbuf[1] = SHIFTNON((UNCH)ch);
          return NON_;
     }
     setcurchar(ch);
     /* If in content, return DCE_ for element content, DAF_ for mixed.  */
     /* If not content, it must be a literal parse, so return MLA_. */
     if (pcb == conpcb) {
	  if (pcb == &pcbcone)
	       return DCE_;
	  else {
	       data = FPOS;
	       /* Action for DAF_ will do REPEATCC. */
	       NEWCC;
	       return DAF_;
	  }
     }
     else
	  return MLA_;
}
/* INITATT: Initialize al with adl. */

VOID initatt(adl)
struct ad *adl;
{
     notadn = 0;              /* No NOTATION attribute yet. */
     conrefsw = 0;            /* Assume no content reference att. */
     /* Copy attribute definition list as a template. */
     memcpy((UNIV)al, (UNIV)adl, (1+ADN(adl))*ADSZ);
}

/* PARSEATT: Parse attribute specification list.
             Make a current copy of the attribute definition list
             and update it with the user's specifications.
             Indicate each attribute that was specified in the
             list (as opposed to defaulted) by setting the ASPEC flag.
             If no attributes were specified, return NULL.  Otherwise,
             if in the prolog, make a permanent copy of the list and
             return its pointer.  If not in the prolog, return al.
*/
struct ad *parseatt(adl, pt)
struct ad *adl;               /* Attribute definition list. */
UNCH *pt;                     /* Tokenization area: tbuf[TAGLEN+ATTSPLEN]. */
{
     UNCH *antvptr;
     UNCH *nm = 0;            /* Pointer to saved name in tbuf (with length). */
     int adn = -1;            /* Position of attribute in list (-1=empty). */
     UNCH *tbuflim = pt + ATTSPLEN;
     mdessv = es;             /* Save es for checking entity nesting. */
     initatt(adl);
     while (pt<=tbuflim) {
          parse(&pcbstag);
          switch (pcbstag.action) {
          case NVS:                     /* Att name or value token found. */
               parsenm(pt, NAMECASE);   /* Case translation wanted on name. */
               pt += *(nm = pt);        /* Save name while pointing past it. */
               continue;

          case AVD:           /* Delimited value found. */
          case AVDA:          /* Delimited value found (alternate delimiter). */
               /* Find position (adn) of saved attribute name in list. */
               adn = anmget((int)ADN(al), nm);
               parselit(pt,
			(adn == 0 || ADTYPE(al, adn) == ACHARS)
			? &pcblitr
			: &pcblitt,
			LITLEN,
			(pcbstag.action==AVD) ? lex.d.lit : lex.d.lita);
	       if (adn == 0) {
                    /* Error: unrecognized attribute name. */
                    sgmlerr(13, &pcbstag, nm+1, pt);
                    continue;
               }
               /* Tokenize and validate value; let it default if an error. */
               /* Put value in list and bump ptr by the normalized length
                  (which is always >= the actual length). */
               if (!attval(1, pt, adn, adl)) pt += ADLEN(al,adn);
	       continue;
          case AVU:           /* Attribute value found: undelimited. */
	       if (!sd.shorttag) sgmlerr(196, &pcbstag, (UNCH *)0, (UNCH *)0);
	       parsetkn(pt, NMC, LITLEN);
               /* Find position (adn) of saved attribute name in list. */
               if ((adn = anmget((int)ADN(al), nm))==0) {
                    /* Error: unrecognized attribute name. */
                    sgmlerr(13, &pcbstag, nm+1, pt);
                    continue;
               }
               /* Tokenize and validate value; let it default if an error. */
               /* Put value in list and bump ptr by the normalized length
                  (which is always >= the actual length). */
               if (!attval(1, pt, adn, adl)) pt += ADLEN(al,adn);
               continue;

          case NASV:          /* Saved NVS was really an NTV. */
               REPEATCC;           /* Put back next token starter. */
               pt = nm;            /* Back up to NVS. */
          case NTV:           /* Name token value found. */
	       if (!sd.shorttag) sgmlerr(195, &pcbstag, (UNCH *)0, (UNCH *)0);
               if (pcbstag.action==NTV) parsenm(pt, NAMECASE);
               if ((adn = antvget((int)ADN(al), pt, &antvptr))==0) {
                    /* Error: unrecognized name token value. */
                    sgmlerr(74, &pcbstag, pt+1, (UNCH *)0);
                    continue;
               }
               /* Validate value; let it default if an error. */
               /* Put value in list and bump ptr by the normalized length
                  (which is always >= the actual length). */
               if (!attval(0, antvptr+1, adn, adl)) pt += ADLEN(al,adn);
               continue;

          default:            /* All attributes have been parsed. */
               REPEATCC;      /* Put next char back for tag close parse. */
               break;
          }
          break;
     }
     if (pt>tbuflim) synerr(75, &pcbstag);
     if (es!=mdessv) synerr(37, &pcbstag);
     if (adn<0) return((struct ad *)0); /* List was empty. */
     TRACEADL(al);
     return al;
}
/* ATTVAL: Validate a specified attribute value.  Issue a message if it is
           the wrong type (or otherwise is not up to spec), and use the default.
           Call PARSEVAL to tokenize the value, unless it is a CDATA string.
           If the attribute is a group, the value is a string.
           For other types, the token count is set by PARSEVAL if the value
           is syntactically correct.  If incorrect (or if CDATA) the token
           count is zero (i.e., the value is a string).
           The length of a token does not include the length byte, and
           there is no EOS.  A string length (as always) includes both
           the length byte and the EOS.
           If it is a CONREF attribute, set a switch for STAG().
           If it is a CURRENT attribute, store the value as the new default.
*/
#define DEFVAL adl[adn].addef /* Default value of current attribute. */
#define DEFNUM adl[adn].adnum /* Default group size of current attribute. */
#define DEFLEN adl[adn].adlen /* Length of default value of current attribute.*/
int attval(mtvsw, adval, adn, adl)
int mtvsw;                    /* Must tokenize value: 1=yes; 0=no. */
UNCH *adval;                  /* Untokenized attribute value. */
int adn;                      /* Attribute's position in list. */
struct ad *adl;               /* Element's master att def list. */
{
     int errcode;             /* Value/declaration conflict error code. */

     if (GET(ADFLAGS(al,adn), ASPEC))      /* Can't respecify same attribute. */
          {sgmlerr(73, &pcbstag, ADNAME(al,adn), adval); return(1);}
     SET(ADFLAGS(al,adn), ASPEC);          /* Indicate att was specified. */
     if (GET(ADFLAGS(al,adn), ACONREF))    /* If attribute is content reference: */
          conrefsw = TAGREF;            /* Set switch for STAG(). */
     if (mtvsw && ADTYPE(al,adn)!=ACHARS) {
          /* If no syntax errors, check for proper group membership. */
          if ( ((errcode = parseval(adval, ADTYPE(al,adn), lbuf))==0)
            && GET(ADFLAGS(al,adn), AGROUP)
            && !amemget(&al[adn], ADNUM(al,adn), lbuf) ) errcode = 18;
          /* If syntax or group membership error, send message and exit. */
          if (errcode) {
               sgmlerr(errcode, &pcbstag, ADNAME(al,adn), adval);
               SET(ADFLAGS(al,adn), AERROR);
               return(1);
          }
          /* Replace specified value in adval with tokenized in lbuf. */
	  ustrcpy(adval, lbuf);
          if (BITOFF(ADFLAGS(al,adn), AGROUP)) ADNUM(al,adn) = (UNCH)tokencnt;
     }
     if (!mtvsw)
	  adval--;
     /* If attribute is FIXED, specified value must equal default. */
     if (BITON(ADFLAGS(al,adn), AFIXED) && ustrcmp(adval, DEFVAL)) {
	  /* Since the value has been tokenized, don't use it in the
	     error message. */
          sgmlerr(67, &pcbstag, ADNAME(al,adn), (UNCH *)0);
          SET(ADFLAGS(al,adn), AERROR);
          return(1);
     }
     ADLEN(al,adn) = vallen(ADTYPE(al,adn), ADNUM(al,adn), adval);
     if (ADLEN(al,adn) > LITLEN) {
	  sgmlerr(224, &pcbstag, ADNAME(al,adn), (UNCH *)0);
	  SET(ADFLAGS(al,adn), AERROR);
	  return 1;
     }
     ADVAL(al,adn) = adval;
     /* If attribute is CURRENT, value is new default.*/
     if (GET(ADFLAGS(al,adn), ACURRENT)) {
          if (ADLEN(al,adn)>DEFLEN) {
               ds.attdef += (ADLEN(al,adn) - DEFLEN);
               DEFLEN = ADLEN(al,adn);
          }
          DEFVAL = replace(DEFVAL, ADVAL(al,adn));
          DEFNUM = ADNUM(al,adn);
     }
     return(0);                   /* Indicate value was valid. */
}
/* ADLVAL: Validate the completed attribute definition list (defaults plus
           specified values).  Issue a message if an
           attribute is required or current and its value is NULL.
*/
VOID adlval(adsz, newetd)
int adsz;                     /* Size of list. */
struct etd *newetd;           /* Element type definition for this element. */
{
     int adn = 1;             /* Position in list. */
     UNCH *npt, *pt;          /* Ptr save areas. */
     UNCH nptsv;              /* Save area for ptr value (length?). */
     struct dcncb *dpt;       /* Save area for dcncb ptr. */

     aentctr = 0;             /* Number of AENTITY tokens in this att list. */
     idrctr = 0;              /* Number of IDREF tokens in this att list. */
     do {
          if (ADVAL(al,adn)==NULL) {                      /* NULL value */
               if (GET(ADFLAGS(al,adn), AREQ+ACURRENT)) { /*Error if REQ, CURRENT*/
                    sgmlerr(19, &pcbstag, ADNAME(al,adn), (UNCH *)0);
                    SET(ADFLAGS(al,adn), AINVALID);
               }
          }
          else switch (ADTYPE(al,adn)) {
          case AENTITY:       /* Return data ecb pointer if valid entity. */
               aenttst(adn, ADVAL(al,adn));
               break;
          case AENTITYS:      /* Return data ecb pointers if valid entities. */
               pt = ADVAL(al,adn);
               tokencnt = (int)ADNUM(al,adn);
               while (tokencnt--) {
                    nptsv = *(npt = pt + *pt+1);
                    *pt += 2; *npt = EOS;
                    aenttst(adn, pt);
                    *pt -= 2; *(pt = npt) = nptsv;
               }
               break;
          case AID:
               /* Define ID; msg if it already exists. */
	       if (iddef(ADVAL(al,adn))) {
		    sgmlerr(71, &pcbstag, ADNAME(al,adn), ADVAL(al,adn)+1);
		    SET(ADFLAGS(al,adn), AINVALID);
		    continue;
	       }
	       ++ds.idcnt;
               break;
          case AIDREF:
               idreftst(adn, ADVAL(al,adn));
               break;
          case AIDREFS:
               pt = ADVAL(al,adn);
               tokencnt = (int)ADNUM(al,adn);
               while (tokencnt--) {
                    nptsv = *(npt = pt + *pt+1);
                    *pt += 2; *npt = EOS;
                    idreftst(adn, pt);
                    *pt -= 2; *(pt = npt) = nptsv;
               }
               break;
          case ANOTEGRP:      /* Return notation identifier. */
               if (GET(ADFLAGS(al,adn), ASPEC)) notadn = adn;/*NOTATION specified*/
               if ((dpt = dcnfind(ADVAL(al,adn)))==0) {
                    sgmlerr(77, &pcbstag, ADNAME(al,adn), ADVAL(al,adn)+1);
                    SET(ADFLAGS(al,adn), AINVALID);
               }
               else ADDATA(al,adn).x = dpt;
               break;
          }
	  if (!sd.shorttag && !sd.omittag && ADVAL(al,adn)!=NULL
	      && !GET(ADFLAGS(al,adn), ASPEC+AINVALID))
	       sgmlerr(197, &pcbstag, ADNAME(al,adn), (UNCH *)0);
     } while ((adn+=BITON(ADFLAGS(al,adn),AGROUP) ? (int)ADNUM(al,adn)+1 : 1)<=adsz);

     /* Error if NOTATION specified with CONREF attribute or EMPTY element. */
     if (notadn && (conrefsw
		    || (newetd && GET(newetd->etdmod->ttype, MNONE)))) {
          sgmlerr((UNS)(conrefsw ? 84 : 76), &pcbstag,
               ADNAME(al,notadn), ADVAL(al,notadn)+1);
          SET(ADFLAGS(al,notadn), AINVALID);
     }
}
/* AENTTST: Validate an individual ENTITY token in AENTITY or AENTITYS value.
*/
VOID aenttst(adn, pt)
int adn;                      /* Position in list. */
UNCH *pt;                     /* Ptr to current ENTITY token in value. */
{
     struct entity *ept;      /* Save area for ecb ptr. */

     if (++aentctr>GRPCNT) {
          sgmlerr(136, &pcbstag, ADNAME(al,adn), pt+1);
          SET(ADFLAGS(al,adn), AINVALID);
          return;
     }
     if ( (ept = entfind(pt))==0
       && (ecbdeflt==0 || (ept = usedef(pt))==0) ) {
          sgmlerr(ecbdeflt ? 151 : 72, &pcbstag, ADNAME(al,adn), pt+1);
          SET(ADFLAGS(al,adn), AINVALID);
          return;
     }
     if (ept->estore==ESX || ept->estore==ESC || ept->estore==ESN) {
          /* Error if DCN has no notation identifier. */
          if (ept->estore==ESN && NEXTYPE(ept->etx.n)!=ESNSUB
	      && !NEDCNDEFINED(ept->etx.n)) {
               sgmlerr(78, &pcbstag, NEDCN(ept->etx.n)+1,
                           pt+1);
               SET(ADFLAGS(al,adn), AINVALID);
          }
     }
     else {
          sgmlerr(86, &pcbstag, ADNAME(al,adn), pt+1);
          SET(ADFLAGS(al,adn), AINVALID);
     }
}
/* IDREFTST: Validate an individual IDREF token in an IDREF or IDREFS value.
*/
VOID idreftst(adn, pt)
int adn;                      /* Position in list. */
UNCH *pt;                     /* Ptr to current IDREF token in value. */
{
     struct fwdref *rp;
     if (++idrctr>GRPCNT) {
          sgmlerr(70, &pcbstag, ADNAME(al,adn), pt+1);
          SET(ADFLAGS(al,adn), AINVALID);
          return;
     }
     /* Note IDREF; indicate if ID exists. */
     if ((rp = idref(pt)) != 0)
	  rp->msg = saverr(69, &pcbstag, ADNAME(al,adn), pt+1);
     ++ds.idrcnt;
}
/* ANMGET: Locate an attribute name in an attribute definition list.
*/
int anmget(adsz, nm)
int adsz;                     /* Size of list. */
UNCH *nm;                     /* Value to be found (with length byte). */
{
     int adn = 0;             /* Position in list. */

     while (++adn <= adsz && ustrcmp(nm+1, ADNAME(al,adn))) {
          if (BITON(ADFLAGS(al,adn), AGROUP)) adn += (int)ADNUM(al,adn);
     }
     return (adn > adsz) ? 0 : adn;
}
/* ANTVGET: Find the position of a name token value in an attribute list.
            Return the position of the attribute definition, or zero
            if none was found.  Set pp to the value, if non-NULL.
*/
int antvget(adsz, nm, pp)
int adsz;                     /* Size of list. */
UNCH *nm;                     /* Value to be found (with length byte). */
UNCH **pp;		      /* Store value here */
{
     int adn = 0;             /* Position in list. */

     while (++adn<=adsz) {
          /* Test only name group members. */
          if (BITON(ADFLAGS(al,adn), AGROUP)) {
	       int advn;      /* Position of value in sub-list. */
               if ((advn = amemget(&al[adn], (int)ADNUM(al,adn), nm))!=0) {
		    if (pp)
			 *pp = al[adn+advn].adname;
                    return adn;
               }
               adn += (int)ADNUM(al,adn);
          }
     }
     return 0;
}
/* AMEMGET: Get the position of a member in an attribute name token group.
            Returns the position, or zero if not found.
            The length byte is ignored in the comparison so that final
            form tokens from ATTVAL can be compared to group members.
*/
int amemget(anmtgrp, adsz, nm)
struct ad anmtgrp[];          /* Name token group. */
int adsz;                     /* Size of group. */
UNCH *nm;                     /* Name to be found (with length byte). */
{
     int adn = 0;             /* Position in group. */

     while ( ++adn<=adsz && ustrncmp(nm+1, anmtgrp[adn].adname+1, (UNS)*nm-1)) ;
     return (adn>adsz) ? 0 : adn;
}
/* VALLEN: Returns the length of an attribute value for capacity
           calculations.  Normally, the length is NORMSEP plus the number
           of characters.  For tokenized lists, it is NORMSEP,
           plus the number of characters in the tokens, plus
           NORMSEP for each token.
	   ACHARS and tokenized lists don't have a length byte.

*/
UNS vallen(type, num, def)
int type;                     /* ADTYPE(al,adn) */
int num;                      /* ADNUM(al,adn) */
UNCH *def;                    /* ADVAL(al,adn) */
{
     if (type == ACHARS)
	  return ustrlen(def) + NORMSEP;
     if (type < ATKNLIST)
	  return *def - 2 + NORMSEP;
     return ustrlen(def) + num * (NORMSEP - 1) + NORMSEP;
}
/* PARSEGRP: Parse GI names, get their etds, and form an array of pointers
             to them.  The array is terminated by a NULL pointer.
             The number of pointers (including the NULL) is returned.
             The grp buffer must have room for GRPCNT+1 etds.
*/
UNS parsegrp(grp, pcb, tbuf)
struct etd *grp[];            /* Buffer for building the group. */
struct parse *pcb;            /* Current parse control block. */
UNCH *tbuf;
{
     int grpcnt = 0;          /* Number of etds in the group. */
     int i;
     int essv = es;           /* Entity stack level when grp started. */

     while (parse(pcb)!=GRPE && grpcnt<GRPCNT) {
          switch (pcb->action) {
          case NAS_:          /* GI name: get its etd for the group. */
               grp[grpcnt] = etddef(parsenm(tbuf, NAMECASE));
	       for (i = 0; i < grpcnt; i++)
		    if (grp[i] == grp[grpcnt]) {
			 mderr(98, ntoa(grpcnt + 1), grp[grpcnt]->etdgi + 1);
			 break;
		    }
	       if (i == grpcnt)
		    grpcnt++;
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
     grp[grpcnt++] = 0;       /* NULL pointer indicates end of group. */
     if (es!=essv) synerr(37, pcb);
     return grpcnt;           /* Return number of ptrs in group. */
}
/* PARSNGRP: Parse notation names, get their dcncbs, and form an array of
             pointers to them.  The array is terminated by a NULL pointer.
             The number of pointers (including the NULL) is returned.
             The grp buffer must have room for GRPCNT+1 members.
*/
UNS parsngrp(grp, pcb, tbuf)
struct dcncb *grp[];          /* Buffer for building the group. */
struct parse  *pcb;           /* Current parse control block. */
UNCH *tbuf;
{
     int grpcnt = 0;          /* Number of members in the group. */
     int i;
     int essv = es;           /* Entity stack level when grp started. */

     while (parse(pcb)!=GRPE && grpcnt<GRPCNT) {
          switch (pcb->action) {
          case NAS_:          /* Member name: get its control block. */
               grp[grpcnt] = dcndef(parsenm(tbuf, NAMECASE));
	       for (i = 0; i < grpcnt; i++)
		    if (grp[i] == grp[grpcnt]) {
			 mderr(98, ntoa(grpcnt + 1), grp[grpcnt]->ename + 1);
			 break;
		    }
	       if (i == grpcnt)
		    grpcnt++;
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
     grp[grpcnt++] = 0;       /* NULL pointer indicates end of group. */
     if (es!=essv) synerr(37, pcb);
     return grpcnt;           /* Return number of ptrs in group. */
}
/* COPYGRP: Allocate storage for a group and copy the group into it.
*/
PETD *copygrp(pg, grpsz)
PETD pg[];                    /* Pointer to a group (array of etd ptrs). */
UNS grpsz;                    /* Number of ptrs in grp, including final NULL. */
{
     UNS glen;                /* Group length in characters. */
     PETD *gnm;               /* Ptr to permanent name group. */

     if (pg==0) return (PETD *)0;
     glen = grpsz * sizeof(struct etd *);
     memcpy( (UNIV)(gnm = (struct etd **)rmalloc(glen)) , (UNIV)pg, glen );
     return gnm;
}
/* INGRP: Locate an etd in a name group and return its index+1 (or zero
          if not found).
*/
int ingrp(pg, ketd)
PETD pg[];                    /* Array of pointers to etds. */
PETD ketd;                    /* Pointer to etd to be found in group. */
{
     int i = 0;               /* Array index. */

     while (pg[i]) if (pg[i++]==ketd) return i;
     return 0;
}
/* PARSELIT: Parse a delimited string and collect it into a token.
             Caller supplies buffer, which must be 1 longer than
             maximum string allowed.
             Caller also supplies character that delimits the string.
             TODO: Return 1 if CDATA, SDATA or NONSGML occurred.
*/
#ifdef USE_PROTOTYPES
VOID parselit(UNCH *tbuf, struct parse *pcb, UNS maxlen, UNCH del)
#else
VOID parselit(tbuf, pcb, maxlen, del)
UNCH *tbuf;                   /* Work area for tokenization (parmlen+1). */
struct parse *pcb;            /* Current parse control block. */
UNS maxlen;                   /* Maximum length of token. */
UNCH del;                     /* Literal delimiter: LIT LITA PIC EOS */
#endif
{
     UNCH *pt = tbuf;         /* Current pointer into tbuf. */
     UNCH lexsv = lexlms[del];/* Saved lexlms value of delimiter. */
     int essv = es;           /* Entity stack level when literal started. */
     UNCH datadel;            /* Delimiter for CDATA/SDATA entity. */
     int parmlen = (int)maxlen;  /* Working limit (to be decremented). */

     lexlms[del] = lex.l.litc;   /* Set delimiter to act as literal close. */
     do {
          switch (parse(pcb)) {
               case LP2_:          /* Move 2nd char back to buffer; redo prev.*/
                    REPEATCC;
               case LPR_:          /* Move previous char to buffer; REPEATCC; */
                    REPEATCC;
               case MLA_:          /* Move character to buffer. */
                    *pt++ = *FPOS; --parmlen;
                    continue;

               case FUN_:          /* Function char found; replace with space.*/
                    *pt++ = ' '; --parmlen;
                    continue;

               case RSM_:          /* Record start: ccnt=0; ++rcnt.*/
                    ++RCNT; CTRSET(RSCC); *pt++ = *FPOS; --parmlen;
                    continue;

               case ERX_:          /* Entity reference: cancel LITC delim. */
               case PEX_:          /* Parameter entity ref: cancel LITC delim.*/
                    lexlms[del] = lexsv;
                    continue;

               case EE_:
                    if (es<essv) {
                         synerr(37, pcb);
                         essv = es;
                    }
                    /* If back at top level, re-enable the LITC delimiter. */
                    if (es==essv) lexlms[del] = lex.l.litc;
                    continue;

               case MLE_:          /* Char not allowed in minimum literal. */
                    synerr(63, pcb);
                    continue;

               case DEF_:          /* Data entity: add it to buffer. */
		    if (pcb == &pcblitt) {
			 int parmlensv = parmlen;
			 entdatsw = 0;
			 parmlen = tokdata(pt, parmlen);
			 if (parmlen < 0)
			      break;
			 pt += parmlensv - parmlen;
			 continue;
		    }
                    if ((parmlen -= (int)datalen+2)<0) {entdatsw = 0; break;}
                    *pt++ = datadel =
                         BITON(entdatsw, CDECONT) ? DELCDATA : DELSDATA;
                    entdatsw = 0;
                    memcpy( pt , data, datalen );
                    pt += datalen;
                    *pt++ = datadel;
                    continue;

               case NON_:          /* Non-SGML char (delimited and shifted). */
                    if ((parmlen -= 2)<0) break;
                    memcpy( pt , nonchbuf, 2 );
                    pt += 2;
                    continue;

               case RPR_:          /* Remove character from buffer. */
                    --pt; ++parmlen;
                    break;

               case EOD_:
                    exiterr(92, pcb);

               default:
                    break;
          }
          break;
     } while (parmlen>=0 && pcb->action!=TER_);

     if (parmlen<0) {--pt; sgmlerr(134, pcb, ntoa((int)maxlen),(UNCH *)0); REPEATCC;}
     datalen = (UNS)(pt-tbuf);/* To return PI string to text processor. */
     *pt++ = EOS;
     lexlms[del] = lexsv;     /* Restore normal delimiter handling. */
     if (es!=essv) synerr(37, pcb);
     return;
}

/* Handle a data entity in a tokenized attribute value literal.
Parmlen is amount of space left.  Return new parmlen. If there's not
enough space return -1, and copy up to parmlen + 1 characters. */

int tokdata(pt, parmlen)
UNCH *pt;
int parmlen;
{
     int skip = (pcblitt.newstate == 0);
     int i;

     for (i = 0; parmlen >= 0 && i < datalen; i++) {
	  switch (data[i]) {
	  case RSCHAR:
	       /* ignore it */
	       break;
	  case RECHAR:
	  case TABCHAR:
	  case SPCCHAR:
	       if (!skip) {
		    *pt++ = data[i];
		    parmlen--;
		    skip = 1;
	       }
	       break;
	  default:
	       if (data[i] == DELNONCH) {
		    assert(i + 1 < datalen);
		    if ((parmlen -= 2) < 0)
			 break;
		    *pt++ = DELNONCH;
		    *pt++ = data[++i];
		    skip = 0;
	       }
	       else {
		    *pt++ = data[i];
		    parmlen--;
		    skip = 0;
	       }
	       break;
	  }
     }
     pcblitt.newstate = skip ? 0 : pcblittda;
     return parmlen;
}


/* PARSEMD: Parser for markup declarations.
            It returns a token each time it is called.

*/
int parsemd(pt, namecase, lpcb, tokenlen)
UNCH *pt;                     /* Token buffer: >=tokenlen+2. */
int namecase;                 /* Case translation: ENTCASE NAMECASE AVALCASE. */
struct parse *lpcb;           /* Parse control block for literal parse. */
UNS tokenlen;                 /* Max length of expected token: NAMELEN LITLEN */
{
     struct parse *pcb;       /* Current parse control block. */

     pcb = (lpcb) ? &pcbmd : &pcbmdc;  /* If no literal pcb, dcl is comment. */

     doparse: while (parse(pcb)==EE_)
          if (es<mdessv) {synerr(37, pcb); mdessv = es;}
     if (pcb->action==PIE_) { /* PI entity reference not allowed. */
          entpisw = 0;        /* Reset PI entity indicator. */
          synerr(59, pcb);
          goto doparse;
     }
     ++parmno;           /* Increment parameter counter. */
     switch (pcb->action) {
     case CDR:           /* COM[1] (MINUS) occurred previously. */
          REPEATCC;
          return (int)pcb->action;
     case LIT:           /* Literal: CDATA with LIT delimiter. */
          parselit(pt, lpcb, tokenlen, lex.d.lit);
          return (int)pcb->action;
     case LITE:          /* Literal: CDATA with LITA delimiter. */
          parselit(pt, lpcb, tokenlen, lex.d.lita);
          return((int)(pcb->action = LIT));
     case RNS:           /* Reserved name started (after RNI). */
          parsenm(pt, NAMECASE);
          return (int)pcb->action;
     case NAS:           /* Name started. */
          if (namecase!=AVALCASE) {
               parsenm(pt, namecase);
               return (int)pcb->action;
          }
          /* Treat attribute value as name character string. */
     case NMT:           /* Name token string. */
          parsetkn(pt, NMC, (int)tokenlen);  /* Get undelimited value. */
          return (int)pcb->action;
     case NUM:           /* Number or number token string. */
          parsetkn(pt, (UNCH)((int)tokenlen<=NAMELEN ? NU:NMC), (int)tokenlen);
          return (int)pcb->action;
     case PENR:
	  REPEATCC;
	  return (pcb->action = PEN);
     case EOD_:
          exiterr(133, pcb);
          /* EXIT */
     default:            /* End of declaration. */
          return (int)pcb->action; /* EMD GRPS MGRP PEN PGRP */
     }
}
/* PARSEMOD: If the declared content was a keyword, the token count is zero
             and it is only necessary to save the type.  Otherwise,
             collect the outermost token count and model type bytes for a model.
             The count includes tokens found in nested groups also.
             After building the model, parse for its occurrence indicator.
*/
struct thdr *parsemod(dctype)
int dctype;                        /* Content type (0=model). */
{
     gbuf[0].ttype = (UNCH)dctype; /* Initialize content flags byte. */
     if (dctype) {gbuf[0].tu.tnum = 0; return gbuf;} /* Return if not model. */

     gbuf[0].tu.tnum = 0;          /* Don't count 1st group or model header. */
     gbuf[1].ttype = 0;            /* Initialize 1st group type ... */
     gbuf[1].tu.tnum = 0;          /* and count. */
     grplvl = 1;                   /* Content model is 1st level group. */
     pcbgrcm.newstate = 0;         /* Go parse the model group. */
     /* Empty group is trapped during syntax parse; other errors return NULL. */
     if (!parsegcm(&pcbgrcm, &gbuf[1], &gbuf[0])) return (struct thdr *)0;
     parse(&pcbgrcs);             /* Get the model suffix, if there is one. */
     switch(pcbgrcs.action) {
     case OPT:                     /* OPT occurrence indicator for model. */
          SET(gbuf[1].ttype, TOPT|TXOPT);
          break;
     case REP:                     /* REP occurrence indicator for model. */
          SET(gbuf[1].ttype, TREP|TXREP);
          break;
     case OREP:                    /* OREP occurrence indicator for model. */
          SET(gbuf[1].ttype, TOREP|TXOREP);
          break;
     default:                      /* RCR_: Repeat char and return. */
          break;
     }
     if (sw.swambig) ambig();	   /* Check content model for ambiguity. */
     return gbuf;
}
/* PARSEGCM: Collect token headers (struct thdr) into a group (array).
             An etd is defined for each GI (if none exists) and its pointer is
             stored in the header.  The function is called recursively.
*/
struct thdr *parsegcm(pcb, pgh, gbuf)
struct parse *pcb;                 /* Current parse control block. */
struct thdr *pgh;                  /* Current group header in group buffer. */
struct thdr *gbuf;                 /* Header for outermost group (model). */
{
#define MCON gbuf->ttype           /* Model type (content attributes). */
     struct thdr *pg=pgh;          /* Current group token. */
     struct thdr *pgsv=pgh;        /* Saved current token for occ indicator. */
     int optcnt = 0;               /* Count of optional tokens in group. */
     int essv = es;                /* Entity stack level when grp started. */

    while (gbuf->tu.tnum<=GRPGTCNT && pgh->tu.tnum<=GRPCNT && parse(pcb)!=GRPE)
     switch (pcb->action) {

     case NAS_:          /* GI name: get its etd and store it. */
          ++gbuf->tu.tnum; ++pgh->tu.tnum;
          (pgsv = ++pg)->ttype = TTETD;
          pg->tu.thetd = etddef(parsenm(tbuf, NAMECASE));
          SET(MCON, MGI);
          continue;

     case RNS_:          /* Reserved name started (#PCDATA). */
          parsenm(tbuf, NAMECASE);
          if (ustrcmp(tbuf+1, key[KPCDATA])) {
               mderr(116, ntoa(gbuf->tu.tnum), tbuf+1);
               return (struct thdr *)0;
          }
          /* If #PCDATA is the first non-group token, model is a phrase. */
          if (!MCON) SET(MCON, MPHRASE);
     case DTAG:          /* Data tag template ignored; treat as #PCDATA. */
          if (pcb->action==DTAG) SET(pgh->ttype, TTSEQ); /* DTAG is SEQ grp. */
          ++gbuf->tu.tnum; ++pgh->tu.tnum;
          (++pg)->ttype = TTCHARS+TOREP;/* #PCDATA is OPT and REP. */
          pg->tu.thetd = ETDCDATA;
          ++optcnt;                     /* Ct opt tokens to see if grp is opt.*/
          SET(MCON, MCHARS);
          continue;

     case GRP_:          /* Group started. */
          ++gbuf->tu.tnum; ++pgh->tu.tnum;
          (pgsv = ++pg)->ttype = 0;     /* Type will be set by connector. */
          pg->tu.tnum = 0;              /* Group has number instead of etd. */
          if (++grplvl>GRPLVL) {
               mderr(115, ntoa(gbuf->tu.tnum), (UNCH *)0);
               return (struct thdr *)0;
          }
          pg = parsegcm(pcb, pg, gbuf);
          if (!pg) return (struct thdr *)0;
          if (GET(pgsv->ttype, TOPT)) ++optcnt;  /* Indicate nested opt grp. */
          --grplvl;
          continue;

     case OREP:          /* OREP occurrence indicator for current token.*/
          SET(pgsv->ttype, TREP|TXREP);
                         /* Now treat like OPT. */
     case OPT:           /* OPT occurrence indicator for current token. */
          SET(pgsv->ttype, TXOPT);
          if (GET(pgsv->ttype, TOPT)) continue;  /* Exit if nested opt grp. */
          SET(pgsv->ttype, TOPT);
          ++optcnt;      /* Count opt tokens to see if grp is optional. */
          continue;
     case REP:           /* REP occurrence indicator for current token. */
          SET(pgsv->ttype, TREP|TXREP);
          continue;

     case OR:            /* OR connector found. */
          if BITOFF(pgh->ttype, TTAND) SET(pgh->ttype, TTOR);
          else if (GET(pgh->ttype, TTAND)!=TTOR)
               mderr(55, ntoa(gbuf->tu.tnum), (UNCH *)0);
          continue;
     case AND:           /* AND connector found. */
          if BITOFF(pgh->ttype, TTAND) SET(pgh->ttype, TTAND);
          else if (GET(pgh->ttype, TTAND)!=TTAND)
               mderr(55, ntoa(gbuf->tu.tnum), (UNCH *)0);
          continue;
     case SEQ:           /* SEQ connector found. */
          if BITOFF(pgh->ttype, TTAND) SET(pgh->ttype, TTSEQ);
          else if (GET(pgh->ttype, TTAND)!=TTSEQ)
               mderr(55, ntoa(gbuf->tu.tnum), (UNCH *)0);
          continue;

     case EE_:           /* Entity ended (correctly or incorrectly). */
          if (es<essv) {synerr(37, pcb); essv = es;}
          continue;

     case PIE_:          /* PI entity reference (not permitted). */
          entpisw = 0;   /* Reset PI entity indicator. */
          synerr(59, pcb);
          continue;

     default:            /* Syntax errors return in disgrace. */
          synerr(37, pcb);
          return (struct thdr *)0;
     }
     if (pgh->tu.tnum>GRPCNT) {
          mderr(113, ntoa(gbuf->tu.tnum), (UNCH *)0);
          return (struct thdr *)0;
     }
     if (gbuf->tu.tnum>GRPGTCNT) {
          mderr(114, ntoa(gbuf->tu.tnum), (UNCH *)0);
          return (struct thdr *)0;
     }
     if (pgh->tu.tnum==1) SET(pgh->ttype, TTSEQ); /* Unit grp is SEQ. */
     /* An optional token in an OR group makes the group optional. */
     if (GET(pgh->ttype, TTMASK)==TTOR && optcnt) SET(pgh->ttype, TOPT);
     /* If all tokens in any group are optional, so is the group. */
     if (pgh->tu.tnum<=optcnt) SET(pgh->ttype, TOPT);

     if (es!=essv) synerr(37, pcb);
     return pg;                             /* Return pointer to GRPS token. */
}
/* PARSENM: Parser for SGML names, which can be translated with LEXTRAN.
            The input is read from the entity stack.  CC is 1st char of name.
            Returns a pointer to the parsed name.
*/
UNCH *parsenm(tbuf, nc)
UNCH *tbuf;                   /* Buffer for name: >=NAMELEN+2. */
int nc;                       /* Namecase translation: 1=yes; 0=no. */
{
     UNCH   len;              /* Length of name (incl EOS & length byte). */

     *(tbuf + (len = 1) ) = nc ? lextran[*FPOS] : *FPOS;
     while ((NEWCC, (int)lextoke[*FPOS]>=NMC) && (len<NAMELEN)) {
          TRACETKN(NMC, lextoke);
          if (lextoke[*(tbuf + ++len) = (nc ? lextran[*FPOS] : *FPOS)]==EOB) {
               --len;
               entget();
          }
     }
     REPEATCC;                       /* Put back the non-token character. */
     *(tbuf + ++len) = EOS;          /* Terminate name with standard EOS. */
     *tbuf = ++len;                  /* Store length ahead of name. */
     return tbuf;
}
/* PARSETKN: Parser for start-tag attribute value tokens.
             First character of token is already in *FPOS.
             Returns a pointer to the parsed token.
	     Parsed token has EOS but no length byte.
*/
#ifdef USE_PROTOTYPES
UNCH *parsetkn(UNCH *tbuf, UNCH scope, int maxlen)
#else
UNCH *parsetkn(tbuf, scope, maxlen)
UNCH *tbuf;		      /* Buffer for token: >=maxlen+1. */
UNCH scope;		      /* Minimum lexical class allowed. */
int maxlen;		      /* Maximum length of a token. */
#endif
{
     int i = 1;
     tbuf[0] = *FPOS;
     while (i < maxlen) {
	  NEWCC;
	  if (lextoke[*FPOS] < scope) {
	       REPEATCC;
	       break;
	  }
          TRACETKN(scope, lextoke);
	  if (*FPOS == EOBCHAR)
	       entget();
	  else
	       tbuf[i++] = *FPOS;
     }
     tbuf[i] = EOS;
     return tbuf;
}
/* PARSESEQ: Parser for blank sequences (i.e., space and TAB characters ).
             First character of sequence is already in *FPOS.
*/
VOID parseseq(tbuf, maxlen)
UNCH *tbuf;		      /* Buffer for storing found sequence. */
int maxlen;		      /* Maximum length of a blank sequence. */
{
     tbuf[0] = *FPOS;
     datalen = 1;
     for (;;) {
	  NEWCC;
	  if (*FPOS == EOBCHAR) {
	       entget();
	       continue;
	  }
	  if ((lextoke[*FPOS] != SEP && *FPOS != SPCCHAR)
	      || datalen >= maxlen)
	       break;
	  tbuf[datalen++] = *FPOS;
	  TRACETKN(SEP, lextoke);
     }
}
/* S2VALNM: Parser for attribute values that are tokenized like names.
            The input is read from a string (hence S ("string") 2 ("to") VALNM).
            It stops at the first bad character.
            Returns a pointer to the created name.
*/
#ifdef USE_PROTOTYPES
UNCH *s2valnm(UNCH *nm, UNCH *s, UNCH scope, int translate)
#else
UNCH *s2valnm(nm, s, scope, translate)
UNCH *nm;                     /* Name to be created. */
UNCH *s;                      /* Source string to be parsed as name. */
UNCH scope;                   /* Minimum lexical class allowed. */
int translate;                /* Namecase translation: 1=yes; 0=no. */
#endif
{
     UNCH len = 0;            /* Length of name (incl EOS and length). */

     for (; (int)lextoke[*s] >= scope && len < NAMELEN; s++)
	  nm[++len] = translate ? lextran[*s] : *s;
     nm[++len] = EOS;         /* Terminate name with standard EOS. */
     *nm = ++len;             /* Store length ahead of name. */
     return nm;
}
/* PARSEVAL: Parser for attribute values.
             The input is read from a string and tokenized in a buffer.
             The input is terminated by EOS.
             Each token is preceded by its actual length; there is no EOS.
             If an error occurs while parsing, or
             if a token doesn't conform, set the token count to 0 to show that
             value was not tokenized and return the error code.
             After successful parse, return buffer length and 0 error code.
             The number of tokens found is set in external variable tokencnt.
*/
int parseval(s, atype, tbuf)
UNCH *s;                      /* Source string to be parsed as token list. */
UNS atype;                    /* Type of token list expected. */
UNCH *tbuf;                   /* Work area for tokenization. */
{
     int t;
     UNCH *pt = tbuf;

     pcbval.newstate = 0; tokencnt = 0;
     while (1) {
          for (;;) {
               pcbval.input = lextoke[*s];
               pcbval.state = pcbval.newstate;
               pcbval.newstate = (*(pcbval.ptab + pcbval.state)) [pcbval.input];
               pcbval.action = (*(pcbval.ptab + pcbval.state+1)) [pcbval.input];
               TRACEVAL(&pcbval, atype, s, tokencnt);
	       if (pcbval.action != NOPA)
		    break;
	       s++;
          }


          switch (pcbval.action) {
          case INVA:          /* Invalid character; terminate parse. */
               if (*s == '\0') goto alldone;  /* Normal termination. */
               tokencnt = 0;  /* Value was not tokenized. */
               return(14);
          case LENA:          /* Length limit of token exceeded; end parse. */
               tokencnt = 0;  /* Value was not tokenized. */
               return(15);
          default:            /* Token begun: NUMA, NASA, or NMTA. */
               break;
          }

          ++tokencnt;         /* One token per iteration. */
          switch (atype) {
          case AENTITY:
               if (tokencnt>1) {tokencnt = 0; return(16);}
          case AENTITYS:
               if (pcbval.action!=NASA) {tokencnt = 0; return(17);}
               s2valnm(pt, s, NMC, ENTCASE);
               break;

          case AID:
          case AIDREF:
          case ANAME:
          case ANOTEGRP:
               if (tokencnt>1) {tokencnt = 0; return(16);}
          case AIDREFS:
          case ANAMES:
               if (pcbval.action!=NASA) {tokencnt = 0; return(17);}
               s2valnm(pt, s, NMC, NAMECASE);
               break;

          case ANMTGRP:
          case ANMTOKE:
               if (tokencnt>1) {tokencnt = 0; return(16);}
          case ANMTOKES:
               /* No test needed because NMTA, NUMA and NASA are all valid. */
               s2valnm(pt, s, NMC, NAMECASE);
               break;

          case ANUMBER:
               if (tokencnt>1) {tokencnt = 0; return(16);}
          case ANUMBERS:
               if (pcbval.action!=NUMA) {tokencnt = 0; return(17);}
               s2valnm(pt, s, NU, NAMECASE);
	       t = lextoke[s[*pt - 2]];
	       if (t == NMS || t == NMC) {tokencnt = 0; return(17);}
               break;

          case ANUTOKE:
               if (tokencnt>1) {tokencnt = 0; return(16);}
          case ANUTOKES:
               if (pcbval.action!=NUMA) {tokencnt = 0; return(17);}
               s2valnm(pt, s, NMC, NAMECASE);
               break;
          }
	  *pt -= 2;
	  s += *pt;
	  pt += *pt + 1;
     }
 alldone:
     *pt++ = EOS;
     if (*tbuf == '\0')
	  return 25;
     if (atype < ATKNLIST)
	  *tbuf += 2;	      /* include length and EOS */
     return 0;
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
