/* device.c */

#include <stdio.h>
#include <ctype.h>

#include <X11/Xos.h>
#include <X11/Intrinsic.h>

#include "device.h"

#ifndef FONTPATH
#define FONTPATH "/usr/local/lib/groff/font:/usr/local/lib/font:/usr/lib/font"
#endif

extern void exit();
extern char *strtok(), *strchr();
extern char *getenv();

/* Name of environment variable containing path to be used for
searching for device and font description files. */
#define FONTPATH_ENV_VAR  "GROFF_FONT_PATH"

#define WS " \t\r\n"

/* Minimum and maximum values a `signed int' can hold.  */
#define INT_MIN (-INT_MAX-1)
#define INT_MAX 2147483647

#define CHAR_TABLE_SIZE 307

struct _DeviceFont {
    char *name;
    int special;
    DeviceFont *next;
    Device *dev;
    struct charinfo *char_table[CHAR_TABLE_SIZE];
    struct charinfo *code_table[256];
};

struct charinfo {
    int width;
    int code;
    struct charinfo *next;
    struct charinfo *code_next;
    char name[1];
};

static char *current_filename = 0;
static int current_lineno = -1;

static void error();
static FILE *open_device_file();
static DeviceFont *load_font();
static Device *new_device();
static DeviceFont *new_font();
static void delete_font();
static unsigned hash_name();
static struct charinfo *add_char();
static int read_charset_section();
static char *canonicalize_name();

static
Device *new_device(name)
    char *name;
{
    Device *dev;

    dev = XtNew(Device);
    dev->sizescale = 1;
    dev->res = 0;
    dev->unitwidth = 0;
    dev->fonts = 0;
    dev->X11 = 0;
    dev->paperlength = 0;
    dev->paperwidth = 0;
    dev->name = XtNewString(name);
    return dev;
}

void device_destroy(dev)
    Device *dev;
{
    DeviceFont *f;
    
    if (!dev)
	return;
    f = dev->fonts;
    while (f) {
	DeviceFont *tem = f;
	f = f->next;
	delete_font(tem);
    }
    
    XtFree(dev->name);
    XtFree((char *)dev);
}

Device *device_load(name)
    char *name;
{
    Device *dev;
    FILE *fp;
    int err = 0;
    char buf[256];

    fp = open_device_file(name, "DESC", &current_filename);
    if (!fp)
	return 0;
    dev = new_device(name);
    current_lineno = 0;
    while (fgets(buf, sizeof(buf), fp)) {
	char *p;
	current_lineno++;
	p = strtok(buf, WS);
	if (p) {
	    int *np = 0;
	    char *q;

	    if (strcmp(p, "charset") == 0)
		break;
	    if (strcmp(p, "X11") == 0)
		dev->X11 = 1;
	    else if (strcmp(p, "sizescale") == 0)
		np = &dev->sizescale;
 	    else if (strcmp(p, "res") == 0)
		np = &dev->res;
 	    else if (strcmp(p, "unitwidth") == 0)
		np = &dev->unitwidth;
 	    else if (strcmp(p, "paperwidth") == 0)
		np = &dev->paperwidth;
 	    else if (strcmp(p, "paperlength") == 0)
		np = &dev->paperlength;
	    
	    if (np) {
		q = strtok((char *)0, WS);
		if (!q || sscanf(q, "%d", np) != 1 || *np <= 0) {
		    error("bad argument");
		    err = 1;
		    break;
		}
	    }	
	}
    }
    fclose(fp);
    current_lineno = -1;
    if (!err) {
	if (dev->res == 0) {
	    error("missing res line");
	    err = 1;
	}
	else if (dev->unitwidth == 0) {
	    error("missing unitwidth line");
	    err = 1;
	}
    }
    if (dev->paperlength == 0)
	dev->paperlength = dev->res*11;
    if (dev->paperwidth == 0)
	dev->paperwidth = dev->res*8 + dev->res/2;
    if (err) {
	device_destroy(dev);
	dev = 0;
    }
    XtFree(current_filename);
    current_filename = 0;
    return dev;
}


DeviceFont *device_find_font(dev, name)
    Device *dev;
    char *name;
{
    DeviceFont *f;

    if (!dev)
	return 0;
    for (f = dev->fonts; f; f = f->next)
	if (strcmp(f->name, name) == 0)
	    return f;
    return load_font(dev, name);
}

static
DeviceFont *load_font(dev, name)
    Device *dev;
    char *name;
{
    FILE *fp;
    char buf[256];
    DeviceFont *f;
    int special = 0;

    fp = open_device_file(dev->name, name, &current_filename);
    if (!fp)
	return 0;
    current_lineno = 0;
    for (;;) {
	char *p;

	if (!fgets(buf, sizeof(buf), fp)) {
	    error("no charset line");
	    return 0;
	}
	current_lineno++;
	p = strtok(buf, WS);
	/* charset must be on a line by itself */
	if (p && strcmp(p, "charset") == 0 && strtok((char *)0, WS) == 0)
	    break;
	if (p && strcmp(p, "special") == 0)
	    special = 1;
    }
    f = new_font(name, dev);
    f->special = special;
    if (!read_charset_section(f, fp)) {
	delete_font(f);
	f = 0;
    }
    else {
	f->next = dev->fonts;
	dev->fonts = f;
    }
    fclose(fp);
    XtFree(current_filename);
    current_filename = 0;
    return f;
}

static
DeviceFont *new_font(name, dev)
    char *name;
    Device *dev;
{
    int i;
    DeviceFont *f;

    f = XtNew(DeviceFont);
    f->name = XtNewString(name);
    f->dev = dev;
    f->special = 0;
    f->next = 0;
    for (i = 0; i < CHAR_TABLE_SIZE; i++)
	f->char_table[i] = 0;
    for (i = 0; i < 256; i++)
	f->code_table[i] = 0;
    return f;
}

static
void delete_font(f)
    DeviceFont *f;
{
    int i;

    if (!f)
	return;
    XtFree(f->name);
    for (i = 0; i < CHAR_TABLE_SIZE; i++) {
	struct charinfo *ptr = f->char_table[i];
	while (ptr) {
	    struct charinfo *tem = ptr;
	    ptr = ptr->next;
	    XtFree((char *)tem);
	}
    }
    XtFree((char *)f);
}


static
unsigned hash_name(name)
    char *name;
{
    unsigned n = 0;
    /* XXX do better than this */
    while (*name)
	n = (n << 1) ^ *name++;

    return n;
}

static
int scale_round(n, x, y)
    int n, x, y;
{
  int y2;

  if (x == 0)
    return 0;
  y2 = y/2;
  if (n >= 0) {
    if (n <= (INT_MAX - y2)/x)
      return (n*x + y2)/y;
  }
  else if (-(unsigned)n <= (-(unsigned)INT_MIN - y2)/x)
      return (n*x - y2)/y;
  return (int)(n*(double)x/(double)y + .5);
}

static
char *canonicalize_name(s)
    char *s;
{
    static char ch[2];
    if (s[0] == 'c' && s[1] == 'h' && s[2] == 'a' && s[3] == 'r') {
	char *p;
	int n;

	for (p = s + 4; *p; p++)
	    if (!isascii(*p) || !isdigit(*p))
		return s;
	n = atoi(s + 4);
	if (n >= 0 && n <= 0xff) {
	    ch[0] = (char)n;
	    return ch;
	}
    }
    return s;
}

/* Return 1 if the character is present in the font; widthp gets the
width if non-null. */

int device_char_width(f, ps, name, widthp)
    DeviceFont *f;
    int ps;
    char *name;
    int *widthp;
{
    struct charinfo *p;

    name = canonicalize_name(name);
    for (p = f->char_table[hash_name(name) % CHAR_TABLE_SIZE];; p = p->next) {
	if (!p)
	    return 0;
	if (strcmp(p->name, name) == 0)
	    break;
    }
    *widthp = scale_round(p->width, ps, f->dev->unitwidth);
    return 1;
}

int device_code_width(f, ps, code, widthp)
    DeviceFont *f;
    int ps;
    int code;
    int *widthp;
{
    struct charinfo *p;

    for (p = f->code_table[code & 0xff];; p = p->code_next) {
	if (!p)
	    return 0;
	if (p->code == code)
	    break;
    }
    *widthp = scale_round(p->width, ps, f->dev->unitwidth);
    return 1;
}

char *device_name_for_code(f, code)
    DeviceFont *f;
    int code;
{
    static struct charinfo *state = 0;
    if (f)
	state = f->code_table[code & 0xff];
    for (; state; state = state->code_next)
	if (state->code == code && state->name[0] != '\0') {
	    char *name = state->name;
	    state = state->code_next;
	    return name;
	}
    return 0;
}

int device_font_special(f)
    DeviceFont *f;
{
    return f->special;
}
    
static
struct charinfo *add_char(f, name, width, code)
    DeviceFont *f;
    char *name;
    int width, code;
{
    struct charinfo **pp;
    struct charinfo *ci;
    
    name = canonicalize_name(name);
    if (strcmp(name, "---") == 0)
	name = "";

    ci = (struct charinfo *)XtMalloc(XtOffsetOf(struct charinfo, name[0])
				     + strlen(name) + 1);
    
    strcpy(ci->name, name);
    ci->width = width;
    ci->code = code;
    
    if (*name != '\0') {
	pp = &f->char_table[hash_name(name) % CHAR_TABLE_SIZE];
	ci->next = *pp;
	*pp = ci;
    }
    pp = &f->code_table[code & 0xff];
    ci->code_next = *pp;
    *pp = ci;
    return ci;
}

/* Return non-zero for success. */

static
int read_charset_section(f, fp)
    DeviceFont *f;
    FILE *fp;
{
    struct charinfo *last_charinfo = 0;
    char buf[256];

    while (fgets(buf, sizeof(buf), fp)) {
	char *name;
	int width;
	int code;
	char *p;

	current_lineno++;
	name = strtok(buf, WS);
	if (!name)
	    continue;		/* ignore blank lines */
	p = strtok((char *)0, WS);
	if (!p)			/* end of charset section */
	    break;
	if (strcmp(p, "\"") == 0) {
	    if (!last_charinfo) {
		error("first line of charset section cannot use `\"'");
		return 0;
	    }
	    else
		(void)add_char(f, name,
			       last_charinfo->width, last_charinfo->code);
	}
	else {
	    char *q;
	    if (sscanf(p, "%d", &width) != 1) {
		error("bad width field");
		return 0;
	    }
	    p = strtok((char *)0, WS);
	    if (!p) {
		error("missing type field");
		return 0;
	    }
	    p = strtok((char *)0, WS);
	    if (!p) {
		error("missing code field");
		return 0;
	    }
	    code = (int)strtol(p, &q, 0);
	    if (q == p) {
		error("bad code field");
		return 0;
	    }
	    last_charinfo = add_char(f, name, width, code);
	}
    }
    return 1;
}

static
FILE *find_file(file, path, result)
    char *file, *path, **result;
{
  char *buf = NULL;
  int bufsiz = 0;
  int flen;
  FILE *fp;
  
  *result = NULL;
  
  if (file == NULL)
    return NULL;
  if (*file == '\0')
    return NULL;
  
  if (*file == '/') {
    fp = fopen(file, "r");
    if (fp)
      *result = XtNewString(file);
    return fp;
  }
  
  flen = strlen(file);
  
  if (!path)
    return NULL;
  
  while (*path) {
    int len;
    char *start, *end;
    
    start = path;
    end = strchr(path, ':');
    if (end)
      path = end + 1;
    else
      path = end = strchr(path, '\0');
    if (start >= end)
      continue;
    if (end[-1] == '/')
      --end;
    len = (end - start) + 1 + flen + 1;
    if (len > bufsiz) {
      if (buf)
	buf = XtRealloc(buf, len);
      else
	buf = XtMalloc(len);
      bufsiz = len;
    }
    memcpy(buf, start, end - start);
    buf[end - start] = '/';
    strcpy(buf + (end - start) + 1, file);
    fp = fopen(buf, "r");
    if (fp) {
      *result = buf;
      return fp;
    }
  }
  XtFree(buf);
  return NULL;
}

static
FILE *open_device_file(device_name, file_name, result)
     char *device_name, *file_name, **result;
{
  char *buf, *path;
  FILE *fp;

  buf = XtMalloc(3 + strlen(device_name) + 1 + strlen(file_name) + 1);
  sprintf(buf, "dev%s/%s", device_name, file_name);
  path = getenv(FONTPATH_ENV_VAR);
  if (!path)
    path = FONTPATH;
  fp = find_file(buf, path, result);
  if (!fp) {
      fprintf(stderr, "can't find device file `%s'\n", file_name);
      fflush(stderr);
  }
  XtFree(buf);
  return fp;
}

static
void error(s)
    char *s;
{
    if (current_filename) {
	fprintf(stderr, "%s:", current_filename);
	if (current_lineno > 0)
	    fprintf(stderr, "%d:", current_lineno);
	putc(' ', stderr);
    }
    fputs(s, stderr);
    putc('\n', stderr);
    fflush(stderr);
}

/*
Local Variables:
c-indent-level: 4
c-continued-statement-offset: 4
c-brace-offset: -4
c-argdecl-indent: 4
c-label-offset: -4
c-tab-always-indent: nil
End:
*/
