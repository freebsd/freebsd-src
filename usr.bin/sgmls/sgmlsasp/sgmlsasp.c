/* sgmlsasp.c
   Translate sgmls output using ASP replacement file.

   Written by James Clark (jjc@jclark.com). */

#include "sgmlsasp.h"
#include "sgmls.h"
#include "replace.h"
#include "getopt.h"

/* Non-zero if general (non-entity) names should be folded to upper case. */
int fold_general_names = 1;

static char *program_name;
static char last_char = '\n';

static void output_begin_line P((void));
static void output_data P((struct sgmls_data *, int));
static void output_pi P((char *, unsigned));
static void output_token P((char *));
static void output_attribute P((struct sgmls_attribute *));
static void output_data_char P((int));
static void output_replacement
  P((struct replacement *, struct sgmls_attribute *));
static void do_file P((FILE *, struct replacement_table *));
static void usage P((void));
static void input_error P((int, char *, unsigned long));

#define output_char(c) (last_char = (c), putchar(c))

int main(argc, argv)
     int argc;
     char **argv;
{
  struct replacement_table *tablep;
  int i;
  int opt;
  program_name = argv[0];

  while ((opt = getopt(argc, argv, "n")) != EOF)
    switch (opt) {
    case 'n':
      fold_general_names = 0;
      break;
    case '?':
      usage();
    default:
      assert(0);
    }
  if (argc - optind <= 0)
    usage();
  tablep = make_replacement_table();
  for (i = optind; i < argc; i++)
    load_replacement_file(tablep, argv[i]);
  (void)sgmls_set_errhandler(input_error);
  do_file(stdin, tablep);
  exit(0);
}

static
void usage()
{
  fprintf(stderr, "usage: %s [-n] replacement_file...\n", program_name);
  exit(1);
}

static
void input_error(num, str, lineno)
     int num;
     char *str;
     unsigned long lineno;
{
  error("Error at input line %lu: %s", lineno, str);
}

static
void do_file(fp, tablep)
     FILE *fp;
     struct replacement_table *tablep;
{
  struct sgmls *sp;
  struct sgmls_event e;

  sp = sgmls_create(fp);
  while (sgmls_next(sp, &e))
    switch (e.type) {
    case SGMLS_EVENT_DATA:
      output_data(e.u.data.v, e.u.data.n);
      break;
    case SGMLS_EVENT_ENTITY:
      /* XXX what should we do here? */
      break;
    case SGMLS_EVENT_PI:
      output_pi(e.u.pi.s, e.u.pi.len);
      break;
    case SGMLS_EVENT_START:
      output_replacement(lookup_replacement(tablep,
					    START_ELEMENT, e.u.start.gi),
			 e.u.start.attributes);
      sgmls_free_attributes(e.u.start.attributes);
      break;
    case SGMLS_EVENT_END:
      output_replacement(lookup_replacement(tablep, END_ELEMENT, e.u.end.gi),
			 0);
      break;
    case SGMLS_EVENT_SUBSTART:
      break;
    case SGMLS_EVENT_SUBEND:
      break;
    case SGMLS_EVENT_APPINFO:
      break;
    case SGMLS_EVENT_CONFORMING:
      break;
    default:
      abort();
    }
  sgmls_free(sp);
}

static
void output_data(v, n)
struct sgmls_data *v;
int n;
{
  int i;

  for (i = 0; i < n; i++) {
    char *s = v[i].s;
    int len = v[i].len;
    for (; len > 0; len--, s++)
      output_data_char(*s);
  }
}

static
void output_pi(s, len)
     char *s;
     unsigned len;
{
  for (; len > 0; len--, s++)
    output_data_char(*s);
}

static
void output_replacement(repl, attributes)
struct replacement *repl;
struct sgmls_attribute *attributes;
{
  struct replacement_item *p;
  struct sgmls_attribute *a;
  int i;

  if (!repl)
    return;
  if (repl->flags & NEWLINE_BEGIN)
    output_begin_line();
  
  for (p = repl->items; p; p = p->next)
    switch (p->type) {
    case DATA_REPL:
      for (i = 0; i < p->u.data.n; i++)
	output_char(p->u.data.s[i]);
      break;
    case ATTR_REPL:
      for (a = attributes; a; a = a->next)
	if (strcmp(a->name, p->u.attr) == 0) {
	  output_attribute(a);
	  break;
	}
      break;
    default:
      abort();
    }

  if (repl->flags & NEWLINE_END)
    output_begin_line();
}

static
void output_attribute(p)
struct sgmls_attribute *p;
{
  switch (p->type) {
  case SGMLS_ATTR_IMPLIED:
    break;
  case SGMLS_ATTR_CDATA:
    output_data(p->value.data.v, p->value.data.n);
    break;
  case SGMLS_ATTR_TOKEN:
    {
      char **token = p->value.token.v;
      int n = p->value.token.n;
      
      if (n > 0) {
	int i;
	output_token(token[0]);
	for (i = 1; i < n; i++) {
	  output_char(' ');
	  output_token(token[i]);
	}
      }
    }
    break;
  case SGMLS_ATTR_ENTITY:
    {
      struct sgmls_entity **v = p->value.entity.v;
      int n = p->value.entity.n;
      int i;

      for (i = 0; i < n; i++) {
	if (i > 0)
	  output_char(' ');
	output_token(v[i]->is_internal
		     ? v[i]->u.internal.name
		     : v[i]->u.external.name);
      }
    }
    break;
  case SGMLS_ATTR_NOTATION:
    if (p->value.notation)
      output_token(p->value.notation->name);
    break;
  default:
    abort();
  }
}

static
void output_token(s)
     char *s;
{
  for (; *s; s++)
    output_char(*s);
}

static
void output_data_char(c)
     int c;
{
  if (c != RSCHAR) {
    if (c == RECHAR)
      c = '\n';
    output_char(c);
  }
}

static
void output_begin_line()
{
  if (last_char != '\n')
    output_char('\n');
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
