/* sgmls.c:
   Library for reading output of sgmls.

   Written by James Clark (jjc@jclark.com). */

#include "config.h"
#include "std.h"
#include "sgmls.h"
#include "lineout.h"

#ifdef USE_PROTOTYPES
#define P(parms) parms
#else
#define P(parms) ()
#endif

typedef struct sgmls_data data_s;
typedef struct sgmls_notation notation_s;
typedef struct sgmls_internal_entity internal_entity_s;
typedef struct sgmls_external_entity external_entity_s;
typedef struct sgmls_entity entity_s;
typedef struct sgmls_attribute attribute_s;
typedef struct sgmls_event event_s;

/* lists are sorted in reverse order of level */
struct list {
  int subdoc_level;		/* -1 if associated with finished subdoc */
  struct list *next;
  char *name;
};

struct entity_list {
  int subdoc_level;
  struct entity_list *next;
  entity_s entity;
};

struct notation_list {
  int subdoc_level;
  struct notation_list *next;
  notation_s notation;
};

struct sgmls {
  FILE *fp;
  char *buf;
  unsigned buf_size;
  struct entity_list *entities;
  struct notation_list *notations;
  attribute_s *attributes;
  unsigned long lineno;
  char *filename;
  unsigned filename_size;
  unsigned long input_lineno;
  int subdoc_level;
  char **files;			/* from `f' commands */
  int nfiles;
  char *sysid;			/* from `s' command */
  char *pubid;			/* from `p' command */
};

enum error_code {
  E_ZERO,			/* Not an error */
  E_NOMEM,			/* Out of memory */
  E_BADESCAPE,			/* Bad escape */
  E_NULESCAPE,			/* \000 other than in data */
  E_NUL,			/* A null input character */
  E_BADENTITY,			/* Reference to undefined entity */
  E_INTERNALENTITY,		/* Internal entity when external was needed */
  E_SYSTEM,			/* System input error */
  E_COMMAND,			/* Bad command letter */
  E_MISSING,			/* Missing arguments */
  E_NUMBER,			/* Not a number */
  E_ATTR,			/* Bad attribute type */
  E_BADNOTATION,		/* Reference to undefined notation */
  E_BADINTERNAL,		/* Bad internal entity type */
  E_BADEXTERNAL,		/* Bad external entity type */
  E_EOF,			/* EOF in middle of line */
  E_SDATA,			/* \| other than in data */
  E_LINELENGTH			/* line longer than UNSIGNED_MAX */
};

static char *errlist[] = {
  0,
  "Out of memory",
  "Bad escape",
  "\\0 escape not in data",
  "Nul character in input",
  "Reference to undefined entity",
  "Internal entity when external was needed",
  "System input error",
  "Bad command letter",
  "Missing arguments",
  "Not a number",
  "Bad attribute type",
  "Reference to undefined notation",
  "Bad internal entity type",
  "Bad external entity type",
  "EOF in middle of line",
  "\\| other than in data",
  "Too many V commands",
  "Input line too long"
};

static void error P((enum error_code));
static int parse_data P((char *, unsigned long *));
static void parse_location P((char *, struct sgmls *));
static void parse_notation P((char *, notation_s *));
static void parse_internal_entity P((char *, internal_entity_s *));
static void parse_external_entity
  P((char *, struct sgmls *, external_entity_s *));
static void parse_subdoc_entity P((char *, external_entity_s *));
static attribute_s *parse_attribute P((struct sgmls *, char *));
static void grow_datav P((void));
static char *unescape P((char *));
static char *unescape_file P((char *));
static int unescape1 P((char *));
static char *scan_token P((char **));
static int count_args P((char *));
static struct list *list_find P((struct list *, char *, int));
static UNIV xmalloc P((unsigned));
static UNIV xrealloc P((UNIV , unsigned));
static char *strsave P((char *));
static int read_line P((struct sgmls *));
static notation_s *lookup_notation P((struct sgmls *, char *));
static entity_s *lookup_entity P((struct sgmls *, char *));
static external_entity_s *lookup_external_entity P((struct sgmls *, char *));
static void define_external_entity P((struct sgmls *, external_entity_s *));
static void define_internal_entity P((struct sgmls *, internal_entity_s *));
static void define_notation P((struct sgmls *, notation_s *));
static data_s *copy_data P((data_s *, int));
static void list_finish_level P((struct list **, int));
static void add_attribute P((attribute_s **, attribute_s *));
static void default_errhandler P((int, char *, unsigned long));

#define xfree(s) do { if (s) free(s); } while (0)

static sgmls_errhandler *errhandler = default_errhandler;
static unsigned long input_lineno = 0;

static data_s *datav = 0;
static int datav_size = 0;

struct sgmls *sgmls_create(fp)
     FILE *fp;
{
  struct sgmls *sp;

  sp = (struct sgmls *)malloc(sizeof(struct sgmls));
  if (!sp)
    return 0;
  sp->fp = fp;
  sp->entities = 0;
  sp->notations = 0;
  sp->attributes = 0;
  sp->lineno = 0;
  sp->filename = 0;
  sp->filename_size = 0;
  sp->input_lineno = 0;
  sp->buf_size = 0;
  sp->buf = 0;
  sp->subdoc_level = 0;
  sp->files = 0;
  sp->nfiles = 0;
  sp->sysid = 0;
  sp->pubid = 0;
  return sp;
}

void sgmls_free(sp)
     struct sgmls *sp;
{
  struct entity_list *ep;
  struct notation_list *np;

  if (!sp)
    return;
  xfree(sp->filename);
  sgmls_free_attributes(sp->attributes);

  for (ep = sp->entities; ep;) {
    struct entity_list *tem = ep->next;
    if (ep->entity.is_internal) {
      xfree(ep->entity.u.internal.data.s);
      free(ep->entity.u.internal.name);
    }
    else {
      int i;
      for (i = 0; i < ep->entity.u.external.nfilenames; i++)
	xfree(ep->entity.u.external.filenames[i]);
      xfree(ep->entity.u.external.filenames);
      xfree(ep->entity.u.external.sysid);
      xfree(ep->entity.u.external.pubid);
      sgmls_free_attributes(ep->entity.u.external.attributes);
      free(ep->entity.u.internal.name);
    }
    free(ep);
    ep = tem;
  }

  for (np = sp->notations; np;) {
    struct notation_list *tem = np->next;
    xfree(np->notation.sysid);
    xfree(np->notation.pubid);
    free(np->notation.name);
    free(np);
    np = tem;
  }

  xfree(sp->buf);
  xfree(sp->pubid);
  xfree(sp->sysid);
  if (sp->files) {
    int i;
    for (i = 0; i < sp->nfiles; i++)
      free(sp->files[i]);
    free(sp->files);
  }
  free(sp);

  xfree(datav);
  datav = 0;
  datav_size = 0;
}

sgmls_errhandler *sgmls_set_errhandler(handler)
     sgmls_errhandler *handler;
{
  sgmls_errhandler *old = errhandler;
  if (handler)
    errhandler = handler;
  return old;
}

int sgmls_next(sp, e)
     struct sgmls *sp;
     event_s *e;
{
  while (read_line(sp)) {
    char *buf = sp->buf;

    e->filename = sp->filename;
    e->lineno = sp->lineno;

    switch (buf[0]) {
    case DATA_CODE:
      e->u.data.n = parse_data(buf + 1, &sp->lineno);
      e->u.data.v = datav;
      e->type = SGMLS_EVENT_DATA;
      return 1;
    case START_CODE:
      {
	char *p;
	e->u.start.attributes = sp->attributes;
	sp->attributes = 0;
	e->type = SGMLS_EVENT_START;
	p = buf + 1;
	e->u.start.gi = scan_token(&p);
	return 1;
      }
    case END_CODE:
      {
	char *p = buf + 1;
	e->type = SGMLS_EVENT_END;
	e->u.end.gi = scan_token(&p);
	return 1;
      }
    case START_SUBDOC_CODE:
    case END_SUBDOC_CODE:
      {
	char *p = buf + 1;
	char *name = scan_token(&p);
	if (buf[0] == START_SUBDOC_CODE) {
	  e->u.entity = lookup_external_entity(sp, name);
	  sp->subdoc_level++;
	  e->type = SGMLS_EVENT_SUBSTART;
	}
	else {
	  e->type = SGMLS_EVENT_SUBEND;
	  list_finish_level((struct list **)&sp->entities, sp->subdoc_level);
	  list_finish_level((struct list **)&sp->notations, sp->subdoc_level);
	  sp->subdoc_level--;
	  e->u.entity = lookup_external_entity(sp, name);
	}
	return 1;
      }
    case ATTRIBUTE_CODE:
      add_attribute(&sp->attributes, parse_attribute(sp, buf + 1));
      break;
    case DATA_ATTRIBUTE_CODE:
      {
	char *p = buf + 1;
	char *name;
	attribute_s *a;
	external_entity_s *ext;
	
	name = scan_token(&p);
	a = parse_attribute(sp, p);
	ext = lookup_external_entity(sp, name);
	add_attribute(&ext->attributes, a);
      }
      break;
    case REFERENCE_ENTITY_CODE:
      {
	char *p = buf + 1;
	char *name;
	name = scan_token(&p);
	e->u.entity = lookup_external_entity(sp, name);
	e->type = SGMLS_EVENT_ENTITY;
	return 1;
      }
    case DEFINE_NOTATION_CODE:
      {
	notation_s notation;

	parse_notation(buf + 1, &notation);
	define_notation(sp, &notation);
      }
      break;
    case DEFINE_EXTERNAL_ENTITY_CODE:
      {
	external_entity_s external;

	parse_external_entity(buf + 1, sp, &external);
	define_external_entity(sp, &external);
      }
      break;
    case DEFINE_SUBDOC_ENTITY_CODE:
      {
	external_entity_s external;

	parse_subdoc_entity(buf + 1, &external);
	define_external_entity(sp, &external);
      }
      break;
    case DEFINE_INTERNAL_ENTITY_CODE:
      {
	internal_entity_s internal;

	parse_internal_entity(buf + 1, &internal);
	define_internal_entity(sp, &internal);
      }
      break;
    case PI_CODE:
      e->u.pi.len = unescape1(buf + 1);
      e->u.pi.s = buf + 1;
      e->type = SGMLS_EVENT_PI;
      return 1;
    case LOCATION_CODE:
      parse_location(buf + 1, sp);
      break;
    case APPINFO_CODE:
      e->u.appinfo = unescape(buf + 1);
      e->type = SGMLS_EVENT_APPINFO;
      return 1;
    case SYSID_CODE:
      sp->sysid = strsave(unescape(buf + 1));
      break;
    case PUBID_CODE:
      sp->pubid = strsave(unescape(buf + 1));
      break;
    case FILE_CODE:
      sp->files = xrealloc(sp->files, (sp->nfiles + 1)*sizeof(char *));
      sp->files[sp->nfiles] = strsave(unescape_file(buf + 1));
      sp->nfiles += 1;
      break;
    case CONFORMING_CODE:
      e->type = SGMLS_EVENT_CONFORMING;
      return 1;
    default:
      error(E_COMMAND);
    }
  }

  return 0;
}

static
int parse_data(p, linenop)
     char *p;
     unsigned long *linenop;
{
  int n = 0;
  char *start = p;
  char *q;
  int is_sdata = 0;

  /* No need to copy before first escape. */

  for (; *p != '\\' && *p != '\0'; p++)
    ;
  q = p;
  while (*p) {
    if (*p == '\\') {
      switch (*++p) {
      case '\\':
	*q++ = *p++;
	break;
      case 'n':
	*q++ = RECHAR;
	*linenop += 1;
	p++;
	break;
      case '0':
      case '1':
      case '2':
      case '3':
      case '4':
      case '5':
      case '6':
      case '7':
	{
	  int val = *p++ - '0';
	  if (*p >= '0' && *p <= '7') {
	    val = val*8 + (*p++ - '0');
	    if (*p >= '0' && *p <= '7')
	      val = val*8 + (*p++ - '0');
	  }
	  *q++ = (char)val;
	}
	break;
      case '|':
	if (q > start || is_sdata) {
	  if (n >= datav_size)
	    grow_datav();
	  datav[n].s = start;
	  datav[n].len = q - start;
	  datav[n].is_sdata = is_sdata;
	  n++;
	}
	is_sdata = !is_sdata;
	start = q;
	p++;
	break;
      default:
	error(E_BADESCAPE);
      }
    }
    else
      *q++ = *p++;
  }
  
  if (q > start || is_sdata) {
    if (n >= datav_size)
      grow_datav();
    datav[n].s = start;
    datav[n].len = q - start;
    datav[n].is_sdata = is_sdata;
    n++;
  }
  return n;
}

static
void grow_datav()
{
  unsigned size = datav_size ? 2*datav_size : 2;
  datav = (data_s *)xrealloc((UNIV)datav, size*sizeof(data_s));
  datav_size = size;
}

static
void parse_location(s, sp)
     char *s;
     struct sgmls *sp;
{
  unsigned size;

  if (*s < '0' || *s > '9' || sscanf(s, "%lu", &sp->lineno) != 1)
    error(E_NUMBER);
  do {
    ++s;
  } while (*s >= '0' && *s <= '9');

  if (*s != ' ')
    return;
  s++;
  s = unescape_file(s);
  size = strlen(s) + 1;
  if (size <= sp->filename_size)
    strcpy(sp->filename, s);
  else {
    sp->filename = xrealloc(sp->filename, size);
    strcpy(sp->filename, s);
    sp->filename_size = size;
  }
}

static
void parse_notation(s, n)
     char *s;
     notation_s *n;
{
  n->name = strsave(scan_token(&s));
}

static
void parse_internal_entity(s, e)
     char *s;
     internal_entity_s *e;
{
  char *type;

  e->name = strsave(scan_token(&s));
  type = scan_token(&s);
  if (strcmp(type, "CDATA") == 0)
    e->data.is_sdata = 0;
  else if (strcmp(type, "SDATA") == 0)
    e->data.is_sdata = 1;
  else
    error(E_BADINTERNAL);
  e->data.len = unescape1(s);
  if (e->data.len == 0)
    e->data.s = 0;
  else {
    e->data.s = xmalloc(e->data.len);
    memcpy(e->data.s, s, e->data.len);
  }
}

static
void parse_external_entity(s, sp, e)
     char *s;
     struct sgmls *sp;
     external_entity_s *e;
{
  char *type;
  char *notation;

  e->name = strsave(scan_token(&s));
  type = scan_token(&s);
  if (strcmp(type, "CDATA") == 0)
    e->type = SGMLS_ENTITY_CDATA;
  else if (strcmp(type, "SDATA") == 0)
    e->type = SGMLS_ENTITY_SDATA;
  else if (strcmp(type, "NDATA") == 0)
    e->type = SGMLS_ENTITY_NDATA;
  else
    error(E_BADEXTERNAL);
  notation = scan_token(&s);
  e->notation = lookup_notation(sp, notation);
}

static
void parse_subdoc_entity(s, e)
     char *s;
     external_entity_s *e;
{
  e->name = strsave(scan_token(&s));
  e->type = SGMLS_ENTITY_SUBDOC;
}

static
attribute_s *parse_attribute(sp, s)
     struct sgmls *sp;
     char *s;
{
  attribute_s *a;
  char *type;

  a = (attribute_s *)xmalloc(sizeof(*a));
  a->name = strsave(scan_token(&s));
  type = scan_token(&s);
  if (strcmp(type, "CDATA") == 0) {
    unsigned long lineno = 0;
    a->type = SGMLS_ATTR_CDATA;
    a->value.data.n = parse_data(s, &lineno);
    a->value.data.v = copy_data(datav, a->value.data.n);
  }
  else if (strcmp(type, "IMPLIED") == 0) {
    a->type = SGMLS_ATTR_IMPLIED;
  }
  else if (strcmp(type, "NOTATION") == 0) {
    a->type = SGMLS_ATTR_NOTATION;
    a->value.notation = lookup_notation(sp, scan_token(&s));
  }
  else if (strcmp(type, "ENTITY") == 0) {
    int n, i;
    a->type = SGMLS_ATTR_ENTITY;
    n = count_args(s);
    if (n == 0)
      error(E_MISSING);
    a->value.entity.v = (entity_s **)xmalloc(n*sizeof(entity_s *));
    a->value.entity.n = n;
    for (i = 0; i < n; i++)
      a->value.entity.v[i] = lookup_entity(sp, scan_token(&s));
  }
  else if (strcmp(type, "TOKEN") == 0) {
    int n, i;
    a->type = SGMLS_ATTR_TOKEN;
    n = count_args(s);
    if (n == 0)
      error(E_MISSING);
    a->value.token.v = (char **)xmalloc(n * sizeof(char *));
    for (i = 0; i < n; i++)
      a->value.token.v[i] = strsave(scan_token(&s));
    a->value.token.n = n;
  }
  else
    error(E_ATTR);
  return a;
}

void sgmls_free_attributes(p)
     attribute_s *p;
{
  while (p) {
    attribute_s *nextp = p->next;
    switch (p->type) {
    case SGMLS_ATTR_CDATA:
      if (p->value.data.v) {
	free(p->value.data.v[0].s);
	free(p->value.data.v);
      }
      break;
    case SGMLS_ATTR_TOKEN:
      {
	int i;
	for (i = 0; i < p->value.token.n; i++)
	  free(p->value.token.v[i]);
	xfree(p->value.token.v);
      }
      break;
    case SGMLS_ATTR_ENTITY:
      xfree(p->value.entity.v);
      break;
    case SGMLS_ATTR_IMPLIED:
    case SGMLS_ATTR_NOTATION:
      break;
    }
    free(p->name);
    free(p);
    p = nextp;
  }
}

static
data_s *copy_data(v, n)
     data_s *v;
     int n;
{
  if (n == 0)
    return 0;
  else {
    int i;
    unsigned total;
    char *p;
    data_s *result;
    
    result = (data_s *)xmalloc(n*sizeof(data_s));
    total = 0;
    for (i = 0; i < n; i++)
      total += v[i].len;
    if (!total)
      total++;
    p = xmalloc(total);
    for (i = 0; i < n; i++) {
      result[i].s = p;
      memcpy(result[i].s, v[i].s, v[i].len);
      result[i].len = v[i].len;
      p += v[i].len;
      result[i].is_sdata = v[i].is_sdata;
    }
    return result;
  }
}

/* Unescape s, and return nul-terminated data.  Give an error
if the data contains 0. */

static
char *unescape(s)
     char *s;
{
  int len = unescape1(s);
  if (
#ifdef __BORLANDC__
      len > 0 &&
#endif
      memchr(s, '\0', len))
    error(E_NULESCAPE);
  s[len] = '\0';
  return s;
}

/* Like unescape(), but REs are represented by 012 not 015. */

static
char *unescape_file(s)
     char *s;
{
  char *p;
  p = s = unescape(s);
  while ((p = strchr(p, RECHAR)) != 0)
    *p++ = '\n';
  return s;

}

/* Unescape s, and return length of data.  The data may contain 0. */

static
int unescape1(s)
     char *s;
{
  const char *p;
  char *q;

  q = strchr(s, '\\');
  if (!q)
    return strlen(s);
  p = q;
  while (*p) {
    if (*p == '\\') {
      switch (*++p) {
      case '\\':
	*q++ = *p++;
	break;
      case 'n':
	*q++ = RECHAR;
	p++;
	break;
      case '0':
      case '1':
      case '2':
      case '3':
      case '4':
      case '5':
      case '6':
      case '7':
	{
	  int val = *p++ - '0';
	  if (*p >= '0' && *p <= '7') {
	    val = val*8 + (*p++ - '0');
	    if (*p >= '0' && *p <= '7')
	      val = val*8 + (*p++ - '0');
	  }
	  *q++ = (char)val;
	}
	break;
      case '|':
	error(E_SDATA);
      default:
	error(E_BADESCAPE);
      }
    }
    else
      *q++ = *p++;
  }
  return q - s;
}

static
char *scan_token(pp)
     char **pp;
{
  char *start = *pp;
  while (**pp != '\0') {
    if (**pp == ' ') {
      **pp = '\0';
      *pp += 1;
      break;
    }
    *pp += 1;
  }
  if (!*start)
    error(E_MISSING);
  return start;
}

static
int count_args(p)
     char *p;
{
  int n = 0;

  while (*p != '\0') {
    n++;
    do {
      ++p;
      if (*p == ' ') {
	p++;
	break;
      }
    } while (*p != '\0');
  }
  return n;
}

static
int read_line(sp)
     struct sgmls *sp;
{
  unsigned i = 0;
  FILE *fp = sp->fp;
  int c;
  char *buf = sp->buf;
  unsigned buf_size = sp->buf_size;

  c = getc(fp);
  if (c == EOF) {
    input_lineno = sp->input_lineno;
    if (ferror(fp))
      error(E_SYSTEM);
    return 0;
  }
  
  sp->input_lineno++;
  input_lineno = sp->input_lineno;
  for (;;) {
    if (i >= buf_size) {
      if (buf_size == 0)
	buf_size = 24;
      else if (buf_size > (unsigned)UINT_MAX/2) {
	if (buf_size == (unsigned)UINT_MAX)
	  error(E_LINELENGTH);
	buf_size = (unsigned)UINT_MAX;
      }
      else
	buf_size *= 2;
      buf = xrealloc(buf, buf_size);
      sp->buf = buf;
      sp->buf_size = buf_size;
    }
    if (c == '\0')
      error(E_NUL);
    if (c == '\n') {
      buf[i] = '\0';
      break;
    }
    buf[i++] = c;
    c = getc(fp);
    if (c == EOF) {
      if (ferror(fp))
	error(E_SYSTEM);
      else
	error(E_EOF);
    }
  }
  return 1;
}

static
notation_s *lookup_notation(sp, name)
struct sgmls *sp;
char *name;
{
  struct notation_list *p
    = (struct notation_list *)list_find((struct list *)sp->notations, name,
					sp->subdoc_level);
  if (!p)
    error(E_BADNOTATION);
  return &p->notation;
}

static
entity_s *lookup_entity(sp, name)
struct sgmls *sp;
char *name;
{
  struct entity_list *p
    = (struct entity_list *)list_find((struct list *)sp->entities, name,
				      sp->subdoc_level);
  if (!p)
    error(E_BADENTITY);
  return &p->entity;
}

static
external_entity_s *lookup_external_entity(sp, name)
struct sgmls *sp;
char *name;
{
  entity_s *p = lookup_entity(sp, name);
  if (p->is_internal)
    error(E_INTERNALENTITY);
  return &p->u.external;
}

static
void define_external_entity(sp, e)
struct sgmls *sp;
external_entity_s *e;
{
  struct entity_list *p;
  e->attributes = 0;
  e->filenames = sp->files;
  e->nfilenames = sp->nfiles;
  sp->files = 0;
  sp->nfiles = 0;
  e->pubid = sp->pubid;
  sp->pubid = 0;
  e->sysid = sp->sysid;
  sp->sysid = 0;
  p = (struct entity_list *)xmalloc(sizeof(struct entity_list));
  memcpy((UNIV)&p->entity.u.external, (UNIV)e, sizeof(*e));
  p->entity.is_internal = 0;
  p->subdoc_level = sp->subdoc_level;
  p->next = sp->entities;
  sp->entities = p;
}

static
void define_internal_entity(sp, e)
struct sgmls *sp;
internal_entity_s *e;
{
  struct entity_list *p;
  p = (struct entity_list *)xmalloc(sizeof(struct entity_list));
  memcpy((UNIV)&p->entity.u.internal, (UNIV)e, sizeof(*e));
  p->entity.is_internal = 1;
  p->subdoc_level = sp->subdoc_level;
  p->next = sp->entities;
  sp->entities = p;
}

static
void define_notation(sp, np)
struct sgmls *sp;
notation_s *np;
{
  struct notation_list *p;
  np->sysid = sp->sysid;
  sp->sysid = 0;
  np->pubid = sp->pubid;
  sp->pubid = 0;
  p = (struct notation_list *)xmalloc(sizeof(struct notation_list));
  memcpy((UNIV)&p->notation, (UNIV)np, sizeof(*np));
  p->subdoc_level = sp->subdoc_level;
  p->next = sp->notations;
  sp->notations = p;
}

static
struct list *list_find(p, name, level)
     struct list *p;
     char *name;
     int level;
{
  for (; p && p->subdoc_level == level; p = p->next)
    if (strcmp(p->name, name) == 0)
      return p;
  return 0;
}

/* Move all the items in the list whose subdoc level is level to the
end of the list and make their subdoc_level -1. */

static
void list_finish_level(listp, level)
     struct list **listp;
     int level;
{
  struct list **pp, *next_level, *old_level;
  for (pp = listp; *pp && (*pp)->subdoc_level == level; pp = &(*pp)->next)
    (*pp)->subdoc_level = -1;
  next_level = *pp;
  *pp = 0;
  old_level = *listp;
  *listp = next_level;
  for (pp = listp; *pp; pp = &(*pp)->next)
    ;
  *pp = old_level;
}

static
void add_attribute(pp, a)
     attribute_s **pp, *a;
{
#if 0
  for (; *pp && strcmp((*pp)->name, a->name) < 0; pp = &(*pp)->next)
    ;
#endif
  a->next = *pp;
  *pp = a;
}

     
static
char *strsave(s)
char *s;
{
  if (!s)
    return s;
  else {
    char *p = xmalloc(strlen(s) + 1);
    strcpy(p, s);
    return p;
  }
}

static
UNIV xmalloc(n)
  unsigned n;
{
  UNIV p = malloc(n);
  if (!p)
    error(E_NOMEM);
  return p;
}

/* ANSI C says first argument to realloc can be NULL, but not everybody
   appears to support this. */

static
UNIV xrealloc(p, n)
     UNIV p;
     unsigned n;
{
  p = p ? realloc(p, n) : malloc(n);
  if (!p)
    error(E_NOMEM);
  return p;
}

static
void error(num)
     enum error_code num;
{
  (*errhandler)((int)num, errlist[num], input_lineno);
  abort();
}

static
void default_errhandler(num, msg, lineno)
     int num;
     char *msg;
     unsigned long lineno;
{
  fprintf(stderr, "Line %lu: %s\n", lineno, msg);
  exit(1);
}
