/* replace.c
   Parse ASP style replacement file.

   Written by James Clark (jjc@jclark.com). */

#include "sgmlsasp.h"
#include "replace.h"

#define TABLE_SIZE 251

struct table_entry {
  enum event_type type;
  char *gi;
  struct replacement replacement;
  struct table_entry *next;
};

struct replacement_table {
  struct table_entry *table[TABLE_SIZE];
};

struct buffer {
  char *s;
  unsigned len;
  unsigned size;
};

/* Tokens returned by get_token(). */

#define STRING 1
#define STAGO 2
#define ETAGO 3
#define PLUS 4

static int get P((void));
static int peek P((void));
static int get_token P((void));
static void scan_name P((struct buffer *, int));
static struct replacement *define_replacement
  P((struct replacement_table *, enum event_type, char *));
static struct replacement_item **parse_string
  P((struct replacement_item **, int));
static UNIV xmalloc P((unsigned));
static UNIV xrealloc P((UNIV, unsigned));
static struct replacement_item **add_replacement_data
  P((struct replacement_item **, char *, unsigned));
static struct replacement_item **add_replacement_attr
  P((struct replacement_item **, char *));
static int hash P((enum event_type, char *));
static NO_RETURN void parse_error VP((char *,...));
static VOID buffer_init P((struct buffer *));
static VOID buffer_append P((struct buffer *, int));
static char *buffer_extract P((struct buffer *));
#if 0
static VOID buffer_free P((struct buffer *));
#endif

#define buffer_length(buf) ((buf)->len)

#define NEW(type) ((type *)xmalloc(sizeof(type)))

static int current_lineno;
static char *current_file;
static FILE *fp;

struct replacement_table *make_replacement_table()
{
  int i;
  struct replacement_table *tablep;

  tablep = NEW(struct replacement_table);
  for (i = 0; i < TABLE_SIZE; i++)
    tablep->table[i] = 0;
  return tablep;
}

void load_replacement_file(tablep, file)
     struct replacement_table *tablep;
     char *file;
{
  int tok;
  struct buffer name;

  buffer_init(&name);
  errno = 0;
  fp = fopen(file, "r");
  if (!fp) {
    if (errno)
      error("can't open `%s': %s", file, strerror(errno));
    else
      error("can't open `%s'", file);
  }
      
  current_lineno = 1;
  current_file = file;
  tok = get_token();
  while (tok != EOF) {
    struct replacement *p;
    struct replacement_item **tail;
    enum event_type type;

    if (tok != STAGO && tok != ETAGO)
      parse_error("syntax error");
    type = tok == STAGO ? START_ELEMENT : END_ELEMENT;
    scan_name(&name, '>');
    p = define_replacement(tablep, type, buffer_extract(&name));
    tok = get_token();
    if (tok == PLUS) {
      if (p)
	p->flags |= NEWLINE_BEGIN;
      tok = get_token();
    }
    tail = p ? &p->items : 0;
    while (tok == STRING) {
      tail = parse_string(tail, type == START_ELEMENT);
      tok = get_token();
    }
    if (tok == PLUS) {
      if (p)
	p->flags |= NEWLINE_END;
      tok = get_token();
    }
  }
  fclose(fp);
}

static
struct replacement_item **parse_string(tail, recog_attr)
     struct replacement_item **tail;
     int recog_attr;
{
  struct buffer buf;
  unsigned len;
  
  buffer_init(&buf);
  for (;;) {
    int c = get();
    if (c == '\"')
      break;
    if (recog_attr && c == '[') {
      if (buffer_length(&buf)) {
	len = buffer_length(&buf);
	tail = add_replacement_data(tail, buffer_extract(&buf), len);
      }
      scan_name(&buf, ']');
      tail = add_replacement_attr(tail, buffer_extract(&buf));
    }
    else {
      if (c == '\\') {
	c = get();
	switch (c) {
	case EOF:
	  parse_error("unfinished string at end of file");
	case 's':
	  buffer_append(&buf, ' ');
	  break;
	case 'n':
	  buffer_append(&buf, '\n');
	  break;
	case 't':
	  buffer_append(&buf, '\t');
	  break;
	case 'r':
	  buffer_append(&buf, '\r');
	  break;
	case 'f':
	  buffer_append(&buf, '\f');
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
	    int val = c - '0';
	    c = peek();
	    if ('0' <= c && c <= '7') {
	      (void)get();
	      val = val*8 + (c - '0');
	      c = peek();
	      if ('0' <= c && c <= '7') {
		(void)get();
		val = val*8 + (c - '0');
	      }
	    }
	    buffer_append(&buf, val);
	    break;
	  }
	default:
	  buffer_append(&buf, c);
	  break;
	}
      }
      else
	buffer_append(&buf, c);
    }
  }
  len = buffer_length(&buf);
  if (len > 0)
    tail = add_replacement_data(tail, buffer_extract(&buf), len);
  return tail;
}

static
struct replacement_item **add_replacement_data(tail, buf, n)
     struct replacement_item **tail;
     char *buf;
     unsigned n;
{
  if (!tail)
    free(buf);
  else {
    *tail = NEW(struct replacement_item);
    (*tail)->type = DATA_REPL;
    (*tail)->u.data.n = n;
    (*tail)->next = 0;
    (*tail)->u.data.s = buf;
    tail = &(*tail)->next;
  }
  return tail;
}

static
struct replacement_item **add_replacement_attr(tail, name)
     struct replacement_item **tail;
     char *name;
{
  if (!tail)
    free(name);
  else {
    *tail = NEW(struct replacement_item);
    (*tail)->type = ATTR_REPL;
    (*tail)->next = 0;
    (*tail)->u.attr = name;
    tail = &(*tail)->next;
  }
  return tail;
}

static
int get_token()
{
  int c;

  for (;;) {
    c = get();
    while (isspace(c))
      c = get();
    if (c != '%')
      break;
    do {
      c = get();
      if (c == EOF)
	return EOF;
    } while (c != '\n');
  }
  switch (c) {
  case '+':
    return PLUS;
  case '<':
    c = peek();
    if (c == '/') {
      (void)get();
      return ETAGO;
    }
    return STAGO;
  case '"':
    return STRING;
  case EOF:
    return EOF;
  default:
    parse_error("bad input character `%c'", c);
  }
}

static
void scan_name(buf, term)
     struct buffer *buf;
     int term;
{
  int c;
  for (;;) {
    c = get();
    if (c == term)
      break;
    if (c == '\n' || c == EOF)
      parse_error("missing `%c'", term);
    if (fold_general_names) {
      if (islower((unsigned char)c))
	c = toupper((unsigned char)c);
    }
    buffer_append(buf, c);
  }
  if (buffer_length(buf) == 0)
    parse_error("empty name");
  buffer_append(buf, '\0');
}

static
int get()
{
  int c = getc(fp);
  if (c == '\n')
    current_lineno++;
  return c;
}

static
int peek()
{
  int c = getc(fp);
  if (c != EOF)
    ungetc(c, fp);
  return c;
}

struct replacement *lookup_replacement(tablep, type, name)
     struct replacement_table *tablep;
     enum event_type type;
     char *name;
{
  int h = hash(type, name);
  struct table_entry *p;
  
  for (p = tablep->table[h]; p; p = p->next)
    if (strcmp(name, p->gi) == 0 && type == p->type)
      return &p->replacement;
  return 0;
}

/* Return 0 if already defined. */

static
struct replacement *define_replacement(tablep, type, name)
     struct replacement_table *tablep;
     enum event_type type;
     char *name;
{
  int h = hash(type, name);
  struct table_entry *p;
  
  for (p = tablep->table[h]; p; p = p->next)
    if (strcmp(name, p->gi) == 0 && type == p->type)
      return 0;
  p = NEW(struct table_entry);
  p->next = tablep->table[h];
  tablep->table[h] = p;
  p->type = type;
  p->gi = name;
  p->replacement.flags = 0;
  p->replacement.items = 0;
  return &p->replacement;
}

static
VOID buffer_init(buf)
     struct buffer *buf;
{
  buf->size = buf->len = 0;
  buf->s = 0;
}

static
char *buffer_extract(buf)
     struct buffer *buf;
{
  char *s = buf->s;
  buf->s = 0;
  buf->len = 0;
  buf->size = 0;
  return s;
}

#if 0
static
VOID buffer_free(buf)
     struct buffer *buf;
{
  if (buf->s) {
    free((UNIV)buf->s);
    buf->s = 0;
    buf->size = buf->size = 0;
  }
}
#endif

static
VOID buffer_append(buf, c)
     struct buffer *buf;
     int c;
{
  if (buf->len >= buf->size) {
    if (!buf->size)
      buf->s = (char *)xmalloc(buf->size = 10);
    else
      buf->s = (char *)xrealloc((UNIV)buf->s, buf->size *= 2);
  }
  buf->s[buf->len] = c;
  buf->len += 1;
}

static
int hash(type, s)
     enum event_type type;
     char *s;
{
  unsigned long h = 0, g;
  
  while (*s != 0) {
    h <<= 4;
    h += *s++;
    if ((g = h & 0xf0000000) != 0) {
      h ^= g >> 24;
      h ^= g;
    }
  }
  h ^= (int)type;
  return (int)(h % TABLE_SIZE);
}

static
UNIV xmalloc(n)
     unsigned n;
{
  UNIV p = (UNIV)malloc(n);
  if (!p)
    parse_error("out of memory");
  return p;
}

static
UNIV xrealloc(p, size)
     UNIV p;
     unsigned size;
{
  p = (UNIV)realloc(p, size);
  if (!p)
    parse_error("out of memory");
  return p;
}
     
static NO_RETURN
#ifdef VARARGS
void parse_error(va_alist) va_dcl
#else
void parse_error(char *message,...)
#endif
{
  char buf[512];
#ifdef VARARGS
  char *message;
#endif
  va_list ap;
  
#ifdef VARARGS
  va_start(ap);
  message = va_arg(ap, char *);
#else
  va_start(ap, message);
#endif
  vsprintf(buf, message, ap);
  va_end(ap);
  error("%s:%d: %s", current_file, current_lineno, buf);
}
