/* lineout.c -
   Implements line-oriented output format.

     Written by James Clark (jjc@jclark.com).
*/

#include "config.h"
#include "std.h"
#include "entity.h"           /* Templates for entity control blocks. */
#include "adl.h"              /* Definitions for attribute list processing. */
#include "sgmlmain.h"         /* Main interface to SGML services. */
#include "lineout.h"
#include "appl.h"

static VOID flush_data P((void));
static VOID define_external_entity P((PNE));
static VOID define_entity P((UNCH *));
static VOID handle_attributes P((UNCH *, struct ad *));
static VOID handle_token_list P((UNCH *, struct ad *, int));
static VOID handle_single_token P((UNCH *, struct ad *, int));
static VOID output_notation P((UNCH *, UNCH *, UNCH *));
static VOID output_internal_entity P((UNCH *, int, UNCH *));
static VOID output_external_entity P((UNCH *, int, UNIV, UNCH *, UNCH *,
				      UNCH *));
static VOID output_subdoc P((UNCH *, UNIV, UNCH *, UNCH *));
#ifdef SUPPORT_SUBDOC
static VOID process_subdoc P((UNCH *, UNIV));
#endif /* SUPPORT_SUBDOC */
static VOID output_record_end P((void));
static VOID output_pcdata P((UNS, UNCH *));
static VOID output_cdata P((UNS, UNCH *));
static VOID output_sdata P((UNS, UNCH *));
static VOID output_entity_reference P((UNCH *));
static VOID output_start_tag P((UNCH *));
static VOID output_end_tag P((UNCH *));
static VOID output_processing_instruction P((UNS, UNCH *));
static VOID output_implied_attribute P((UNCH *, UNCH *));
static char *attribute_type_string P((int));
static VOID output_begin_attribute P((UNCH *, UNCH *, int));
static VOID output_attribute_token P((UNS, UNCH *));
static VOID output_end_attribute P((void));
static VOID print_data P((UNS, UNCH *, int));
static VOID print_string P((UNS, UNCH *, int));
static VOID print_id P((UNIV, UNCH *, UNCH *));
static VOID print_filename P((char *));
static VOID output_location P((void));
static VOID output_appinfo P((UNS, UNCH *));

static int have_data = 0;
static char *current_filename = 0;
static unsigned long current_lineno = 0;

VOID process_document(subdocsw)
int subdocsw;
{
     enum sgmlevent rc;
     struct rcbtag rcbtag;
     struct rcbdata rcbdaf;

     while ((rc = sgmlnext(&rcbdaf, &rcbtag)) != SGMLEOD) {
#ifdef SUPPORT_SUBDOC
	  if (rc == SGMLDAF && !CONTERSW(rcbdaf) && NDESW(rcbdaf)
	      && NEXTYPE(NEPTR(rcbdaf)) == ESNSUB) {
	       if (!suppsw && !sgmlment(NEENAME(NEPTR(rcbdaf))))
		    define_external_entity(NEPTR(rcbdaf));
	       process_subdoc(NEENAME(NEPTR(rcbdaf)) + 1,
			      NEID(NEPTR(rcbdaf)));
	       continue;
	  }
#endif /* SUPPORT_SUBDOC */
	  if (!suppsw)
	       switch (rc) {
	       case SGMLDAF:
		    if (CONTERSW(rcbdaf))
			 break;
		    if (CDESW(rcbdaf))
			 output_cdata(CDATALEN(rcbdaf), CDATA(rcbdaf));
		    else if (SDESW(rcbdaf))
			 output_sdata(CDATALEN(rcbdaf), CDATA(rcbdaf));
		    else if (NDESW(rcbdaf)) {
			 assert(NEXTYPE(NEPTR(rcbdaf)) != ESNSUB);
			 if (!sgmlment(NEENAME(NEPTR(rcbdaf))))
			      define_external_entity(NEPTR(rcbdaf));
			 output_entity_reference(NEENAME(NEPTR(rcbdaf)) + 1);
		    }
		    else
			 output_pcdata(CDATALEN(rcbdaf), CDATA(rcbdaf));
		    break;
	       case SGMLSTG:
		    if (CONTERSW(rcbtag))
			 break;
		    if (ALPTR(rcbtag))
			 handle_attributes((UNCH *)NULL, ALPTR(rcbtag));
		    output_start_tag(CURGI(rcbtag));
		    break;
	       case SGMLETG:
		    if (CONTERSW(rcbtag))
			 break;
		    output_end_tag(CURGI(rcbtag));
		    break;
	       case SGMLPIS:
		    if (CONTERSW(rcbdaf))
			 break;
		    output_processing_instruction(PDATALEN(rcbdaf),
						  PDATA(rcbdaf));
		    break;
	       case SGMLREF:
		    if (CONTERSW(rcbdaf))
			 break;
		    output_record_end();
		    break;
	       case SGMLAPP:
		    if (CONTERSW(rcbdaf))
			 break;
		    if (!subdocsw)
			 output_appinfo(ADATALEN(rcbdaf), ADATA(rcbdaf));
		    break;
	       default:
		    abort();
	       }
     }
}

/* Output an indication that the document was conforming. */

VOID output_conforming()
{
     if (!suppsw)
	  printf("%c\n", CONFORMING_CODE);
}

static VOID define_external_entity(p)
PNE p;
{
     if (NEXTYPE(p) == ESNSUB)
	  output_subdoc(NEENAME(p) + 1, NEID(p), NEPUBID(p), NESYSID(p));
     else {
	  if (!NEDCNMARK(p))
	       output_notation(NEDCN(p) + 1, NEDCNPUBID(p), NEDCNSYSID(p));
	  output_external_entity(NEENAME(p) + 1, NEXTYPE(p), NEID(p),
				 NEPUBID(p), NESYSID(p), NEDCN(p) + 1);
	  if (NEAL(p))
	       handle_attributes(NEENAME(p) + 1, NEAL(p));
     }
}

static VOID define_entity(ename)
UNCH *ename;
{
     int rc;
     PNE np;
     UNCH *tp;

     if (sgmlment(ename))		/* already defined it */
	  return;
     rc = sgmlgent(ename, &np, &tp);
     switch (rc) {
     case 1:
	  define_external_entity(np);
	  break;
     case 2:
     case 3:
	  output_internal_entity(ename + 1, rc == 3, tp);
	  break;
     }
}

/* ENT is the name of the entity with which these attributes are associated;
if it's NULL, they're associated with the next start tag. */

static VOID handle_attributes(ent, al)
UNCH *ent;
struct ad *al;
{
     int aln;

     for (aln = 1; aln <= ADN(al); aln++) {
	  if (GET(ADFLAGS(al, aln), AERROR))
	       ;
	  else if (GET(ADFLAGS(al, aln), AINVALID))
	       ;
	  else if (ADVAL(al, aln) == NULL)
	       output_implied_attribute(ent, ADNAME(al, aln));
	  else if (ADTYPE(al, aln) >= ATKNLIST)
	       handle_token_list(ent, al, aln);
	  else
	       handle_single_token(ent, al, aln);
	  if (BITON(ADFLAGS(al, aln), AGROUP))
	       aln += ADNUM(al, aln);
     }
}

static VOID handle_token_list(ent, al, aln)
UNCH *ent;
struct ad *al;
int aln;
{
     UNCH *ptr;
     int i;
     if (ADTYPE(al, aln) == AENTITYS) {
	  ptr = ADVAL(al, aln);
	  for (i = 0; i < ADNUM(al, aln); i++) {
	       /* Temporarily make token look like normal
		  name with length and EOS. */
	       UNCH c = ptr[*ptr + 1];
	       ptr[*ptr + 1] = '\0';
	       *ptr += 2;
	       define_entity(ptr);
	       *ptr -= 2;
	       ptr += *ptr + 1;
	       *ptr = c;
	  }
     }
     output_begin_attribute(ent, ADNAME(al, aln), ADTYPE(al, aln));
     ptr = ADVAL(al, aln);
     for (i = 0; i < ADNUM(al, aln); i++) {
	  /* The first byte is a length NOT including the length
	     byte; the tokens are not EOS terminated. */
	  output_attribute_token(*ptr, ptr + 1);
	  ptr += *ptr + 1;
     }
     output_end_attribute();
}

static VOID handle_single_token(ent, al, aln)
UNCH *ent;
struct ad *al;
int aln;
{
     if (ADTYPE(al, aln) == ANOTEGRP && !DCNMARK(ADDATA(al, aln).x))
	  output_notation(ADVAL(al, aln) + 1,
			  ADDATA(al, aln).x->pubid,
			  ADDATA(al, aln).x->sysid);
     else if (ADTYPE(al, aln) == AENTITY)
	  define_entity(ADVAL(al, aln));
     output_begin_attribute(ent, ADNAME(al, aln), ADTYPE(al, aln));
     if (ADTYPE(al, aln) == ACHARS)
	  output_attribute_token(ustrlen(ADVAL(al, aln)), ADVAL(al, aln));
     else
	  output_attribute_token(*ADVAL(al, aln) - 2, ADVAL(al, aln) + 1);
     output_end_attribute();
}

static VOID output_notation(name, pubid, sysid)
UNCH *name;
UNCH *pubid, *sysid;
{
     flush_data();
     print_id((UNIV)0, pubid, sysid);
     printf("%c%s\n", DEFINE_NOTATION_CODE, name);
}

static VOID output_internal_entity(ename, is_sdata, text)
UNCH *ename;
int is_sdata;
UNCH *text;
{
     flush_data();
     printf("%c%s %s ", DEFINE_INTERNAL_ENTITY_CODE, ename,
	    is_sdata ? "SDATA" : "CDATA");
     print_string(text ? ustrlen(text) : 0, text, 0);
     putchar('\n');
}

static VOID output_subdoc(nm, id, pubid, sysid)
UNCH *nm;
UNIV id;
UNCH *pubid, *sysid;
{
     flush_data();
     print_id(id, pubid, sysid);
     printf("%c%s\n", DEFINE_SUBDOC_ENTITY_CODE, nm);
}

#ifdef SUPPORT_SUBDOC

static VOID process_subdoc(nm, id)
UNCH *nm;
UNIV id;
{
     if (!suppsw) {
	  flush_data();
	  output_location();
	  printf("%c%s\n", START_SUBDOC_CODE, nm);
	  fflush(stdout);
     }
     fflush(stderr);

     if (id) {
	  char **argv;
	  int ret;

	  argv = make_argv(id);
	  ret = run_process(argv);
	  if (ret != 0)
	       suberr++;

	  current_filename = 0;
	  free(argv);
	  if (ret == 0)
	       get_subcaps();
     }
     else {
	  suberr++;
	  appl_error(E_SUBDOC, nm);
     }

     if (!suppsw)
	  printf("%c%s\n", END_SUBDOC_CODE, nm);
}

#endif /* SUPPORT_SUBDOC */

static VOID output_external_entity(nm, xtype, id, pubid, sysid, dcn)
UNCH *nm, *dcn;
UNIV id;
UNCH *pubid, *sysid;
int xtype;
{
     char *type;

     flush_data();

     print_id(id, pubid, sysid);

     switch (xtype) {
     case ESNCDATA:
	  type = "CDATA";
	  break;
     case ESNNDATA:
	  type = "NDATA";
	  break;
     case ESNSDATA:
	  type = "SDATA";
	  break;
     default:
	  return;
     }
     printf("%c%s %s %s\n", DEFINE_EXTERNAL_ENTITY_CODE, nm, type, dcn);
}

static VOID output_record_end()
{
     static UNCH re = RECHAR;
     print_data(1, &re, 0);
}

static VOID output_pcdata(n, s)
UNS n;
UNCH *s;
{
     print_data(n, s, 0);
}

static VOID output_cdata(n, s)
UNS n;
UNCH *s;
{
     print_data(n, s, 0);
}

static VOID output_sdata(n, s)
UNS n;
UNCH *s;
{
     print_data(n, s, 1);
}

static VOID output_entity_reference(s)
UNCH *s;
{
     flush_data();
     output_location();
     printf("%c%s\n", REFERENCE_ENTITY_CODE, s);
}

static VOID output_start_tag(s)
UNCH *s;
{
     flush_data();
     output_location();
     printf("%c%s\n", START_CODE, s);
}

static VOID output_end_tag(s)
UNCH *s;
{
     flush_data();
     printf("%c%s\n", END_CODE, s);
}

static VOID output_processing_instruction(n, s)
UNS n;
UNCH *s;
{
     flush_data();
     output_location();
     putchar(PI_CODE);
     print_string(n, s, 0);
     putchar('\n');
}

static VOID output_appinfo(n, s)
UNS n;
UNCH *s;
{
     flush_data();
     output_location();
     putchar(APPINFO_CODE);
     print_string(n, s, 0);
     putchar('\n');
}


static VOID output_implied_attribute(ent, aname)
UNCH *ent, *aname;
{
     flush_data();
     if (ent)
	  printf("%c%s %s IMPLIED\n", DATA_ATTRIBUTE_CODE, ent, aname);
     else
	  printf("%c%s IMPLIED\n", ATTRIBUTE_CODE, aname);
}

static char *attribute_type_string(type)
int type;
{
     switch (type) {
     case ANMTGRP:
     case ANAME:
     case ANMTOKE:
     case ANUTOKE:
     case ANUMBER:
     case ANAMES:
     case ANMTOKES:
     case ANUTOKES:
     case ANUMBERS:
     case AID:
     case AIDREF:
     case AIDREFS:
	  return "TOKEN";
     case ANOTEGRP:
	  return "NOTATION";
     case ACHARS:
	  return "CDATA";
     case AENTITY:
     case AENTITYS:
	  return "ENTITY";
     }
#if 0
     fatal("invalid attribute type %d", type);
#endif
     return "INVALID";
}

static VOID output_begin_attribute(ent, aname, type)
UNCH *ent, *aname;
int type;
{
     flush_data();
     if (ent)
	  printf("%c%s %s %s", DATA_ATTRIBUTE_CODE, ent, aname,
		 attribute_type_string(type));
     else
	  printf("%c%s %s", ATTRIBUTE_CODE, aname,
		 attribute_type_string(type));

}

static VOID output_attribute_token(vallen, val)
UNS vallen;
UNCH *val;
{
     putchar(' ');
     print_string(vallen, val, 0);
}

static VOID output_end_attribute()
{
     putchar('\n');
}

static VOID print_data(n, s, is_sdata)
UNS n;
UNCH *s;
int is_sdata;
{
     if (n > 0 || is_sdata) {
	  if (n == 1 && *s == RECHAR)
	       current_lineno++;
	  else
	       output_location();
	  if (!have_data)
	       putchar(DATA_CODE);
	  print_string(n, s, is_sdata);
	  have_data = 1;
     }
}

static VOID flush_data()
{
     if (have_data) {
	  putchar('\n');
	  have_data = 0;
     }
}

static VOID output_location()
{
     char *filename;
     unsigned long lineno;
     int filename_changed = 0;

     if (!locsw)
	  return;
     if (!sgmlloc(&lineno, &filename))
	  return;
     if (!current_filename || strcmp(filename, current_filename) != 0)
	  filename_changed = 1;
     else if (lineno == current_lineno)
	  return;
     flush_data();
     printf("%c%lu", LOCATION_CODE, lineno);
     current_lineno = lineno;
     if (filename_changed) {
	  putchar(' ');
	  print_filename(filename);
	  current_filename = filename;
     }
     putchar('\n');
}

static VOID print_string(slen, s, is_sdata)
UNS slen;
UNCH *s;
int is_sdata;
{
     if (is_sdata)
	  fputs("\\|", stdout);
     while (slen > 0) {
	  UNCH ch = *s++;
	  slen--;
	  if (ch == DELSDATA) {
	       if (is_sdata)
		    ;		/* I don't think this should happen */
	       else
		    fputs("\\|", stdout);
	       ;
	  }
	  else if (ch == DELCDATA)
	       ;
	  else {
	       if (ch == DELNONCH) {
		    if (!slen)
			 break;
		    ch = UNSHIFTNON(*s);
		    s++;
		    slen--;
	       }
	       switch (ch) {
	       case RECHAR:
		    fputs("\\n", stdout);
		    break;
	       case '\\':
		    fputs("\\\\", stdout);
		    break;
	       default:
		    if (ISASCII(ch) && isprint(ch))
			 putchar(ch);
		    else
			 printf("\\%03o", ch);
		    break;
	       }
	  }
     }
     if (is_sdata)
	  fputs("\\|", stdout);
}


static VOID print_id(id, pubid, sysid)
UNIV id;
UNCH *pubid;
UNCH *sysid;
{

     if (pubid) {
	  putchar(PUBID_CODE);
	  print_string(ustrlen(pubid), pubid, 0);
	  putchar('\n');
     }

     if (sysid) {
	  putchar(SYSID_CODE);
	  print_string(ustrlen(sysid), sysid, 0);
	  putchar('\n');
     }

     if (id) {
	  char *p;

	  for (p = id; *p != '\0'; p++) {
	       putchar(FILE_CODE);
	       do {
		    switch (*p) {
		    case '\\':
			 fputs("\\\\", stdout);
			 break;
		    case '\n':
			 fputs("\\n", stdout);
			 break;
		    default:
			 if (ISASCII(*p) && isprint((UNCH)*p))
			      putchar(*p);
			 else
			      printf("\\%03o", (UNCH)*p);
			 break;
		    }
	       } while (*++p);
	       putchar('\n');
	  }
     }
}

static VOID print_filename(s)
char *s;
{
     for (; *s; s++)
	  switch (*s) {
	  case '\\':
	       fputs("\\\\", stdout);
	       break;
	  case '\n':
	       fputs("\\n", stdout);
	       break;
	  default:
	       if (ISASCII(*s) && isprint((UNCH)*s))
		    putchar(*s);
	       else
		    printf("\\%03o", (UNCH)*s);
	       break;
	  }
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
