/* xfprintf.c -
   X/Open extended v?fprintf implemented in terms of v?fprintf.

     Written by James Clark (jjc@jclark.com).
*/

/* Compile with:

   -DVARARGS			to use varargs.h instead of stdarg.h
   -DLONG_DOUBLE_MISSING 	if your compiler doesn't like `long double'
   -DFP_SUPPORT                 to include floating point stuff
*/

#include "config.h"

#ifndef HAVE_EXTENDED_PRINTF

#include "std.h"

#ifdef lint
/* avoid stupid lint warnings */
#undef va_arg
#define va_arg(ap, type) (ap, (type)0)
#endif

#ifdef FP_SUPPORT
#ifdef LONG_DOUBLE_MISSING
typedef double long_double;
#else
typedef long double long_double;
#endif
#endif /* FP_SUPPORT */

#ifdef USE_PROTOTYPES
#define P(parms) parms
#else
#define P(parms) ()
#endif

#ifdef VARARGS
typedef int (*printer)();
#else
typedef int (*printer)(UNIV, const char *, ...);
#endif

enum arg_type {
  NONE,
  INT,
  UNSIGNED,
  LONG,
  UNSIGNED_LONG,
#ifdef FP_SUPPORT
  DOUBLE,
  LONG_DOUBLE,
#endif /* FP_SUPPORT */
  PCHAR,
  PINT,
  PLONG,
  PSHORT
};

union arg {
  int i;
  unsigned u;
  long l;
  unsigned long ul;
#ifdef FP_SUPPORT
  double d;
  long_double ld;
#endif /* FP_SUPPORT */
  char *pc;
  UNIV pv;
  int *pi;
  short *ps;
  long *pl;
};

#define NEXT 0
#define MISSING 10

struct spec {
  enum arg_type type;
  char pos;
  char field_width;
  char precision;
};

#define FLAG_CHARS "-+ #0"

static int parse_spec P((const char **, struct spec *));
static int find_arg_types P((const char *, enum arg_type *));
static void get_arg P((enum arg_type, va_list *, union arg *));
static int do_arg P((UNIV, printer, const char *, enum arg_type, union arg *));
static int xdoprt P((UNIV, printer, const char *, va_list));
static int printit P((UNIV, printer, const char *, va_list, int, union arg *));
static int maybe_positional P((const char *));

/* Return 1 if sucessful, 0 otherwise. **pp points to character after % */

static int parse_spec(pp, sp)
const char **pp;
struct spec *sp;
{
  char modifier = 0;
  sp->pos = NEXT;
  if (isdigit((unsigned char)(**pp)) && (*pp)[1] == '$') {
    if (**pp == '0')
      return 0;
    sp->pos = **pp - '0';
    *pp += 2;
  }
  
  while (**pp != '\0' && strchr(FLAG_CHARS, **pp))
    *pp += 1;
  
  /* handle the field width */

  sp->field_width = MISSING;
  if (**pp == '*') {
    *pp += 1;
    if (isdigit((unsigned char)**pp) && (*pp)[1] == '$') {
      if (**pp == '0')
	return 0;
      sp->field_width = **pp - '0';
      *pp += 2;
    }
    else
      sp->field_width = NEXT;
  }
  else {
    while (isdigit((unsigned char)**pp))
      *pp += 1;
  }

  /* handle the precision */
  sp->precision = MISSING;
  if (**pp == '.') {
    *pp += 1;
    if (**pp == '*') {
      *pp += 1;
      if (isdigit((unsigned char)**pp) && (*pp)[1] == '$') {
	if (**pp == '0')
	  return 0;
	sp->precision = **pp - '0';
	*pp += 2;
      }
      else
	sp->precision = NEXT;
    }
    else {
      while (isdigit((unsigned char)**pp))
	*pp += 1;
    }
  }
  /* handle h l or L */

  if (**pp == 'h' || **pp == 'l' || **pp == 'L') {
    modifier = **pp;
    *pp += 1;
  }
  
  switch (**pp) {
  case 'd':
  case 'i':
    sp->type = modifier == 'l' ? LONG : INT;
    break;
  case 'o':
  case 'u':
  case 'x':
  case 'X':
    sp->type = modifier == 'l' ? UNSIGNED_LONG : UNSIGNED;
    break;
#ifdef FP_SUPPORT
  case 'e':
  case 'E':
  case 'f':
  case 'g':
  case 'G':
    sp->type = modifier == 'L' ? LONG_DOUBLE : DOUBLE;
    break;
#endif /* FP_SUPPORT */
  case 'c':
    sp->type = INT;
    break;
  case 's':
    sp->type = PCHAR;
    break;
  case 'p':
    /* a pointer to void has the same representation as a pointer to char */
    sp->type = PCHAR;
    break;
  case 'n':
    if (modifier == 'h')
      sp->type = PSHORT;
    else if (modifier == 'l')
      sp->type = PLONG;
    else
      sp->type = PINT;
    break;
  case '%':
    sp->type = NONE;
    break;
  default:
    return 0;
  }
  *pp += 1;
  return 1;
}


static int find_arg_types(format, arg_type)
     const char *format;
     enum arg_type *arg_type;
{
  int i, pos;
  const char *p;
  struct spec spec;
  
  for (i = 0; i < 9; i++)
    arg_type[i] = NONE;

  pos = 0;

  p = format;
  while (*p)
    if (*p == '%') {
      p++;
      if (!parse_spec(&p, &spec))
	return 0;
      if (spec.type != NONE) {
	int n;
	if (spec.pos == NEXT)
	  n = pos++;
	else
	  n = spec.pos - 1;
	if (n < 9) {
	  enum arg_type t = arg_type[n];
	  if (t != NONE && t != spec.type)
	    return 0;
	  arg_type[n] = spec.type;
	}
      }
      if (spec.field_width != MISSING) {
	int n;
	if (spec.field_width == NEXT)
	  n = pos++;
	else
	  n = spec.field_width - 1;
	if (n < 9) {
	  enum arg_type t = arg_type[n];
	  if (t != NONE && t != INT)
	    return 0;
	  arg_type[n] = INT;
	}
      }
      if (spec.precision != MISSING) {
	int n;
	if (spec.precision == NEXT)
	  n = pos++;
	else
	  n = spec.precision - 1;
	if (n < 9) {
	  enum arg_type t = arg_type[n];
	  if (t != NONE && t != INT)
	    return 0;
	  arg_type[n] = INT;
	}
      }
    }
    else
      p++;
  return 1;
}

static void get_arg(arg_type, app, argp)
     enum arg_type arg_type;
     va_list *app;
     union arg *argp;
{
  switch (arg_type) {
  case NONE:
    break;
  case INT:
    argp->i = va_arg(*app, int);
    break;
  case UNSIGNED:
    argp->u = va_arg(*app, unsigned);
    break;
  case LONG:
    argp->l = va_arg(*app, long);
    break;
  case UNSIGNED_LONG:
    argp->ul = va_arg(*app, unsigned long);
    break;
#ifdef FP_SUPPORT
  case DOUBLE:
    argp->d = va_arg(*app, double);
    break;
  case LONG_DOUBLE:
    argp->ld = va_arg(*app, long_double);
    break;
#endif /* FP_SUPPORT */
  case PCHAR:
    argp->pc = va_arg(*app, char *);
    break;
  case PINT:
    argp->pi = va_arg(*app, int *);
    break;
  case PSHORT:
    argp->ps = va_arg(*app, short *);
    break;
  case PLONG:
    argp->pl = va_arg(*app, long *);
    break;
  default:
    abort();
  }
}

static int do_arg(handle, func, buf, arg_type, argp)
     UNIV handle;
     printer func;
     const char *buf;
     enum arg_type arg_type;
     union arg *argp;
{
  switch (arg_type) {
  case NONE:
    return (*func)(handle, buf);
  case INT:
    return (*func)(handle, buf, argp->i);
  case UNSIGNED:
    return (*func)(handle, buf, argp->u);
  case LONG:
    return (*func)(handle, buf, argp->l);
  case UNSIGNED_LONG:
    return (*func)(handle, buf, argp->ul);
#ifdef FP_SUPPORT
  case DOUBLE:
    return (*func)(handle, buf, argp->d);
  case LONG_DOUBLE:
    return (*func)(handle, buf, argp->ld);
#endif /* FP_SUPPORT */
  case PCHAR:
    return (*func)(handle, buf, argp->pc);
  case PINT:
    return (*func)(handle, buf, argp->pi);
  case PSHORT:
    return (*func)(handle, buf, argp->ps);
  case PLONG:
    return (*func)(handle, buf, argp->pl);
  default:
    abort();
  }
  /* NOTREACHED */
}

static int printit(handle, func, p, ap, nargs, arg)
     UNIV handle;
     printer func;
     const char *p;
     va_list ap;
     int nargs;
     union arg *arg;
{
  char buf[512];		/* enough for a spec */
  int count = 0;
  int pos = 0;

  while (*p)
    if (*p == '%') {
      char *q;
      struct spec spec;
      const char *start;
      int had_field_width;
      union arg *argp;
      union arg a;
      int res;

      start = ++p;
      if (!parse_spec(&p, &spec))
	abort();		/* should have caught it in find_arg_types */
      
      buf[0] = '%';
      q = buf + 1;

      if (spec.pos != NEXT)
	start += 2;

      /* substitute in precision and field width if necessary */
      had_field_width = 0;
      while (start < p) {
	if (*start == '*') {
	  char c;
	  int n, val;

	  start++;
	  if (!had_field_width && spec.field_width != MISSING) {
	    c = spec.field_width;
	    had_field_width = 1;
	  }
	  else
	    c = spec.precision;
	  if (c == NEXT)
	    n = pos++;
	  else {
	    start += 2;
	    n = c - 1;
	  }
	  if (n >= nargs)
	    val = va_arg(ap, int);
	  else
	    val = arg[n].i;

	  /* ignore negative precision */
	  if (val >= 0 || q[-1] != '.') {
	    (void)sprintf(q, "%d", val);
	    q = strchr(q, '\0');
	  }
	}
	else
	  *q++ = *start++;
      }
      *q++ = '\0';

      argp = 0;
      if (spec.type != NONE) {
	int n = spec.pos == NEXT ? pos++ : spec.pos - 1;
	if (n >= nargs) {
	  get_arg(spec.type, &ap, &a);
	  argp = &a;
	}
	else
	  argp = arg + n;
      }

      res = do_arg(handle, func, buf, spec.type, argp);
      if (res < 0)
	return -1;
      count += res;
    }
    else {
      if ((*func)(handle, "%c", *p++) < 0)
	return -1;
      count++;
    }
  return count;
}

/* Do a quick check to see if it may contains any positional thingies. */

static int maybe_positional(format)
     const char *format;
{
  const char *p;

  p = format;
  for (;;) {
    p = strchr(p, '$');
    if (!p)
      return 0;
    if (p - format >= 2
	&& isdigit((unsigned char)p[-1])
	&& (p[-2] == '%' || p[-2] == '*'))
      break;			/* might be a positional thingy */
  }
  return 1;
}
   
static int xdoprt(handle, func, format, ap)
     UNIV handle;
     printer func;
     const char *format;
     va_list ap;
{
  enum arg_type arg_type[9];
  union arg arg[9];
  int nargs, i;

  if (!find_arg_types(format, arg_type))
    return -1;
  
  for (nargs = 0; nargs < 9; nargs++)
    if (arg_type[nargs] == NONE)
      break;

  for (i = nargs; i < 9; i++)
    if (arg_type[i] != NONE)
      return -1;
  
  for (i = 0; i < nargs; i++)
    get_arg(arg_type[i], &ap, arg + i);

  return printit(handle, func, format, ap, nargs, arg);
}

#ifdef VARARGS
static int do_fprintf(va_alist) va_dcl
#else
static int do_fprintf(UNIV p, const char *format,...)
#endif
{
#ifdef VARARGS
  UNIV p;
  const char *format;
#endif
  va_list ap;
  int res;

#ifdef VARARGS
  va_start(ap);
  p = va_arg(ap, UNIV);
  format = va_arg(ap, char *);
#else
  va_start(ap, format);
#endif

  res = vfprintf((FILE *)p, format, ap);
  va_end(ap);
  return res;
}

#ifdef VARARGS
int xfprintf(va_alist) va_dcl
#else
int xfprintf(FILE *fp, const char *format, ...)
#endif
{
#ifdef VARARGS
  FILE *fp;
  char *format;
#endif
  va_list ap;
  int res;

#ifdef VARARGS
  va_start(ap);
  fp = va_arg(ap, FILE *);
  format = va_arg(ap, char *);
#else
  va_start(ap, format);
#endif
  if (maybe_positional(format))
    res = xdoprt((UNIV)fp, do_fprintf, format, ap);
  else
    res = vfprintf(fp, format, ap);
  va_end(ap);
  return res;
}

int xvfprintf(fp, format, ap)
     FILE *fp;
     const char *format;
     va_list ap;
{
  int res;
  if (maybe_positional(format))
    res = xdoprt((UNIV)fp, do_fprintf, format, ap);
  else
    res = vfprintf(fp, format, ap);
  return res;
}

#endif /* not HAVE_EXTENDED_PRINTF */
