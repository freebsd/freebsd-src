/* sgmldecl.c -
   SGML declaration parsing.

   Written by James Clark (jjc@jclark.com).
*/

#include "sgmlincl.h"

/* Symbolic names for the error numbers that are be generated only by
this module. */

#define E_STANDARD 163
#define E_SIGNIFICANT 164
#define E_BADLIT 165
#define E_SCOPE 166
#define E_XNUM 167
#define E_BADVERSION 168
#define E_NMUNSUP 169
#define E_XNMLIT 170
#define E_CHARDESC 171
#define E_CHARDUP 172
#define E_CHARRANGE 173
#define E_7BIT 174
#define E_CHARMISSING 175
#define E_SHUNNED 176
#define E_NONSGML 177
#define E_CAPSET 178
#define E_CAPMISSING 179
#define E_SYNTAX 180
#define E_CHARNUM 181
#define E_SWITCHES 182
#define E_INSTANCE 183
#define E_ZEROFEATURE 184
#define E_YESNO 185
#define E_CAPACITY 186
#define E_NOTSUPPORTED 187
#define E_FORMAL 189
#define E_BADCLASS 190
#define E_MUSTBENON 191
#define E_BADBASECHAR 199
#define E_SYNREFUNUSED 200
#define E_SYNREFUNDESC 201
#define E_SYNREFUNKNOWN 202
#define E_SYNREFUNKNOWNSET 203
#define E_FUNDUP 204
#define E_BADFUN 205
#define E_FUNCHAR 206
#define E_GENDELIM 207
#define E_SRDELIM 208
#define E_BADKEY 209
#define E_BADQUANTITY 210
#define E_BADNAME 211
#define E_REFNAME 212
#define E_DUPNAME 213
#define E_QUANTITY 214
#define E_QTOOBIG 215
#define E_NMSTRTCNT 219
#define E_NMCHARCNT 220
#define E_NMDUP 221
#define E_NMBAD 222
#define E_NMMINUS 223
#define E_UNKNOWNSET 227

#define CANON_NMC '.'		/* Canonical name character. */
#define CANON_NMS 'A'		/* Canonical name start character. */
#define CANON_MIN ':'		/* Canonical minimum data character. */

#define SUCCESS 1
#define FAIL 0
#define SIZEOF(v) (sizeof(v)/sizeof(v[0]))
#define matches(tok, str) (ustrcmp((tok)+1, (str)) == 0)

static UNCH standard[] = "ISO 8879:1986";

#define REFERENCE_SYNTAX "ISO 8879:1986//SYNTAX Reference//EN"
#define CORE_SYNTAX "ISO 8879:1986//SYNTAX Core//EN"

static UNCH (*newkey)[REFNAMELEN+1] = 0;

struct pmap {
     char *name;
     UNIV value;
};

/* The reference capacity set. */
#define REFCAPSET \
{ 35000L, 35000L, 35000L, 35000L, 35000L, 35000L, 35000L, 35000L, 35000L, \
35000L, 35000L, 35000L, 35000L, 35000L, 35000L, 35000L, 35000L }

long refcapset[NCAPACITY] = REFCAPSET;

/* A pmap of known capacity sets. */

static struct pmap capset_map[] = {
     { "ISO 8879:1986//CAPACITY Reference//EN", (UNIV)refcapset },
     { 0 },
};

/* Table of capacity names.  Must match *CAP in sgmldecl.h. */

char *captab[] = {
     "TOTALCAP",
     "ENTCAP",
     "ENTCHCAP",
     "ELEMCAP",
     "GRPCAP",
     "EXGRPCAP",
     "EXNMCAP",
     "ATTCAP",
     "ATTCHCAP",
     "AVGRPCAP",
     "NOTCAP",
     "NOTCHCAP",
     "IDCAP",
     "IDREFCAP",
     "MAPCAP",
     "LKSETCAP",
     "LKNMCAP",
};

/* The default SGML declaration. */
#define MAXNUMBER 99999999L

/* Reference quantity set */

#define REFATTCNT 40
#define REFATTSPLEN 960
#define REFBSEQLEN 960
#define REFDTAGLEN 16
#define REFDTEMPLEN 16
#define REFENTLVL 16
#define REFGRPCNT 32
#define REFGRPGTCNT 96
#define REFGRPLVL 16
#define REFNORMSEP 2
#define REFPILEN 240
#define REFTAGLEN 960
#define REFTAGLVL 24

#define ALLOC_MAX 65534

#define BIGINT 30000

#define MAXATTCNT ((ALLOC_MAX/sizeof(struct ad)) - 2)
#define MAXATTSPLEN BIGINT
#define MAXBSEQLEN BIGINT
#define MAXDTAGLEN 16
#define MAXDTEMPLEN 16
#define MAXENTLVL ((ALLOC_MAX/sizeof(struct source)) - 1)
#define MAXGRPCNT MAXGRPGTCNT
/* Must be between 96 and 253 */
#define MAXGRPGTCNT 253
#define MAXGRPLVL MAXGRPGTCNT
#define MAXLITLEN BIGINT
/* This guarantees that NAMELEN < LITLEN (ie there's always space for a name
in a buffer intended for a literal.) */
#define MAXNAMELEN (REFLITLEN - 1)
#define MAXNORMSEP 2
#define MAXPILEN BIGINT
#define MAXTAGLEN BIGINT
#define MAXTAGLVL ((ALLOC_MAX/sizeof(struct tag)) - 1)

/* Table of quantity names.  Must match Q* in sgmldecl.h. */

static char *quantity_names[] = {
    "ATTCNT",
    "ATTSPLEN",
    "BSEQLEN",
    "DTAGLEN",
    "DTEMPLEN",
    "ENTLVL",
    "GRPCNT",
    "GRPGTCNT",
    "GRPLVL",
    "LITLEN",
    "NAMELEN",
    "NORMSEP",
    "PILEN",
    "TAGLEN",
    "TAGLVL",
};

static int max_quantity[] = {
    MAXATTCNT,
    MAXATTSPLEN,
    MAXBSEQLEN,
    MAXDTAGLEN,
    MAXDTEMPLEN,
    MAXENTLVL,
    MAXGRPCNT,
    MAXGRPGTCNT,
    MAXGRPLVL,
    MAXLITLEN,
    MAXNAMELEN,
    MAXNORMSEP,
    MAXPILEN,
    MAXTAGLEN,
    MAXTAGLVL,
};

static char *quantity_changed;

/* Non-zero means the APPINFO parameter was not NONE. */
static int appinfosw = 0;

struct sgmldecl sd = {
     REFCAPSET,			/* capacity */
#ifdef SUPPORT_SUBDOC
     MAXNUMBER,			/* subdoc */
#else /* not SUPPORT_SUBDOC */
     0,				/* subdoc */
#endif /* not SUPPORT_SUBDOC */
     1,				/* formal */
     1,				/* omittag */
     1,				/* shorttag */
     1,				/* shortref */
     { 1, 0 },			/* general/entity name case translation */
     {				/* reference quantity set */
	  REFATTCNT,
	  REFATTSPLEN,
	  REFBSEQLEN,
	  REFDTAGLEN,
	  REFDTEMPLEN,
	  REFENTLVL,
	  REFGRPCNT,
	  REFGRPGTCNT,
	  REFGRPLVL,
	  REFLITLEN,
	  REFNAMELEN,
	  REFNORMSEP,
	  REFPILEN,
	  REFTAGLEN,
	  REFTAGLVL,
     },
};

static int systemcharset[] = {
0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47,
48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63,
64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79,
80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95,
96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111,
112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127,
128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 140, 141, 142, 143,
144, 145, 146, 147, 148, 149, 150, 151, 152, 153, 154, 155, 156, 157, 158, 159,
160, 161, 162, 163, 164, 165, 166, 167, 168, 169, 170, 171, 172, 173, 174, 175,
176, 177, 178, 179, 180, 181, 182, 183, 184, 185, 186, 187, 188, 189, 190, 191,
192, 193, 194, 195, 196, 197, 198, 199, 200, 201, 202, 203, 204, 205, 206, 207,
208, 209, 210, 211, 212, 213, 214, 215, 216, 217, 218, 219, 220, 221, 222, 223,
224, 225, 226, 227, 228, 229, 230, 231, 232, 233, 234, 235, 236, 237, 238, 239,
240, 241, 242, 243, 244, 245, 246, 247, 248, 249, 250, 251, 252, 253, 254, 255,
};

static struct pmap charset_map[] = {
     { "ESC 2/5 4/0", (UNIV)asciicharset }, /* ISO 646 IRV */
     { "ESC 2/8 4/2", (UNIV)asciicharset }, /* ISO Registration Number 6, ASCII */
     { SYSTEM_CHARSET_DESIGNATING_SEQUENCE, (UNIV)systemcharset },
				/* system character set */
     { 0 }
};

static int synrefcharset[256];	/* the syntax reference character set */

#define CHAR_NONSGML 01
#define CHAR_SIGNIFICANT 02
#define CHAR_MAGIC 04
#define CHAR_SHUNNED 010

static UNCH char_flags[256];
static int done_nonsgml = 0;
static UNCH *nlextoke = 0;	/* new lextoke */
static UNCH *nlextran = 0;	/* new lextran */


static UNCH kcharset[] = "CHARSET";
static UNCH kbaseset[] = "BASESET";
static UNCH kdescset[] = "DESCSET";
static UNCH kunused[] = "UNUSED";
static UNCH kcapacity[] = "CAPACITY";
static UNCH kpublic[] = "PUBLIC";
static UNCH ksgmlref[] = "SGMLREF";
static UNCH kscope[] = "SCOPE";
static UNCH kdocument[] = "DOCUMENT";
static UNCH kinstance[] = "INSTANCE";
static UNCH ksyntax[] = "SYNTAX";
static UNCH kswitches[] = "SWITCHES";
static UNCH kfeatures[] = "FEATURES";
static UNCH kminimize[] = "MINIMIZE";
static UNCH kdatatag[] = "DATATAG";
static UNCH komittag[] = "OMITTAG";
static UNCH krank[] = "RANK";
static UNCH kshorttag[] = "SHORTTAG";
static UNCH klink[] = "LINK";
static UNCH ksimple[] = "SIMPLE";
static UNCH kimplicit[] = "IMPLICIT";
static UNCH kexplicit[] = "EXPLICIT";
static UNCH kother[] = "OTHER";
static UNCH kconcur[] = "CONCUR";
static UNCH ksubdoc[] = "SUBDOC";
static UNCH kformal[] = "FORMAL";
static UNCH kyes[] = "YES";
static UNCH kno[] = "NO";
static UNCH kappinfo[] = "APPINFO";
static UNCH knone[] = "NONE";
static UNCH kshunchar[] = "SHUNCHAR";
static UNCH kcontrols[] = "CONTROLS";
static UNCH kfunction[] = "FUNCTION";
static UNCH krs[] = "RS";
static UNCH kre[] = "RE";
static UNCH kspace[] = "SPACE";
static UNCH knaming[] = "NAMING";
static UNCH klcnmstrt[] = "LCNMSTRT";
static UNCH kucnmstrt[] = "UCNMSTRT";
static UNCH klcnmchar[] = "LCNMCHAR";
static UNCH kucnmchar[] = "UCNMCHAR";
static UNCH knamecase[] = "NAMECASE";
static UNCH kdelim[] = "DELIM";
static UNCH kgeneral[] = "GENERAL";
static UNCH kentity[] = "ENTITY";
static UNCH kshortref[] = "SHORTREF";
static UNCH knames[] = "NAMES";
static UNCH kquantity[] = "QUANTITY";

#define sderr mderr

static UNIV pmaplookup P((struct pmap *, char *));
static UNCH *ltous P((long));
static VOID sdfixstandard P((UNCH *));
static int sdparm P((UNCH *, struct parse *));
static int sdname P((UNCH *, UNCH *));
static int sdckname P((UNCH *, UNCH *));
static int sdversion P((UNCH *));
static int sdcharset P((UNCH *));
static int sdcsdesc P((UNCH *, int *));
static int sdpubcapacity P((UNCH *));
static int sdcapacity P((UNCH *));
static int sdscope P((UNCH *));
static VOID setlexical P((void));
static VOID noemptytag P((void));
static int sdpubsyntax P((UNCH *));
static int sdsyntax P((UNCH *));
static int sdxsyntax P((UNCH *));
static int sdtranscharnum P((UNCH *));
static int sdtranschar P((int));
static int sdshunchar P((UNCH *));
static int sdsynref P((UNCH *));
static int sdfunction P((UNCH *));
static int sdnaming P((UNCH *));
static int sddelim P((UNCH *));
static int sdnames P((UNCH *));
static int sdquantity P((UNCH *));
static int sdfeatures P((UNCH *));
static int sdappinfo P((UNCH *));

static VOID bufsalloc P((void));
static VOID bufsrealloc P((void));

/* Parse the SGML declaration. Return non-zero if there was some appinfo. */

int sgmldecl()
{
     int i;
     int errsw = 0;
     UNCH endbuf[REFNAMELEN+2];	/* buffer for parsing terminating > */
     static int (*section[]) P((UNCH *)) = {
	  sdversion,
	  sdcharset,
	  sdcapacity,
	  sdscope,
	  sdsyntax,
	  sdfeatures,
	  sdappinfo,
     };
     /* These are needed if we use mderr. */
     parmno = 0;
     mdname = sgmlkey;
     subdcl = NULL;
     for (i = 0; i < SIZEOF(section); i++)
	  if ((*section[i])(tbuf) == FAIL) {
	       errsw = 1;
	       break;
	  }
     if (!errsw)
	  setlexical();
     bufsrealloc();
     /* Parse the >.  Don't overwrite the appinfo. */
     if (!errsw)
	  sdparm(endbuf, 0);
     /* We must exit if we hit end of document. */
     if (pcbsd.action == EOD_)
	  exiterr(161, &pcbsd);
     if (!errsw && pcbsd.action != ESGD)
	  sderr(126, (UNCH *)0, (UNCH *)0);
     return appinfosw;
}

/* Parse the literal (which should contain the version of the
standard) at the beginning of a SGML declaration. */

static int sdversion(tbuf)
UNCH *tbuf;
{
     if (sdparm(tbuf, &pcblitv) != LIT1) {
	  sderr(123, (UNCH *)0, (UNCH *)0);
	  return FAIL;
     }
     sdfixstandard(tbuf);
     if (ustrcmp(tbuf, standard) != 0)
	  sderr(E_BADVERSION, tbuf, standard);
     return SUCCESS;
}

/* Parse the CHARSET section. Use one token lookahead. */

static int sdcharset(tbuf)
UNCH *tbuf;
{
     int i;
     int status[256];

     if (sdname(tbuf, kcharset) == FAIL) return FAIL;
     (void)sdparm(tbuf, 0);

     if (sdcsdesc(tbuf, status) == FAIL)
	  return FAIL;

     for (i = 128; i < 256; i++)
	  if (status[i] != UNDESC)
	       break;
     if (i >= 256) {
	  /* Only a 7-bit character set was described.  Fill it out to 8-bits. */
	  for (i = 128; i < 256; i++)
	       status[i] = UNUSED;
#if 0
	  sderr(E_7BIT, (UNCH *)0, (UNCH *)0);
#endif
     }
     /* Characters that are declared UNUSED in the document character set
	are assigned to non-SGML. */
     for (i = 0; i < 256; i++) {
	  if (status[i] == UNDESC) {
	       sderr(E_CHARMISSING, ltous((long)i), (UNCH *)0);
	       char_flags[i] |= CHAR_NONSGML;
	  }
	  else if (status[i] == UNUSED)
	       char_flags[i] |= CHAR_NONSGML;
     }
     done_nonsgml = 1;
     return SUCCESS;
}

/* Parse a character set description.   Uses one character lookahead. */

static int sdcsdesc(tbuf, status)
UNCH *tbuf;
int *status;
{
     int i;
     int nsets = 0;
     struct fpi fpi;

     for (i = 0; i < 256; i++)
	  status[i] = UNDESC;

     for (;;) {
	  int nchars;
	  int *baseset = 0;

	  if (pcbsd.action != NAS1) {
	       if (nsets == 0) {
		    sderr(120, (UNCH *)0, (UNCH *)0);
		    return FAIL;
	       }
	       break;
	  }
	  if (!matches(tbuf, kbaseset)) {
	       if (nsets == 0) {
		    sderr(118, tbuf+1, kbaseset);
		    return FAIL;
	       }
	       break;
	  }
	  nsets++;
	  MEMZERO((UNIV)&fpi, FPISZ);
	  if (sdparm(tbuf, &pcblitv) != LIT1) {
	       sderr(123, (UNCH *)0, (UNCH *)0);
	       return FAIL;
	  }
	  fpi.fpipubis = tbuf;
	  /* Give a warning if it is not a CHARSET fpi. */
	  if (parsefpi(&fpi))
	       sderr(E_FORMAL, (UNCH *)0, (UNCH *)0);
	  else if (fpi.fpic != FPICHARS)
	       sderr(E_BADCLASS, kcharset, (UNCH *)0);
	  else {
	       fpi.fpipubis[fpi.fpil + fpi.fpill] = '\0';
	       baseset = (int *)pmaplookup(charset_map,
					   (char *)fpi.fpipubis + fpi.fpil);
	       if (!baseset)
		    sderr(E_UNKNOWNSET, fpi.fpipubis + fpi.fpil, (UNCH *)0);
	  }
	  if (sdname(tbuf, kdescset) == FAIL) return FAIL;
	  nchars = 0;
	  for (;;) {
	       long start, count;
	       long basenum;
	       if (sdparm(tbuf, 0) != NUM1)
		    break;
	       start = atol((char *)tbuf);
	       if (sdparm(tbuf, 0) != NUM1) {
		    sderr(E_XNUM, (UNCH *)0, (UNCH *)0);
		    return FAIL;
	       }
	       count = atol((char *)tbuf);
	       switch (sdparm(tbuf, &pcblitv)) {
	       case NUM1:
		    basenum = atol((char *)tbuf);
		    break;
	       case LIT1:
		    basenum = UNKNOWN;
		    break;
	       case NAS1:
		    if (matches(tbuf, kunused)) {
			 basenum = UNUSED;
			 break;
		    }
		    /* fall through */
	       default:
		    sderr(E_CHARDESC, ltous(start), (UNCH *)0);
		    return FAIL;
	       }
	       if (start + count > 256)
		    sderr(E_CHARRANGE, (UNCH *)0, (UNCH *)0);
	       else {
		    int i;
		    int lim = (int)start + count;
		    for (i = (int)start; i < lim; i++) {
			 if (status[i] != UNDESC)
			      sderr(E_CHARDUP, ltous((long)i), (UNCH *)0);
			 else if (basenum == UNUSED || basenum == UNKNOWN)
			      status[i] = (int)basenum;
			 else if (baseset == 0)
			      status[i] = UNKNOWN_SET;
			 else {
			      int n = basenum + (i - start);
			      if (n < 0 || n > 255)
				   sderr(E_CHARRANGE, (UNCH *)0, (UNCH *)0);
			      else if (baseset[n] == UNUSED)
				   sderr(E_BADBASECHAR, ltous((long)n), (UNCH *)0);
			      else
				   status[i] = baseset[n];
			 }
		    }
	       }
	       nchars++;
	  }
	  if (nchars == 0) {
	       sderr(E_XNUM, (UNCH *)0, (UNCH *)0);
	       return FAIL;
	  }
     }
     return SUCCESS;
}

/* Parse the CAPACITY section.  Uses one token lookahead. */

static int sdcapacity(tbuf)
UNCH *tbuf;
{
     int ncap;

     if (sdckname(tbuf, kcapacity) == FAIL)
	  return FAIL;
     if (sdparm(tbuf, 0) != NAS1) {
	  sderr(120, (UNCH *)0, (UNCH *)0);
	  return FAIL;
     }
     if (matches(tbuf, kpublic))
	  return sdpubcapacity(tbuf);
     if (!matches(tbuf, ksgmlref)) {
	  sderr(E_CAPACITY, tbuf+1, (UNCH *)0);
	  return FAIL;
     }
     memcpy((UNIV)sd.capacity, (UNIV)refcapset, sizeof(sd.capacity));
     ncap = 0;
     for (;;) {
	  int capno = -1;
	  int i;

	  if (sdparm(tbuf, 0) != NAS1)
	       break;
	  for (i = 0; i < SIZEOF(captab); i++)
	       if (matches(tbuf, captab[i])) {
		    capno = i;
		    break;
	       }
	  if (capno < 0)
	       break;
	  if (sdparm(tbuf, 0) != NUM1) {
	       sderr(E_XNUM, (UNCH *)0, (UNCH *)0);
	       return FAIL;
	  }
	  sd.capacity[capno] = atol((char *)tbuf);
	  ncap++;
     }
     if (ncap == 0) {
	  sderr(E_CAPMISSING, (UNCH *)0, (UNCH *)0);
	  return FAIL;
     }

     return SUCCESS;
}

/* Parse a CAPACITY section that started with PUBLIC.  Must do one
token lookahead, since sdcapacity() also does. */

static int sdpubcapacity(tbuf)
UNCH *tbuf;
{
     UNIV ptr;
     if (sdparm(tbuf, &pcblitv) != LIT1) {
	  sderr(123, (UNCH *)0, (UNCH *)0);
	  return FAIL;
     }
     sdfixstandard(tbuf);
     ptr = pmaplookup(capset_map, (char *)tbuf);
     if (!ptr)
	  sderr(E_CAPSET, tbuf, (UNCH *)0);
     else
	  memcpy((UNIV)sd.capacity, (UNIV)ptr, sizeof(sd.capacity));
     (void)sdparm(tbuf, 0);
     return SUCCESS;
}

/* Parse the SCOPE section. Uses no lookahead. */

static int sdscope(tbuf)
UNCH *tbuf;
{
     if (sdckname(tbuf, kscope) == FAIL)
	  return FAIL;
     if (sdparm(tbuf, 0) != NAS1) {
	  sderr(120, (UNCH *)0, (UNCH *)0);
	  return FAIL;
     }
     if (matches(tbuf, kdocument))
	  ;
     else if (matches(tbuf, kinstance))
	  sderr(E_INSTANCE, (UNCH *)0, (UNCH *)0);
     else {
	  sderr(E_SCOPE, tbuf+1, (UNCH *)0);
	  return FAIL;
     }
     return SUCCESS;
}

/* Parse the SYNTAX section.  Uses one token lookahead. */

static int sdsyntax(tbuf)
UNCH *tbuf;
{
     if (sdname(tbuf, ksyntax) == FAIL) return FAIL;
     if (sdparm(tbuf, 0) != NAS1) {
	  sderr(120, (UNCH *)0, (UNCH *)0);
	  return FAIL;
     }
     if (matches(tbuf, kpublic))
	  return sdpubsyntax(tbuf);
     return sdxsyntax(tbuf);
}

/* Parse the SYNTAX section which starts with PUBLIC.  Uses one token
lookahead. */

static int sdpubsyntax(tbuf)
UNCH *tbuf;
{
     int nswitches;
     if (sdparm(tbuf, &pcblitv) != LIT1)
	  return FAIL;
     sdfixstandard(tbuf);
     if (ustrcmp(tbuf, CORE_SYNTAX) == 0)
	  sd.shortref = 0;
     else if (ustrcmp(tbuf, REFERENCE_SYNTAX) == 0)
	  sd.shortref = 1;
     else
	  sderr(E_SYNTAX, tbuf, (UNCH *)0);
     if (sdparm(tbuf, 0) != NAS1)
	  return SUCCESS;
     if (!matches(tbuf, kswitches))
	  return SUCCESS;
     nswitches = 0;
     for (;;) {
	  int errsw = 0;

	  if (sdparm(tbuf, 0) != NUM1)
	       break;
	  if (atol((char *)tbuf) > 255) {
	       sderr(E_CHARNUM, (UNCH *)0, (UNCH *)0);
	       errsw = 1;
	  }
	  if (sdparm(tbuf, 0) != NUM1) {
	       sderr(E_XNUM, (UNCH *)0, (UNCH *)0);
	       return FAIL;
	  }
	  if (!errsw) {
	       if (atol((char *)tbuf) > 255)
		    sderr(E_CHARNUM, (UNCH *)0, (UNCH *)0);
	  }
	  nswitches++;
     }
     if (nswitches == 0) {
	  sderr(E_XNUM, (UNCH *)0, (UNCH *)0);
	  return FAIL;
     }
     sderr(E_SWITCHES, (UNCH *)0, (UNCH *)0);
     return SUCCESS;
}

/* Parse an explicit concrete syntax. Uses one token lookahead. */

static
int sdxsyntax(tbuf)
UNCH *tbuf;
{
     static int (*section[]) P((UNCH *)) = {
	  sdshunchar,
	  sdsynref,
	  sdfunction,
	  sdnaming,
	  sddelim,
	  sdnames,
	  sdquantity,
     };
     int i;

     for (i = 0; i < SIZEOF(section); i++)
	  if ((*section[i])(tbuf) == FAIL)
	       return FAIL;
     return SUCCESS;
}

/* Parse the SHUNCHAR section. Uses one token lookahead. */

static
int sdshunchar(tbuf)
UNCH *tbuf;
{
     int i;
     for (i = 0; i < 256; i++)
	  char_flags[i] &= ~CHAR_SHUNNED;

     if (sdckname(tbuf, kshunchar) == FAIL)
	  return FAIL;

     if (sdparm(tbuf, 0) == NAS1) {
	  if (matches(tbuf, knone)) {
	       (void)sdparm(tbuf, 0);
	       return SUCCESS;
	  }
	  if (matches(tbuf, kcontrols)) {
	       for (i = 0; i < 256; i++)
		    if (ISASCII(i) && iscntrl(i))
			 char_flags[i] |= CHAR_SHUNNED;
	       if (sdparm(tbuf, 0) != NUM1)
		    return SUCCESS;
	  }
     }
     if (pcbsd.action != NUM1) {
	  sderr(E_XNUM, (UNCH *)0, (UNCH *)0);
	  return FAIL;
     }
     do {
	  long n = atol((char *)tbuf);
	  if (n > 255)
	       sderr(E_CHARNUM, (UNCH *)0, (UNCH *)0);
	  else
	       char_flags[(int)n] |= CHAR_SHUNNED;
     } while (sdparm(tbuf, 0) == NUM1);
     return SUCCESS;
}

/* Parse the syntax reference character set. Uses one token lookahead. */

static
int sdsynref(tbuf)
UNCH *tbuf;
{
     return sdcsdesc(tbuf, synrefcharset);
}

/* Translate a character number from the syntax reference character set
to the system character set. If it can't be done, give an error message
and return -1. */

static
int sdtranscharnum(tbuf)
UNCH *tbuf;
{
     long n = atol((char *)tbuf);
     if (n > 255) {
	  sderr(E_CHARNUM, (UNCH *)0, (UNCH *)0);
	  return -1;
     }
     return sdtranschar((int)n);
}


static
int sdtranschar(n)
int n;
{
     int ch = synrefcharset[n];
     if (ch >= 0)
	  return ch;
     switch (ch) {
     case UNUSED:
	  sderr(E_SYNREFUNUSED, ltous((long)n), (UNCH *)0);
	  break;
     case UNDESC:
	  sderr(E_SYNREFUNDESC, ltous((long)n), (UNCH *)0);
	  break;
     case UNKNOWN:
	  sderr(E_SYNREFUNKNOWN, ltous((long)n), (UNCH *)0);
	  break;
     case UNKNOWN_SET:
	  sderr(E_SYNREFUNKNOWNSET, ltous((long)n), (UNCH *)0);
	  break;
     default:
	  abort();
     }
     return -1;
}


/* Parse the function section. Uses two tokens lookahead. "NAMING"
could be a function name. */

static
int sdfunction(tbuf)
UNCH *tbuf;
{
     static UNCH *fun[] = { kre, krs, kspace };
     static int funval[] = { RECHAR, RSCHAR, ' ' };
     int i;
     int had_tab = 0;
     int changed = 0;		/* attempted to change reference syntax */

     if (sdckname(tbuf, kfunction) == FAIL)
	  return FAIL;
     for (i = 0; i < SIZEOF(fun); i++) {
	  int ch;
	  if (sdname(tbuf, fun[i]) == FAIL)
	       return FAIL;
	  if (sdparm(tbuf, 0) != NUM1) {
	       sderr(E_XNUM, (UNCH *)0, (UNCH *)0);
	       return FAIL;
	  }
	  ch = sdtranscharnum(tbuf);
	  if (ch >= 0 && ch != funval[i])
	       changed = 1;
     }
     for (;;) {
	  int tabsw = 0;
	  int namingsw = 0;
	  if (sdparm(tbuf, 0) != NAS1) {
	       sderr(120, (UNCH *)0, (UNCH *)0);
	       return FAIL;
	  }
	  if (matches(tbuf, (UNCH *)"TAB")) {
	       tabsw = 1;
	       if (had_tab)
		    sderr(E_FUNDUP, (UNCH *)0, (UNCH *)0);
	  }
	  else {
	       for (i = 0; i < SIZEOF(fun); i++)
		    if (matches(tbuf, fun[i]))
			 sderr(E_BADFUN, fun[i], (UNCH *)0);
	       if (matches(tbuf, knaming))
		    namingsw = 1;
	       else
		    changed = 1;
	  }
	  if (sdparm(tbuf, 0) != NAS1) {
	       sderr(120, (UNCH *)0, (UNCH *)0);
	       return FAIL;
	  }
	  if (namingsw) {
	       if (matches(tbuf, klcnmstrt))
		    break;
	       changed = 1;
	  }
	  if (sdparm(tbuf, 0) != NUM1) {
	       sderr(E_XNUM, (UNCH *)0, (UNCH *)0);
	       return FAIL;
	  }
	  if (tabsw && !had_tab) {
	       int ch = sdtranscharnum(tbuf);
	       if (ch >= 0 && ch != TABCHAR)
		    changed = 1;
	       had_tab = 1;
	  }

     }
     if (!had_tab)
	  changed = 1;
     if (changed)
	  sderr(E_FUNCHAR, (UNCH *)0, (UNCH *)0);
     return SUCCESS;
}

/* Parse the NAMING section.  Uses no lookahead. */

static
int sdnaming(tbuf)
UNCH *tbuf;
{
     int i;
     int bad = 0;
     static UNCH *classes[] = { klcnmstrt, kucnmstrt, klcnmchar, kucnmchar };
     static UNCH *types[] = { kgeneral, kentity };

#define NCLASSES SIZEOF(classes)

     int bufsize = 4;		/* allocated size of buf */
     UNCH *buf = (UNCH *)rmalloc(bufsize); /* holds characters
					      in naming classes */
     int bufi = 0;		/* next index into buf */
     int start[NCLASSES];	/* index of first character for each class */
     int count[NCLASSES];	/* number of characters for each class */

     for (i = 0; i < NCLASSES; i++) {
	  UNCH *s;

	  if (sdckname(tbuf, classes[i]) == FAIL) {
	       frem((UNIV)buf);
	       return FAIL;
	  }
	  if (sdparm(tbuf, &pcblitp) != LIT1) {
	       sderr(123, (UNCH *)0, (UNCH *)0);
	       frem((UNIV)buf);
	       return FAIL;
	  }
	  start[i] = bufi;

	  for (s = tbuf; *s; s++) {
	       int c = *s;
	       if (c == DELNONCH) {
		    c = UNSHIFTNON(*s);
		    s++;
	       }
	       c = sdtranschar(c);
	       if (c < 0)
		    bad = 1;
	       else if ((char_flags[c] & (CHAR_SIGNIFICANT | CHAR_MAGIC))
			&& c != '.' && c != '-') {
		    int class = lextoke[c];
		    if (class == SEP || class == SP || class == NMC
			|| class == NMS || class == NU)
			 sderr(E_NMBAD, ltous((long)c), (UNCH *)0);
		    else
			 sderr(E_NMUNSUP, ltous((long)c), (UNCH *)0);
		    bad = 1;
	       }
	       if (bufi >= bufsize)
		    buf = (UNCH *)rrealloc((UNIV)buf, bufsize *= 2);
	       buf[bufi++] = c;
	  }

	  count[i] = bufi - start[i];
	  (void)sdparm(tbuf, 0);
     }
     if (!bad && count[0] != count[1]) {
	  sderr(E_NMSTRTCNT, (UNCH *)0, (UNCH *)0);
	  bad = 1;
     }
     if (!bad && count[2] != count[3]) {
	  sderr(E_NMCHARCNT, (UNCH *)0, (UNCH *)0);
	  bad = 1;
     }
     if (!bad) {
	  nlextoke = (UNCH *)rmalloc(256);
	  memcpy((UNIV)nlextoke, lextoke, 256);
	  nlextoke['.'] = nlextoke['-'] = INV;

	  nlextran = (UNCH *)rmalloc(256);
	  memcpy((UNIV)nlextran, lextran, 256);

	  for (i = 0; i < count[0]; i++) {
	       UNCH lc = buf[start[0] + i];
	       UNCH uc = buf[start[1] + i];
	       nlextoke[lc] = NMS;
	       nlextoke[uc] = NMS;
	       nlextran[lc] = uc;
	  }

	  for (i = 0; i < count[2]; i++) {
	       UNCH lc = buf[start[2] + i];
	       UNCH uc = buf[start[3] + i];
	       if (nlextoke[lc] == NMS) {
		    sderr(E_NMDUP, ltous((long)lc), (UNCH *)0);
		    bad = 1;
	       }
	       else if (nlextoke[uc] == NMS) {
		    sderr(E_NMDUP, ltous((long)uc), (UNCH *)0);
		    bad = 1;
	       }
	       else {
		    nlextoke[lc] = NMC;
		    nlextoke[uc] = NMC;
		    nlextran[lc] = uc;
	       }
	  }
	  if (nlextoke['-'] != NMC) {
	       sderr(E_NMMINUS, (UNCH *)0, (UNCH *)0);
	       bad = 1;
	  }
	  if (bad) {
	       if (nlextoke) {
		    frem((UNIV)nlextoke);
		    nlextoke = 0;
	       }
	       if (nlextran) {
		    frem((UNIV)nlextran);
		    nlextran = 0;
	       }
	  }
     }

     frem((UNIV)buf);

     if (sdckname(tbuf, knamecase) == FAIL)
	  return FAIL;
     for (i = 0; i < SIZEOF(types); ++i) {
	  if (sdname(tbuf, types[i]) == FAIL)
	       return FAIL;
	  if (sdparm(tbuf, 0) != NAS1) {
	       sderr(120, (UNCH *)0, (UNCH *)0);
	       return FAIL;
	  }
	  if (matches(tbuf, kyes))
	       sd.namecase[i] = 1;
	  else if (matches(tbuf, kno))
	       sd.namecase[i] = 0;
	  else {
	       sderr(E_YESNO, tbuf+1, (UNCH *)0);
	       return FAIL;
	  }
     }
     return SUCCESS;
}

/* Parse the DELIM section. Uses one token lookahead. */

static
int sddelim(tbuf)
UNCH *tbuf;
{
     int changed = 0;
     if (sdname(tbuf, kdelim) == FAIL
	 || sdname(tbuf, kgeneral) == FAIL
	 || sdname(tbuf, ksgmlref) == FAIL)
	  return FAIL;
     for (;;) {
	  if (sdparm(tbuf, 0) != NAS1) {
	       sderr(120, (UNCH *)0, (UNCH *)0);
	       return FAIL;
	  }
	  if (matches(tbuf, kshortref))
	       break;
	  if (sdparm(tbuf, &pcblitp) != LIT1) {
	       sderr(123, (UNCH *)0, (UNCH *)0);
	       return FAIL;
	  }
	  changed = 1;
     }
     if (changed) {
	  sderr(E_GENDELIM, (UNCH *)0,(UNCH *)0);
	  changed = 0;
     }
     if (sdparm(tbuf, 0) != NAS1) {
	  sderr(120, (UNCH *)0, (UNCH *)0);
	  return FAIL;
     }
     if (matches(tbuf, ksgmlref))
	  sd.shortref = 1;
     else if (matches(tbuf, knone))
	  sd.shortref = 0;
     else {
	  sderr(118, tbuf+1, ksgmlref);	/* probably they forgot SGMLREF */
	  return FAIL;
     }
     while (sdparm(tbuf, &pcblitp) == LIT1)
	  changed = 1;
     if (changed)
	  sderr(E_SRDELIM, (UNCH *)0, (UNCH *)0);
     return SUCCESS;
}

/* Parse the NAMES section. Uses one token lookahead. */

static
int sdnames(tbuf)
UNCH *tbuf;
{
     int i;
     if (sdckname(tbuf, knames) == FAIL)
	  return FAIL;
     if (sdname(tbuf, ksgmlref) == FAIL)
	  return FAIL;

     while (sdparm(tbuf, 0) == NAS1) {
	  int j;
	  if (matches(tbuf, kquantity))
	       break;
	  for (i = 0; i < NKEYS; i++)
	       if (matches(tbuf, key[i]))
		    break;
	  if (i >= NKEYS) {
	       sderr(E_BADKEY, tbuf+1, (UNCH *)0);
	       return FAIL;
	  }
	  if (sdparm(tbuf, &pcblitp) != NAS1) {
	       sderr(120, (UNCH *)0, (UNCH *)0);
	       return FAIL;
	  }
	  if (!newkey) {
	       newkey = (UNCH (*)[REFNAMELEN+1])rmalloc((REFNAMELEN+1)*NKEYS);
	       MEMZERO((UNIV)newkey, (REFNAMELEN+1)*NKEYS);
	  }
	  for (j = 0; j < NKEYS; j++) {
	       if (matches(tbuf, key[j])) {
		    sderr(E_REFNAME, tbuf + 1, (UNCH *)0);
		    break;
	       }
	       if (matches(tbuf, newkey[j])) {
		    sderr(E_DUPNAME, tbuf + 1, (UNCH *)0);
		    break;
	       }
	  }
	  if (j >= NKEYS)
	       ustrcpy(newkey[i], tbuf + 1);
     }
     /* Now install the new keys. */
     if (newkey) {
	  for (i = 0; i < NKEYS; i++)
	       if (newkey[i][0] != '\0') {
		    UNCH temp[REFNAMELEN + 1];

		    ustrcpy(temp, key[i]);
		    ustrcpy(key[i], newkey[i]);
		    ustrcpy(newkey[i], temp);
	       }
     }
     return SUCCESS;
}

/* Parse the QUANTITY section. Uses one token lookahead. */

static int sdquantity(tbuf)
UNCH *tbuf;
{
     int quantity[NQUANTITY];
     int i;

     for (i = 0; i < NQUANTITY; i++)
	  quantity[i] = -1;
     if (sdckname(tbuf, kquantity) == FAIL)
	  return FAIL;
     if (sdname(tbuf, ksgmlref) == FAIL)
	  return FAIL;
     while (sdparm(tbuf, 0) == NAS1 && !matches(tbuf, kfeatures)) {
	  long n;
	  for (i = 0; i < SIZEOF(quantity_names); i++)
	       if (matches(tbuf, quantity_names[i]))
		    break;
	  if (i >= SIZEOF(quantity_names)) {
	       sderr(E_BADQUANTITY, tbuf + 1, (UNCH *)0);
	       return FAIL;
	  }
	  if (sdparm(tbuf, 0) != NUM1) {
	       sderr(E_XNUM, (UNCH *)0, (UNCH *)0);
	       return FAIL;
	  }
	  n = atol((char *)tbuf);
	  if (n < sd.quantity[i])
	       sderr(E_QUANTITY, (UNCH *)quantity_names[i],
		     ltous((long)sd.quantity[i]));
	  else if (n > max_quantity[i]) {
	       sderr(E_QTOOBIG, (UNCH *)quantity_names[i],
		     ltous((long)max_quantity[i]));
	       quantity[i] = max_quantity[i];
	  }
	  else
	       quantity[i] = (int)n;
     }
     for (i = 0; i < NQUANTITY; i++)
	  if (quantity[i] > 0) {
	       sd.quantity[i] = quantity[i];
	       if (!quantity_changed)
		    quantity_changed = (char *)rmalloc(NQUANTITY);
	       quantity_changed[i] = 1;
	  }
     return SUCCESS;
}

/* Parse the FEATURES section.  Uses no lookahead. */

static int sdfeatures(tbuf)
UNCH *tbuf;
{
     static struct  {
	  UNCH *name;
	  UNCH argtype;  /* 0 = no argument, 1 = boolean, 2 = numeric */
	  UNIV valp;     /* UNCH * if boolean, long * if numeric. */
     } features[] = {
	  { kminimize, 0, 0 },
	  { kdatatag, 1, 0 },
	  { komittag, 1, (UNIV)&sd.omittag },
	  { krank, 1, 0 },
	  { kshorttag, 1, (UNIV)&sd.shorttag },
	  { klink, 0, 0 },
	  { ksimple, 2, 0 },
	  { kimplicit, 1, 0 },
	  { kexplicit, 2, 0 },
	  { kother, 0, 0 },
	  { kconcur, 2, 0 },
	  { ksubdoc, 2, (UNIV)&sd.subdoc },
	  { kformal, 1, (UNIV)&sd.formal },
     };

     int i;

     if (sdckname(tbuf, kfeatures) == FAIL)
	  return FAIL;
     for (i = 0; i < SIZEOF(features); i++) {
	  if (sdname(tbuf, features[i].name) == FAIL) return FAIL;
	  if (features[i].argtype > 0) {
	       long n;
	       if (sdparm(tbuf, 0) != NAS1) {
		    sderr(120, (UNCH *)0, (UNCH *)0);
		    return FAIL;
	       }
	       if (matches(tbuf, kyes)) {
		    if (features[i].argtype > 1) {
			 if (sdparm(tbuf, 0) != NUM1) {
			      sderr(E_XNUM, (UNCH *)0, (UNCH *)0);
			      return FAIL;
			 }
			 n = atol((char *)tbuf);
			 if (n == 0)
			      sderr(E_ZEROFEATURE, features[i].name, (UNCH *)0);
		    }
		    else
			 n = 1;
	       }
	       else if (matches(tbuf, kno))
		    n = 0;
	       else {
		    sderr(E_YESNO, tbuf+1, (UNCH *)0);
		    return FAIL;
	       }
	       if (features[i].valp == 0) {
		    if (n > 0)
			 sderr(E_NOTSUPPORTED, features[i].name,
			      (UNCH *)0);
	       }
	       else if (features[i].argtype > 1)
		    *(long *)features[i].valp = n;
	       else
		    *(UNCH *)features[i].valp = (UNCH)n;
	  }
     }
     if (!sd.shorttag)
	  noemptytag();
     return SUCCESS;
}

/* Parse the APPINFO section.  Uses no lookahead. */

static int sdappinfo(tbuf)
UNCH *tbuf;
{
     if (sdname(tbuf, kappinfo) == FAIL) return FAIL;
     switch (sdparm(tbuf, &pcblitv)) {
     case LIT1:
	  appinfosw = 1;
	  break;
     case NAS1:
	  if (matches(tbuf, knone))
	       break;
	  sderr(118, tbuf+1, knone);
	  return FAIL;
     default:
	  sderr(E_XNMLIT, knone, (UNCH *)0);
	  return FAIL;
     }
     return SUCCESS;
}

/* Change a prefix of ISO 8879-1986 to ISO 8879:1986.  Amendment 1 to
the standard requires the latter. */

static VOID sdfixstandard(tbuf)
UNCH *tbuf;
{
     if (strncmp((char *)tbuf, "ISO 8879-1986", 13) == 0) {
	  sderr(E_STANDARD, (UNCH *)0, (UNCH *)0);
	  tbuf[8] = ':';
     }
}

static int sdname(tbuf, key)
UNCH *tbuf;
UNCH *key;
{
     if (sdparm(tbuf, 0) != NAS1) {
	  sderr(120, (UNCH *)0, (UNCH *)0);
	  return FAIL;
     }
     if (!matches(tbuf, key)) {
	  sderr(118, tbuf+1, key);
	  return FAIL;
     }
     return SUCCESS;
}

static int sdckname(tbuf, key)
UNCH *tbuf;
UNCH *key;
{
     if (pcbsd.action != NAS1) {
	  sderr(120, (UNCH *)0, (UNCH *)0);
	  return FAIL;
     }
     if (!matches(tbuf, key)) {
	  sderr(118, tbuf+1, key);
	  return FAIL;
     }
     return SUCCESS;
}

/* Parse a SGML declaration parameter.  If lpcb is NULL, pt must be
REFNAMELEN+2 characters long, otherwise at least LITLEN+2 characters
long. LPCB should be NULL if a literal is not allowed. */

static int sdparm(pt, lpcb)
UNCH *pt;			/* Token buffer. */
struct parse *lpcb;		/* PCB for literal parse. */
{
     for (;;) {
	  parse(&pcbsd);
	  if (pcbsd.action != ISIG)
	       break;
	  sderr(E_SIGNIFICANT, (UNCH *)0, (UNCH *)0);
     }
     ++parmno;
     switch (pcbsd.action) {
     case LIT1:
	  if (!lpcb) {
	       sderr(E_BADLIT, (UNCH *)0, (UNCH *)0);
	       REPEATCC;
	       return pcbsd.action = INV_;
	  }
	  parselit(pt, lpcb, REFLITLEN, lex.d.lit);
	  return pcbsd.action;
     case LIT2:
	  if (!lpcb) {
	       sderr(E_BADLIT, (UNCH *)0, (UNCH *)0);
	       REPEATCC;
	       return pcbsd.action = INV_;
	  }
	  parselit(pt, lpcb, REFLITLEN, lex.d.lita);
	  return pcbsd.action = LIT1;
     case NAS1:
	  parsenm(pt, 1);
	  return pcbsd.action;
     case NUM1:
	  parsetkn(pt, NU, REFNAMELEN);
	  return pcbsd.action;
     }
     return pcbsd.action;
}

VOID sdinit()
{
     int i;
     /* Shunned character numbers in the reference concrete syntax. */
     static UNCH refshun[] = {
	  0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18,
	  19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 127, 255
	  };
     UNCH **p;
     /* A character is magic if it is a non-SGML character used for
     some internal purpose in the parser. */
     char_flags[EOS] |= CHAR_MAGIC;
     char_flags[EOBCHAR] |= CHAR_MAGIC;
     char_flags[EOFCHAR] |= CHAR_MAGIC;
     char_flags[GENRECHAR] |= CHAR_MAGIC;
     char_flags[DELNONCH] |= CHAR_MAGIC;
     char_flags[DELCDATA] |= CHAR_MAGIC;
     char_flags[DELSDATA] |= CHAR_MAGIC;

     /* Figure out the significant SGML characters. */
     for (p = lextabs; *p; p++) {
	  UNCH datclass = (*p)[CANON_DATACHAR];
	  UNCH nonclass = (*p)[CANON_NONSGML];
	  for (i = 0; i < 256; i++)
	       if (!(char_flags[i] & CHAR_MAGIC)
		   && (*p)[i] != datclass && (*p)[i] != nonclass)
		    char_flags[i] |= CHAR_SIGNIFICANT;
     }
     for (i = 0; i < SIZEOF(refshun); i++)
	  char_flags[refshun[i]] |= CHAR_SHUNNED;
     for (i = 0; i < 256; i++)
	  if (ISASCII(i) && iscntrl(i))
	       char_flags[i] |= CHAR_SHUNNED;
     bufsalloc();
}


static
VOID bufsalloc()
{
     scbs = (struct source *)rmalloc((REFENTLVL+1)*sizeof(struct source));
     tbuf = (UNCH *)rmalloc(REFATTSPLEN+REFLITLEN+1);
     /* entbuf is used for parsing numeric character references */
     entbuf = (UNCH *)rmalloc(REFNAMELEN + 2);
}

static
VOID bufsrealloc()
{
     UNS size;

     if (ENTLVL != REFENTLVL)
	  scbs = (struct source *)rrealloc((UNIV)scbs,
					   (ENTLVL+1)*sizeof(struct source));
     /* Calculate the size for tbuf. */
     size = LITLEN + ATTSPLEN;
     if (PILEN > size)
	  size = PILEN;
     if (BSEQLEN > size)
	  size = BSEQLEN;
     if (size != REFATTSPLEN + REFLITLEN)
	  tbuf = (UNCH *)rrealloc((UNIV)tbuf, size + 1);
     if (NAMELEN != REFNAMELEN)
	  entbuf = (UNCH *)rrealloc((UNIV)entbuf, NAMELEN + 2);
}


/* Check that the non-SGML characters are compatible with the concrete
syntax and munge the lexical tables accordingly.  If IMPLIED is
non-zero, then the SGML declaration was implied; in this case, don't
give error messages about shunned characters not being declared
non-SGML.  Also make any changes that are required by the NAMING section.
*/

static VOID setlexical()
{
     int i;
     UNCH **p;

     if (nlextoke) {
	  /* Handle characters that were made significant by the
	     NAMING section. */
	  for (i = 0; i < 256; i++)
	       if (nlextoke[i] == NMC || nlextoke[i] == NMS)
		    char_flags[i] |= CHAR_SIGNIFICANT;
     }

     for (i = 0; i < 256; i++)
	  if (char_flags[i] & CHAR_SIGNIFICANT) {
	       /* Significant SGML characters musn't be non-SGML. */
	       if (char_flags[i] & CHAR_NONSGML) {
		    UNCH buf[2];
		    buf[0] = i;
		    buf[1] = '\0';
		    sderr(E_NONSGML, buf, (UNCH *)0);
		    char_flags[i] &= ~CHAR_NONSGML;
	       }
	  }
	  else {
	       /* Shunned characters that are not significant SGML characters
		  must be non-SGML. */
	       if ((char_flags[i] & (CHAR_SHUNNED | CHAR_NONSGML))
		   == CHAR_SHUNNED) {
		   sderr(E_SHUNNED, ltous((long)i), (UNCH *)0);
		   char_flags[i] |= CHAR_NONSGML;
	       }
	  }


     /* Now munge the lexical tables. */
     for (p = lextabs; *p; p++) {
	  UNCH nonclass = (*p)[CANON_NONSGML];
	  UNCH datclass = (*p)[CANON_DATACHAR];
	  UNCH nmcclass = (*p)[CANON_NMC];
	  UNCH nmsclass = (*p)[CANON_NMS];
	  UNCH minclass = (*p)[CANON_MIN];
	  for (i = 0; i < 256; i++) {
	       if (char_flags[i] & CHAR_NONSGML) {
		    /* We already know that it's not significant. */
		    if (!(char_flags[i] & CHAR_MAGIC))
			 (*p)[i] = nonclass;
	       }
	       else {
		    if (char_flags[i] & CHAR_MAGIC) {
			 sderr(E_MUSTBENON, ltous((long)i), (UNCH *)0);
		    }
		    else if (!(char_flags[i] & CHAR_SIGNIFICANT))
			 (*p)[i] = datclass;
		    else if (nlextoke
			     /* This relies on the fact that lextoke
				occurs last in lextabs. */
			     && lextoke[i] != nlextoke[i]) {
			 switch (nlextoke[i]) {
			 case NMC:
			      (*p)[i] = nmcclass;
			      break;
			 case NMS:
			      (*p)[i] = nmsclass;
			      break;
			 case INV:
			      /* This will happen if period is not a
				 name character. */
			      (*p)[i] = minclass;
			      break;
			 default:
			      abort();
			 }
		    }
	       }
	  }
     }
     if (nlextran) {
	  memcpy((UNIV)lextran, (UNIV)nlextran, 256);
	  frem((UNIV)nlextran);
     }
     if (nlextoke) {
	  frem((UNIV)nlextoke);
	  nlextoke = 0;
     }

}

/* Munge parse tables so that empty start and end tags are not recognized. */

static VOID noemptytag()
{
     static struct parse *pcbs[] = { &pcbconm, &pcbcone, &pcbconr, &pcbconc };
     int i;

     for (i = 0; i < SIZEOF(pcbs); i++) {
	  int maxclass, maxstate;
	  int j, k, act;
	  UNCH *plex = pcbs[i]->plex;
	  UNCH **ptab = pcbs[i]->ptab;

	  /* Figure out the maximum lexical class. */
	  maxclass = 0;
	  for (j = 0; j < 256; j++)
	       if (plex[j] > maxclass)
		    maxclass = plex[j];

	  /* Now figure out the maximum state number and at the same time
	     change actions. */

	  maxstate = 0;

	  for (j = 0; j <= maxstate; j += 2) {
	       for (k = 0; k <= maxclass; k++)
		    if (ptab[j][k] > maxstate)
			 maxstate = ptab[j][k];
	       /* If the '>' class has an empty start or end tag action,
		  change it to the action that the NMC class has. */
	       act = ptab[j + 1][plex['>']];
	       if (act == NET_ || act == NST_)
		    ptab[j + 1][plex['>']] = ptab[j + 1][plex['_']];
	  }
     }
}

/* Lookup the value of the entry in pmap PTR whose key is KEY. */

static UNIV pmaplookup(ptr, key)
struct pmap *ptr;
char *key;
{
     for (; ptr->name; ptr++)
	  if (strcmp(key, ptr->name) == 0)
	       return ptr->value;
     return 0;
}

/* Return an ASCII representation of N. */

static UNCH *ltous(n)
long n;
{
     static char buf[sizeof(long)*3 + 2];
     sprintf(buf, "%ld", n);
     return (UNCH *)buf;
}

VOID sgmlwrsd(fp)
FILE *fp;
{
     int i;
     int changed;
     char *p;
     char uc[256];		/* upper case characters (with different lower
				   case characters) */
     char lcletter[256];	/* LC letters: a-z */

     fprintf(fp, "<!SGML \"%s\"\n", standard);
     fprintf(fp, "CHARSET\nBASESET \"%s//CHARSET %s//%s\"\nDESCSET\n",
	     SYSTEM_CHARSET_OWNER,
	     SYSTEM_CHARSET_DESCRIPTION,
	     SYSTEM_CHARSET_DESIGNATING_SEQUENCE);

     if (!done_nonsgml) {
	  done_nonsgml = 1;
	  for (i = 0; i < 256; i++)
	       if ((char_flags[i] & (CHAR_SIGNIFICANT | CHAR_SHUNNED))
		   == CHAR_SHUNNED)
	            char_flags[i] |= CHAR_NONSGML;
     }
     i = 0;
     while (i < 256) {
	  int j;
	  for (j = i + 1; j < 256; j++)
	       if ((char_flags[j] & CHAR_NONSGML)
		   != (char_flags[i] & CHAR_NONSGML))
		    break;
	  if (char_flags[i] & CHAR_NONSGML)
	       fprintf(fp, "%d %d UNUSED\n", i, j - i);
	  else
	       fprintf(fp, "%d %d %d\n", i, j - i, i);
	  i = j;
     }
     fprintf(fp, "CAPACITY\n");
     changed = 0;
     for (i = 0; i < NCAPACITY; i++)
	  if (refcapset[i] != sd.capacity[i]) {
	       if (!changed) {
		    fprintf(fp, "SGMLREF\n");
		    changed = 1;
	       }
	       fprintf(fp, "%s %ld\n", captab[i], sd.capacity[i]);
	  }
     if (!changed)
	  fprintf(fp, "PUBLIC \"%s\"\n", capset_map[0].name);
     fprintf(fp, "SCOPE DOCUMENT\n");

     fprintf(fp, "SYNTAX\nSHUNCHAR");
     for (i = 0; i < 256; i++)
	  if (char_flags[i] & CHAR_SHUNNED)
	       fprintf(fp, " %d", i);
     fprintf(fp, "\n");
     fprintf(fp, "BASESET \"%s//CHARSET %s//%s\"\nDESCSET 0 256 0\n",
	     SYSTEM_CHARSET_OWNER,
	     SYSTEM_CHARSET_DESCRIPTION,
	     SYSTEM_CHARSET_DESIGNATING_SEQUENCE);

     fprintf(fp, "FUNCTION\nRE 13\nRS 10\nSPACE 32\nTAB SEPCHAR 9\n");

     MEMZERO((UNIV)uc, 256);
     for (i = 0; i < 256; i++)
	  if (lextran[i] != i)
	       uc[lextran[i]] = 1;

     MEMZERO((UNIV)lcletter, 256);
     for (p = "abcdefghijklmnopqrstuvwxyz"; *p; p++)
	  lcletter[(unsigned char)*p]= 1;

     fprintf(fp, "NAMING\n");
     fputs("LCNMSTRT \"", fp);
     for (i = 0; i < 256; i++)
	  if (lextoke[i] == NMS && !uc[i] && !lcletter[i])
	       fprintf(fp, "&#%d;", i);
     fputs("\"\n", fp);
     fputs("UCNMSTRT \"", fp);
     for (i = 0; i < 256; i++)
	  if (lextoke[i] == NMS && !uc[i] && !lcletter[i])
	       fprintf(fp, "&#%d;", lextran[i]);
     fputs("\"\n", fp);
     fputs("LCNMCHAR \"", fp);
     for (i = 0; i < 256; i++)
	  if (lextoke[i] == NMC && !uc[i])
	       fprintf(fp, "&#%d;", i);
     fputs("\"\n", fp);
     fputs("UCNMCHAR \"", fp);
     for (i = 0; i < 256; i++)
	  if (lextoke[i] == NMC && !uc[i])
	       fprintf(fp, "&#%d;", lextran[i]);
     fputs("\"\n", fp);

     fprintf(fp, "NAMECASE\nGENERAL %s\nENTITY %s\n",
	     sd.namecase[0] ? "YES" : "NO",
	     sd.namecase[1] ? "YES" : "NO");
     fprintf(fp, "DELIM\nGENERAL SGMLREF\nSHORTREF %s\n",
	     sd.shortref ? "SGMLREF" : "NONE");
     fprintf(fp, "NAMES SGMLREF\n");
     if (newkey) {
	  /* The reference key was saved in newkey. */
	  for (i = 0; i < NKEYS; i++)
	       if (newkey[i][0])
		    fprintf(fp, "%s %s\n", newkey[i], key[i]);
     }
     fprintf(fp, "QUANTITY SGMLREF\n");
     if (quantity_changed)
	  for (i = 0; i < NQUANTITY; i++)
	       if (quantity_changed[i])
		    fprintf(fp, "%s %d\n", quantity_names[i], sd.quantity[i]);
     fprintf(fp,
	     "FEATURES\nMINIMIZE\nDATATAG NO OMITTAG %s RANK NO SHORTTAG %s\n",
	     sd.omittag ? "YES" : "NO",
	     sd.shorttag ? "YES" : "NO");
     fprintf(fp, "LINK SIMPLE NO IMPLICIT NO EXPLICIT NO\n");
     fprintf(fp, "OTHER CONCUR NO ");
     if (sd.subdoc > 0)
	  fprintf(fp, "SUBDOC YES %ld ", sd.subdoc);
     else
	  fprintf(fp, "SUBDOC NO ");
     fprintf(fp, "FORMAL %s\n", sd.formal ? "YES" : "NO");
     fprintf(fp, "APPINFO NONE");
     fprintf(fp, ">\n");
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
