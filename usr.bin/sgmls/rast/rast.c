/* rast.c
   Translate sgmls output to RAST result format.

   Written by James Clark (jjc@jclark.com). */

#include "config.h"
#include "std.h"
#include "sgmls.h"
#include "getopt.h"

#ifdef USE_PROTOTYPES
#define P(parms) parms
#else
#define P(parms) ()
#endif

#ifdef __GNUC__
#define NO_RETURN volatile
#else
#define NO_RETURN /* as nothing */
#endif

#ifdef VARARGS
#define VP(parms) ()
#else
#define VP(parms) P(parms)
#endif

#ifdef USE_ISASCII
#define ISASCII(c) isascii(c)
#else
#define ISASCII(c) (1)
#endif

NO_RETURN void error VP((char *,...));

static void input_error P((int, char *, unsigned long));
static int do_file P((FILE *));
static void usage P((void));
static void init_sort_code P((void));

static void output_processing_instruction P((char *, unsigned));
static void output_data P((struct sgmls_data *, int));
static void output_data_lines P((char *, unsigned));
static void output_internal_sdata P((char *, unsigned));
static void output_external_entity P((struct sgmls_external_entity *));
static void output_external_entity_info P((struct sgmls_external_entity *));
static void output_element_start P((char *, struct sgmls_attribute *));
static void output_element_end P((char *));
static void output_attribute P((struct sgmls_attribute *));
static void output_attribute_list P((struct sgmls_attribute *));
static void output_tokens P((char **, int));
static void output_markup_chars P((char *, unsigned));
static void output_markup_string P((char *));
static void output_char P((int, int));
static void output_flush P((int));
static void output_external_id P((char *, char *));
static void output_entity P((struct sgmls_entity *));
static void output_external_entity_info P((struct sgmls_external_entity *));
static void output_internal_entity P((struct sgmls_internal_entity *));
/* Don't use a prototype here to avoid problems with qsort. */
static int compare_attributes();

#define output_flush_markup() output_flush('!')
#define output_flush_data() output_flush('|')

static FILE *outfp;
static int char_count = 0;
static char *program_name;

static short sort_code[256];
static struct sgmls_attribute **attribute_vector = 0;
static int attribute_vector_length = 0;

int main(argc, argv)
     int argc;
     char **argv;
{
  int c;
  int opt;
  char *output_file = 0;

  program_name = argv[0];

  while ((opt = getopt(argc, argv, "o:")) != EOF)
    switch (opt) {
    case 'o':
      output_file = optarg;
      break;
    case '?':
      usage();
    default:
      abort();
    }

  if (output_file) {
    errno = 0;
    outfp = fopen(output_file, "w");
    if (!outfp)
      error("couldn't open `%s' for output: %s", strerror(errno));
  }
  else {
    outfp = tmpfile();
    if (!outfp)
      error("couldn't create temporary file: %s", strerror(errno));
  }

  if (argc - optind > 1)
    usage();

  if (argc - optind == 1) {
    if (!freopen(argv[optind], "r", stdin))
      error("couldn't open `%s' for input: %s", argv[optind], strerror(errno));
  }

  (void)sgmls_set_errhandler(input_error);

  init_sort_code();

  if (!do_file(stdin)) {
    fclose(outfp);
    if (output_file) {
      if (!freopen(output_file, "w", stdout))
	error("couldn't reopen `%s' for output: %s", strerror(errno));
    }
    fputs("#ERROR\n", stdout);
    exit(EXIT_FAILURE);
  }

  if (output_file) {
    errno = 0;
    if (fclose(outfp) == EOF)
      error("error closing `%s': %s", output_file, strerror(errno));
  }
  else {
    errno = 0;
    if (fseek(outfp, 0L, SEEK_SET))
      error("couldn't rewind temporary file: %s", strerror(errno));
    while ((c = getc(outfp)) != EOF)
      if (putchar(c) == EOF)
	error("error writing standard output: %s", strerror(errno));
  }
  exit(EXIT_SUCCESS);
}

static
void usage()
{
  fprintf(stderr, "usage: %s [-o output_file] [input_file]\n", program_name);
  exit(EXIT_FAILURE);
}

static
void init_sort_code()
{
  int i;
  static char print[] = "!\"#$%&'()*+,-./0123456789:;<=>?\
@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~";
  for (i = 0; i < 256; i++)
    sort_code[i] = i + 128;
  for (i = 0; print[i]; i++)
    sort_code[(unsigned char)print[i]] = i;
}

static
int do_file(fp)
     FILE *fp;
{
  struct sgmls *sp;
  struct sgmls_event e;
  int conforming = 0;

  sp = sgmls_create(fp);
  while (sgmls_next(sp, &e))
    switch (e.type) {
    case SGMLS_EVENT_DATA:
      output_data(e.u.data.v, e.u.data.n);
      break;
    case SGMLS_EVENT_ENTITY:
      output_external_entity(e.u.entity);
      break;
    case SGMLS_EVENT_PI:
      output_processing_instruction(e.u.pi.s, e.u.pi.len);
      break;
    case SGMLS_EVENT_START:
      output_element_start(e.u.start.gi, e.u.start.attributes);
      sgmls_free_attributes(e.u.start.attributes);
      break;
    case SGMLS_EVENT_END:
      output_element_end(e.u.end.gi);
      break;
    case SGMLS_EVENT_SUBSTART:
      {
	int level = 1;
	output_external_entity(e.u.entity);
	while (level > 0) {
	  if (!sgmls_next(sp, &e))
	    return 0;
	  switch (e.type) {
	  case SGMLS_EVENT_SUBSTART:
	    level++;
	    break;
	  case SGMLS_EVENT_SUBEND:
	    level--;
	    break;
	  case SGMLS_EVENT_START:
	    sgmls_free_attributes(e.u.start.attributes);
	    break;
	  default:
	    /* prevent compiler warnings */
	    break;
	  }
	}
      }
      break;
    case SGMLS_EVENT_APPINFO:
      break;
    case SGMLS_EVENT_CONFORMING:
      conforming = 1;
      break;
    default:
      abort();
    }
  sgmls_free(sp);
  return conforming;
}

static
void output_processing_instruction(s, len)
     char *s;
     unsigned len;
{
  fputs("[?", outfp);
  if (len > 0) {
    putc('\n', outfp);
    output_data_lines(s, len);
    output_flush_data();
  }
  fputs("]\n", outfp);
}

static
void output_data(v, n)
     struct sgmls_data *v;
     int n;
{
  int i;
  for (i = 0; i < n; i++) {
    if (v[i].is_sdata)
      output_internal_sdata(v[i].s, v[i].len);
    else if (v[i].len > 0)
      output_data_lines(v[i].s, v[i].len);
  }
}

static
void output_data_lines(s, n)
     char *s;
     unsigned n;
{
  assert(n > 0);
  for (; n > 0; --n)
    output_char((unsigned char)*s++, '|');
  output_flush_data();
}

static
void output_internal_sdata(s, n)
     char *s;
     unsigned n;
{
  fputs("#SDATA-TEXT\n", outfp);
  output_markup_chars(s, n);
  output_flush_markup();
  fputs("#END-SDATA\n", outfp);
}

static
void output_external_entity(e)
     struct sgmls_external_entity *e;
{
  fprintf(outfp, "[&%s\n", e->name);
  output_external_entity_info(e);
  fputs("]\n", outfp);
}

static
void output_element_start(gi, att)
     char *gi;
     struct sgmls_attribute *att;
{
  fprintf(outfp, "[%s", gi);
  if (att) {
    putc('\n', outfp);
    output_attribute_list(att);
  }
  fputs("]\n", outfp);
}

static
void output_element_end(gi)
     char *gi;
{
  fprintf(outfp, "[/%s]\n", gi);
}

static
void output_attribute_list(att)
     struct sgmls_attribute *att;
{
  struct sgmls_attribute *p;
  int n = 0;
  int i;

  for (p = att; p; p = p->next)
    n++;
  if (attribute_vector_length < n) {
    if (attribute_vector_length == 0)
      attribute_vector
	= (struct sgmls_attribute **)malloc(n*sizeof(*attribute_vector));
    else
      attribute_vector
	= (struct sgmls_attribute **)realloc((UNIV)attribute_vector,
					     n*sizeof(*attribute_vector));
    attribute_vector_length = n;
    if (!attribute_vector)
      error("Out of memory");
  }
  i = 0;
  for (p = att; p; p = p->next)
    attribute_vector[i++] = p;
  qsort(attribute_vector, n, sizeof(attribute_vector[0]), compare_attributes);
  for (i = 0; i < n; i++)
    output_attribute(attribute_vector[i]);
}

static
int compare_attributes(p1, p2)
     UNIV p1, p2;
{
  char *s1 = (*(struct sgmls_attribute **)p1)->name;
  char *s2 = (*(struct sgmls_attribute **)p2)->name;
  
  for (; *s1 && *s2; s1++, s2++)
    if (*s1 != *s2)
      return sort_code[(unsigned char)*s1] - sort_code[(unsigned char)*s2];
  if (*s1)
    return 1;
  else if (*s2)
    return -1;
  else
    return 0;
}

static
void output_attribute(p)
     struct sgmls_attribute *p;
{
  fprintf(outfp, "%s=\n", p->name);
  switch (p->type) {
  case SGMLS_ATTR_IMPLIED:
    fputs("#IMPLIED\n", outfp);
    break;
  case SGMLS_ATTR_CDATA:
    {
      struct sgmls_data *v = p->value.data.v;
      int n = p->value.data.n;
      int i;
      for (i = 0; i < n; i++)
	if (v[i].is_sdata)
	  output_internal_sdata(v[i].s, v[i].len);
	else {
	  output_markup_chars(v[i].s, v[i].len);
	  output_flush_markup();
	}
    }
    break;
  case SGMLS_ATTR_TOKEN:
    output_tokens(p->value.token.v, p->value.token.n);
    break;
  case SGMLS_ATTR_ENTITY:
    {
      int i;
      for (i = 0; i < p->value.entity.n; i++) {
	struct sgmls_entity *e = p->value.entity.v[i];
	char *name;

	if (e->is_internal)
	  name = e->u.internal.name;
	else
	  name = e->u.external.name;
	if (i > 0)
	  output_markup_string(" ");
	output_markup_string(name);
      }
      output_flush_markup();
      for (i = 0; i < p->value.entity.n; i++)
	output_entity(p->value.entity.v[i]);
    }
    break;
  case SGMLS_ATTR_NOTATION:
    output_tokens(&p->value.notation->name, 1);
    output_external_id(p->value.notation->pubid, p->value.notation->sysid);
    break;
  }
}

static void output_tokens(v, n)
     char **v;
     int n;
{
  int i;
  assert(n > 0);
  output_markup_string(v[0]);
  for (i = 1; i < n; i++) {
    output_markup_string(" ");
    output_markup_string(v[i]);
  }
  output_flush_markup();
}

static
void output_markup_chars(s, n)
     char *s;
     unsigned n;
{
  for (; n > 0; --n)
    output_char((unsigned char)*s++, '!');
}

static
void output_markup_string(s)
     char *s;
{
  while (*s)
    output_char((unsigned char)*s++, '!');
}

static
void output_char(c, delim)
     int c;
     int delim;
{
  if (ISASCII(c) && isprint(c)) {
    if (char_count == 0)
      putc(delim, outfp);
    putc(c, outfp);
    char_count++;
    if (char_count == 60) {
      putc(delim, outfp);
      putc('\n', outfp);
      char_count = 0;
    }
  }
  else {
    output_flush(delim);
    switch (c) {
    case RECHAR:
      fputs("#RE\n", outfp);
      break;
    case RSCHAR:
      fputs("#RS\n", outfp);
      break;
    case TABCHAR:
      fputs("#TAB\n", outfp);
      break;
    default:
      fprintf(outfp, "#%d\n", c);
    }
  }
}

static
void output_flush(delim)
     int delim;
{
  if (char_count > 0) {
    putc(delim, outfp);
    putc('\n', outfp);
    char_count = 0;
  }
}

static
void output_external_id(pubid, sysid)
  char *pubid;
  char *sysid;
{
  if (!pubid && !sysid)
    fputs("#SYSTEM\n#NONE\n", outfp);
  else {
    if (pubid) {
      fputs("#PUBLIC\n", outfp);
      if (*pubid) {
	output_markup_string(pubid);
	output_flush_markup();
      }
      else
	fputs("#EMPTY\n", outfp);
    }
    if (sysid) {
      fputs("#SYSTEM\n", outfp);
      if (*sysid) {
	output_markup_string(sysid);
	output_flush_markup();
      }
      else
	fputs("#EMPTY\n", outfp);
    }
  }
}

static
void output_entity(e)
     struct sgmls_entity *e;
{
  if (e->is_internal)
    output_internal_entity(&e->u.internal);
  else
    output_external_entity_info(&e->u.external);
  fputs("#END-ENTITY", outfp);
#ifndef ASIS
  putc('\n', outfp);
#endif
}

static
void output_external_entity_info(e)
     struct sgmls_external_entity *e;
{
  switch (e->type) {
  case SGMLS_ENTITY_CDATA:
    fputs("#CDATA-EXTERNAL", outfp);
    break;
  case SGMLS_ENTITY_SDATA:
    fputs("#SDATA-EXTERNAL", outfp);
    break;
  case SGMLS_ENTITY_NDATA:
    fputs("#NDATA-EXTERNAL", outfp);
    break;
  case SGMLS_ENTITY_SUBDOC:
    fputs("#SUBDOC", outfp);
    break;
  }
  putc('\n', outfp);
  output_external_id(e->pubid, e->sysid);
  if (e->type != SGMLS_ENTITY_SUBDOC) {
    fprintf(outfp, "#NOTATION=%s\n", e->notation->name);
    output_external_id(e->notation->pubid, e->notation->sysid);
    output_attribute_list(e->attributes);
  }
}

static
void output_internal_entity(e)
     struct sgmls_internal_entity *e;
{
  if (e->data.is_sdata)
    fputs("#SDATA-INTERNAL", outfp);
  else
    fputs("#CDATA-INTERNAL", outfp);
  putc('\n', outfp);
  output_markup_chars(e->data.s, e->data.len);
  output_flush_markup();
}

static
void input_error(num, str, lineno)
     int num;
     char *str;
     unsigned long lineno;
{
  error("Error at input line %lu: %s", lineno, str);
}

NO_RETURN
#ifdef VARARGS
void error(va_alist) va_dcl
#else
void error(char *message,...)
#endif
{
#ifdef VARARGS
     char *message;
#endif
     va_list ap;
     
     fprintf(stderr, "%s: ", program_name);
#ifdef VARARGS
     va_start(ap);
     message = va_arg(ap, char *);
#else
     va_start(ap, message);
#endif
     vfprintf(stderr, message, ap);
     va_end(ap);
     fputc('\n', stderr);
     fflush(stderr);
     exit(EXIT_FAILURE);
}
