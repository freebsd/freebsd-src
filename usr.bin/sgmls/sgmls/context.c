#include "sgmlincl.h"         /* #INCLUDE statements for SGML parser. */
#include "context.h"

#define GI (tags[ts].tetd->etdgi+1)              /* GI of current element. */
#define NEWGI (newetd->etdgi+1)                  /* GI of new tag. */
#define STATUS (*statuspt)    /* Token status: RCHIT RCMISS RCEND RCREQ RCNREQ*/
#define PEX (-1)              /* GI is a plus exception and not a minus. */

#define ANYHIT(h) (grplongs == 1 ? ((h)[0] != 0) : anyhit(h))
#define HITSET(h, n) (h[(unsigned)(n-1)>>LONGPOW] \
		      |= (1L<<((n-1)&(LONGBITS-1))))
#define HITON(h, n) (h[(unsigned)(n-1)>>LONGPOW] & (1L<<((n-1)&(LONGBITS-1))))

#define HITOFF(h, n) (!(HITON(h, n)))

#define TOKENHIT HITON(H,T)

static
VOID copypos(to, from)
struct mpos *to, *from;
{
     int i;
     for (i = 0; i <= (int)from[0].t; i++) {
	  to[i].g = from[i].g;
	  to[i].t = from[i].t;
	  memcpy(to[i].h, from[i].h, grplongs*sizeof(unsigned long));
     }
}

/* CONTEXT: Determine whether a GI is valid in the present structural context.
            Returns RCHIT if valid, RCEND if element has ended, RCREQ if a
            different element is required, and RCMISS if it is totally invalid.
            On entry, pos points to the model token to be tested against the GI.
            TO DO: Save allowed GIs for an error message on an RCMISS.
                   Support a "query" mode (what is allowed now?) by working
                   with a copy of pos.
*/
int context(gi, mod, pos, statuspt, mexts)
struct etd *gi;               /* ETD of new GI. */
struct thdr mod[];            /* Model of current open element. */
struct mpos pos[];            /* Position in open element's model. */
UNCH *statuspt;               /* Token status: RCHIT RCMISS RCEND RCREQ RCNREQ*/
int mexts;                    /* >0=stack level of minus grp; -1=plus; 0=none.*/
{
     UNCH toccsv, gtypesv;    /* Save token's TOCC and GTYPE in case grp ends.*/

     if (mexts == -1) {
	  if (STATUS == RCEND)
	       return RCPEX;
	  copypos(savedpos, pos);
     }
     Tstart = T;              /* Save starting token for AND group testing. */
     while (STATUS!=RCMISS && STATUS!=RCEND) {
          TRACEGI("CONTEXT", gi, mod, pos, Tstart);
          while (TTYPE==TTOR || TTYPE==TTSEQ || TTYPE==TTAND) {
               pos[P+1].g = M++; pos[++P].t = 1; HITCLEAR(H);
               Tstart = T;    /* Save starting token for AND group testing. */
               TRACEGI("OPENGRP", gi, mod, pos, Tstart);
          }
          STATUS = (UNCH)tokenreq(gi, mod, pos);
          TRACEGI("STATUS", gi, mod, pos, Tstart);
          if (gi==TOKEN.tu.thetd) {     /* Hit in model. */
               STATUS = (UNCH)RCHIT;
               gtypesv = GTYPE; toccsv = TOCC;
               newtoken(mod, pos, statuspt);
               return(mexts<=0 ? RCHIT : (gtypesv==TTOR || BITON(toccsv, TOPT))
                                       ?  RCMEX : RCHITMEX);
          }
          if (STATUS==RCREQ) {
	       if (mexts == -1)
		    break;
               STATUS = RCHIT;
               nextetd = TOKEN.tu.thetd;
               newtoken(mod, pos, statuspt);
               return(RCREQ);
          }
          /* else if (STATUS==RCNREQ) */
               if (mexts>0) return(RCMEX);
               newtoken(mod, pos, statuspt);
     }
     if (mexts == -1) {
	  copypos(pos, savedpos);
	  return STATUS = RCPEX;
     }
     return((int)STATUS);
}
/* ECONTEXT: Determine whether the current element can be ended, or whether
             non-optional tokens remain at the current level or higher.
             Returns 1 if element can be ended, or 0 if tokens remain.
             On entry, STATUS==RCEND if there are no tokens left; if not,
             pos points to the next model token to be tested.
             TO DO: Support a "query" mode (what is required now?) by working
                    with a copy of pos.
*/
int econtext(mod, pos, statuspt)
struct thdr mod[];            /* Model of current open element. */
struct mpos pos[];            /* Position in open element's model. */
UNCH *statuspt;               /* Token status: RCHIT RCMISS RCEND RCREQ RCNREQ*/
{
     unsigned next;           /* Position in AND group of next testable token.*/

     Tstart = T;
     TRACEEND("ECONT", mod, pos, 0, 0, Tstart);
     if (P<=1) {nextetd = 0; return(TOKENHIT || BITON(TOCC, TOPT));}
     nextetd = TTYPE == TTETD ? TOKEN.tu.thetd : 0;
     while (STATUS!=RCMISS && STATUS!=RCEND) {
          STATUS = (UNCH)testend(mod, pos, 0, 0);
          TRACEEND("ECONTEND", mod, pos, 0, 0, Tstart);
          nextetd = P<=1 || TTYPE != TTETD ? 0 : TOKEN.tu.thetd;
          if (STATUS==RCEND)       return(1);
          if (P<=1)                return(TOKENHIT || BITON(TOCC, TOPT));
          if (STATUS==RCMISS) {
               if (BITON(TOCC, TOPT)) nextetd = 0;
               return(0);
          }
          if (!tokenopt(mod, pos)) return(0);

          STATUS = RCNREQ;
          if (GTYPE!=TTAND) ++T;   /* T!=GNUM or group would have ended. */
          else T = (UNCH)(((next = (UNS)offbit(H, (int)T, GNUM))!=0) ?
               next : offbit(H, 0, GNUM));

          M = G + grpsz(&GHDR, (int)T-1) + 1;
          TRACEEND("ECONTNEW", mod, pos, 0, 0, Tstart);
     }
     if (STATUS==RCMISS) {
          if (BITON(TOCC, TOPT)) nextetd = 0;
          return(0);
     }
     return(1);               /* STATUS==RCEND */
}
/* NEWTOKEN: Find the next token to test.  Set STATUS to indicate results:
                  RCEND  if element has ended (no more tokens to test);
                  RCREQ  if required new token was found;
                  RCNREQ if non-required new token was found;
                  RCHIT  if a hit token was repeated (now non-required);
              and RCMISS if a new token can't be found because current token
              (which was not hit) was neither unconditionally required nor
              optional.
*/
VOID newtoken(mod, pos, statuspt)
struct thdr mod[];            /* Model of current open element. */
struct mpos pos[];            /* Position in open element's model. */
UNCH *statuspt;               /* Token status: RCHIT RCMISS RCEND RCREQ RCNREQ*/
{
     unsigned nextand = 0;    /* Position in AND group of next testable token.*/
     int currhit = (STATUS==RCHIT); /* 1=current GI hit; 0=not. */

     /* If the GI was a hit, turn on the hit bit and set the status to
        assume that the token to be tested against the next GI will
        be non-required.  If the current token is repeatable, exit so
        it will stand as the next token to test.
     */
     if (STATUS==RCHIT) {
          HITSET(H, T);
	  STATUS = RCNREQ;
          if (BITON(TOCC, TREP)) return;
     }
     /* At this point, we must determine the next token to test:
        either against the next GI, if this one was a hit, or
        against the same GI if conditions permit a retry.
        To find the next token, we must first end the current group,
        if possible, and any we can that contain it.
        If the outermost group was a hit and is repeatable, or
        if the element has ended, we exit now.
        If it hasn't ended, or was optional and ended with a miss,
        we can retry the GI against the next token.
     */
     if ((STATUS = (UNCH)testend(mod, pos, 1, 1))!=RCNREQ) return;

     /* At this point, the "current token" is either the original one,
        or the token for the highest level unhit group that it ended.
        We will retry a missed GI, by testing it against the next
        token, if the current token:
        1. Is optional;
        2. Was hit (i.e., because it is repeatable and was hit by a
           previous GI or because it is a hit group that just ended);
        3. Is in an AND or OR group and is not the last testable token.

        It will be the next sequential one (unhit one, in an AND group);
        if there are none left, use the first unhit token in the group.
        In either case, set M to correspond to the new T.
     */
     retest:
     TRACEEND("RETEST", mod, pos, (int)nextand, 1, Tstart);
     if (GTYPE==TTAND) {
          nextand = offbit(H, (int)T, GNUM);
	  if (!nextand)
	       nextand = offbit(H, 0, GNUM);
     }
     if ( BITON(TOCC, TOPT)
       || TOKENHIT
       || GTYPE==TTOR              /* T!=GNUM or group would have ended. */
       || nextand ) {
          if (GTYPE!=TTAND) ++T;   /* T!=GNUM or group would have ended. */
          else T = nextand;
          M = G + grpsz(&GHDR, (int)T-1) + 1;
          if (GTYPE==TTAND) {
	       /* If AND group wrapped, it can end if all non-optionals were
		  hit. */
	       if (T==Tstart && !currhit) {
                    UNCH Psave = P;
                    int rc = testend(mod, pos, 0, 1);
                    if (Psave!=P) {if ((STATUS = (UNCH)rc)==RCNREQ) goto retest;}
                    else STATUS = RCMISS;
               }

	       /* We only test unhit tokens, so we must use an unhit token
		  as Tstart (which is used to detect when the AND group has
		  wrapped). */
	       else if (HITON(H,Tstart)) Tstart = T;
	  }
     }
     else STATUS = RCMISS;
     TRACEEND("NEWTOKEN", mod, pos, (int)nextand, 1, Tstart);
}
/* TESTEND: End the current group, if possible, and any that it is nested in.
            The current token will either be a group header, or some token
            that could not end its group.  Return 1 if the (possibly new)
            current token is repeatable; 0 if it is not.
*/
int testend(mod, pos, andoptsw, newtknsw)
struct thdr mod[];            /* Model of current open element. */
struct mpos pos[];            /* Position in open element's model. */
int andoptsw;                 /* 1=test optional AND members; 0=ignore. */
int newtknsw;                 /* 1=new token test; 0=end element test. */
{
     int rc = 0;              /* Return code: RCNREQ RCHIT RCMISS RCEND */

     while (!rc) {
          TRACEEND("TRACEEND", mod, pos, rc, andoptsw, Tstart);
          /* TESTMISS:
             If we've hit no tokens yet in the current group, and
             the current token is the last unhit one in the group we can test,
             we will end the group (it may never really have started!)
             because we might be able to try the token that follows it.
             In any group, a token is the last testable unhit token if it
             is the last sequential one, as the GI was already tested against
             the preceding unhit tokens.  In addition,
             in a SEQ group, it is the last testable unhit token if it isn't
             optional, because we can't skip past it to the following ones.
             If we end the group, before popping the level, set M to G, as this
             level`s group header will be the next level's current token.
          */
          if (!ANYHIT(H) && (T==GNUM
			     || (GTYPE==TTSEQ && BITOFF(TOCC, TOPT)))) {
               M = G; --P; Tstart = T;
               if (P<=1) {
                    if (BITON(TOCC, TOPT) || TOKENHIT) rc = RCEND;
                    else                               rc = RCMISS;
               }
               continue;
          }
          /* TESTHIT:
             See if we've hit all the non-optional tokens in the group.
             If so, pop to the previous level and set the group's hit bit.
             If we were called from NEWTOKEN we are trying to find the token
             to test against the next start-tag, so if the group is repeatable,
             process it again.  (If not, we were called from ECONTEXT and
             are testing whether the element can be ended.)
             Otherwise, if we are at the first level, the element is over.
          */
          if ((GTYPE==TTOR  && TOKENHIT)
	      || (GTYPE==TTSEQ && T==(UNCH)GNUM
		  && (TOKENHIT || BITON(TOCC, TOPT)))
	      || (GTYPE==TTAND && allhit(&GHDR, H, 0, andoptsw))) {
               M = G;
	       --P;
	       HITSET(H, T);
	       Tstart = T;
               if (newtknsw && BITON(TOCC, TREP)) rc = RCHIT;
               else if (P<=1)                     rc = RCEND;
	       /* If we are looking for a new token to test against the next
		  start-tag, then we need to consider optional and members
		  in this group, even if we didn't need to consider them
		  in the group that we just ended because that group had
		  wrapped. */
	       else if (newtknsw) andoptsw = 1;
               /* Else loop to test new outer group. */
          }
          else rc = RCNREQ;   /* No group ended this time, so return. */
     }
     TRACEEND("ENDFOUND", mod, pos, rc, andoptsw, Tstart);
     return(rc);
}
/* TOKENOPT: Return 1 if current token is contextually optional;
             otherwise, return 0.
*/
int tokenopt(mod, pos)
struct thdr mod[];            /* Model of current open element. */
struct mpos pos[];            /* Position in open element's model. */
{
     TRACEEND("TOKENOPT", mod, pos, 0, 0, Tstart);
     return (BITON(TOCC, TOPT) /* Inherently optional. */
	     || TOKENHIT      /* Was hit (handles "plus" suffix case). */
	     || (!ANYHIT(H) && groupopt(mod, pos)));
			      /* In optional group with no hits. */
}
/* GROUPOPT: Temporarily makes the current group be the current token so that
             TOKENOPT() can be applied to it.  Returns the value returned
             by TOKENOPT.
*/
int groupopt(mod, pos)
struct thdr mod[];            /* Model of current open element. */
struct mpos pos[];            /* Position in open element's model. */
{
     UNCH saveM;              /* Save M when testing if group is not required.*/
     int rc;                  /* 1=contextually optional; 0=not. */

     if (P==1) return(BITON(GOCC, TOPT) || TOKENHIT);
     saveM = M; M = G; --P;
     rc = tokenopt(mod, pos);
     ++P; G = M; M = saveM;
     return(rc);
}
/* TOKENREQ: Returns RCREQ if the current token is "contextually required".
             That is, it is not contextually optional and
                 1) it is a member of a "seq" group that is either required
                    or has at least 1 hit token.
                 2) it is a member of an "and" group in which all other
                    tokens were hit.
                          Optional tokens are not counted
                          if GI is ETDCDATA, as we are looking for an
                          omitted start-tag.  Otherwise, they are counted,
                          as the GI might match one of them.
             Returns RCNREQ if the current token is "not required".
*/
int tokenreq(gi, mod, pos)
struct etd *gi;               /* ETD of new GI. */
struct thdr mod[];            /* Model of current open element. */
struct mpos pos[];            /* Position in open element's model. */
{
     TRACEGI("TOKENREQ", gi, mod, pos, Tstart);
     return( tokenopt(mod, pos) ? RCNREQ
            : ( GTYPE==TTSEQ && (ANYHIT(H) || groupreq(gi, mod, pos)==RCREQ)
#if 0
	       || (GTYPE==TTAND && allhit(&GHDR, H, T, \*gi!=ETDCDATA*\ 1))
#endif
	       )
                ? RCREQ : RCNREQ );
}
/* GROUPREQ: Temporarily makes the current group be the current token so that
             TOKENREQ() can be applied to it.  Returns the value returned
             by TOKENREQ.
*/
int groupreq(gi, mod, pos)
struct etd *gi;               /* ETD of new GI. */
struct thdr mod[];            /* Model of current open element. */
struct mpos pos[];            /* Position in open element's model. */
{
     UNCH saveM;              /* Save M when testing if group is not required.*/
     int rc;                  /* Return code: RCREQ RCNREQ */

     if (P==1) return(BITOFF(GOCC, TOPT) ? RCREQ : RCNREQ);
     saveM = M; M = G; --P;
     rc = tokenreq(gi, mod, pos);
     ++P; G = M; M = saveM;
     return(rc);
}
/* GRPSZ: Returns the number of tokens spanned by a group in the model (M),
          from the group's start (G) to a specified index within the group (T).
          M = 0, plus 1 for each token in the group, plus the size of
          any subgroups (gotten by calling GRPSZ recursively).  On entry,
          M must be equal to G at the current level.
*/
int grpsz(g, t)
struct thdr *g;               /* mod[G]: Ptr to group in the model. */
int t;                        /* T: Index of last token in the group. */
{
     struct thdr *p = g;      /* Ptr to current token in the model. */
     int m = 0;               /* Size of group (including nested groups). */
     int i = 0;               /* Number of group members (loop counter). */
     UNS type;                /* Token type (without TOREP bits). */

     while (++i<=t) {
          ++p; ++m;
          type = GET(p->ttype, TTMASK);
          if (type==TTOR || type==TTSEQ || type==TTAND) {
               m += grpsz(p, p->tu.tnum);
               p = g+m;
          }
     }
     return(m);
}
/* ALLHIT: Returns 1 if all hit bits for the specified group are turned on,
           (other than those that correspond to optional tokens if "opt" is
           0) and the "but" bit (all bits if "but" bit is zero).  Otherwise,
           returns 0.  GRPSZ is used to skip past subgroup tokens.
*/
int allhit(p, hits, but, opt)
struct thdr *p;               /* mod[G]: Ptr to group in the model. */
unsigned long *hits;	      /* H: Hit bits to be tested. */
int but;                      /* Index of bit to ignore; 0=test all. */
int opt;                      /* 1=optional tokens must be hit; 0=ignore. */
{
     int b = 0;               /* Index of bit being tested in hits. */
     int e = p->tu.tnum;      /* Ending index (number of bits to test). */
     unsigned type;           /* Token type (without TOREP bits). */

     while (++p, ++b<=e) {
          if (HITOFF(hits,b) && (opt || BITOFF(p->ttype,TOPT)) && b!=but)
               return 0;
          if ((type = GET(p->ttype,TTMASK))==TTOR || type==TTSEQ || type==TTAND)
               p += grpsz(p, p->tu.tnum);
     }
     return 1;
}
/* OFFBIT: Returns the index of the first unset bit after (i.e., not including)
           the caller's "first" bit. If all bits through the
           specified last bit are on, it returns 0.
*/
int offbit(bits, first, last)
unsigned long *bits;	      /* Bits to be tested. */
int first;                    /* Index of first bit to be tested in bits. */
int last;                     /* Index of last bit to be tested in bits. */
{
     while (++first <= last)
          if (HITOFF(bits, first))
	       return first;
     return 0;
}

/* ANYHIT: Return 1 if any bit is set. */

int anyhit(bits)
unsigned long *bits;
{
     int i;
     for (i = 0; i < grplongs; i++)
	  if (bits[i] != 0)
	       return 1;
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
