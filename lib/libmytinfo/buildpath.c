/*
 * buildpath.c
 * 
 * By Ross Ridge
 * Public Domain
 * 92/02/01 07:29:42
 *
 * _buildpath builds a list of file names and terminal descriprions extracted
 * from its arguments. It returns a pointer to a structure that is used by
 * other routines as the list of file names to search for terminal
 * descriptions.  It is passed a variable number of arguments consisting
 * of file name and type pairs. The file name can actually be a list of 
 * file names seperated by spaces and any environment variables specified
 * by a dollar sign ($) followed by its name are substituted in. A type
 * of 1 indicates that the file name may actually be termcap description
 * and a type of 2 indicates it may be a terminfo description. A type of 0
 * indicates that the file name can only be a file name (or list of them).
 *
 */

#include "defs.h"

#include <ctype.h>

#ifdef USE_SCCS_IDS
static const char SCCSid[] = "@(#) mytinfo buildpath.c 3.2 92/02/01 public domain, By Ross Ridge";
#endif

/* more memory is allocated for file names every HUNK file names */
#define HUNK 32	

/* characters that seperate file names in a list */
#define SEPERATORS " :"

static struct term_path *path = NULL;	/* the list of files */
static int files = 0;			/* # of files in the list */
static int size = 0;			/* # of files there is space for */

/* add a file name, type pair to the list */
static int
addfile(file, type)
char *file;
int type; {
	int l;
	char *s;

	if (file == NULL) {
		if (type != -1)
			return -1;
	} else if (file[0] == '\0')
		return -1;

#ifdef DEBUG
	if (file != NULL)
		printf("addfile: %s\n", file);
#endif

	if (files >= size) {
		size += HUNK;
		if (path == NULL)
			path = (struct term_path *) 
				malloc(size * sizeof(struct term_path));
		else
			path = (struct term_path *)
				realloc((anyptr) path,
					size * sizeof(struct term_path));
		if (path == NULL)
			return 0;
	}
	if (file == NULL) {
		path[files].file = file;
	} else {
		l = strlen(file) + 1;
		s = (char *) malloc(l * sizeof(char));
		if (s == NULL)
			return 0;
		path[files].file = strcpy(s, file);
	}
	path[files].type = type;
	
	return ++files;
}

/* deallocate space used by the path list */
void
_delpath(ppath)
struct term_path *ppath; {
	struct term_path *p;

	p = ppath;
	while(p->file != NULL) {
		free((anyptr)p->file);
		p++;
	}

	free((anyptr)ppath);
}

/* build a list of paths. see above */
#ifdef lint
/*VARARGS2*/
struct term_path *
_buildpath(file, type)
char *file;
int type;
#else
#ifdef USE_STDARG
#ifdef USE_PROTOTYPES
struct term_path *_buildpath(char *file, int type, ...)
#else
struct term_path *_buildpath(file, type)
char *file;
int type;
#endif /* USE_PROTOTYPES */
#else /* USE_STDARG */
struct term_path *_buildpath(va_alist)
va_dcl
#endif /* USE_STDARG */
#endif /* lint */
{
#ifndef lint
#ifndef USE_STDARG
	char *file;
	int type;
#endif
#endif
	va_list ap;
	register char *s, *d, *e;
	char *p;
	char line[MAX_BUF+1];
	char name[MAX_NAME+1];
	int i,j;

	size = 0;
	files = 0;
	path = NULL;

#ifdef lint
	ap = NULL;
#else
#ifdef USE_STDARG
	va_start(ap, type);
#else
	va_start(ap);
	file = va_arg(ap, char *);
	type = va_arg(ap, int);
#endif
#endif

	while (type >= 0 && type <= 2) {
		s = file;
		d = line;
		i = 0;
		while(*s != '\0') {
			if (*s == '$') {
				s++;
				j = 0;
				while(*s != '\0' && (*s == '_' || isalnum(*s))) 
					if (j < MAX_NAME) {
						name[j] = *s++;
						j++;
					} else
						break;
				name[j] = '\0';
				e = getenv(name);
				if (e != NULL) {
					while(*e != '\0') {
						if (i < MAX_BUF) {
							*d++ = *e++;
							i++;
						} else
							break;
					}
				} else if (*s == '/') 
					s++;
			} else {
				if (i < MAX_BUF) {
					*d++ = *s++;
					i++;
				} else
					break;
			}
		}
		*d = '\0';
		if (type == 0 || line[0] == '/') {
			p = line;
			while ((s = strsep(&p, SEPERATORS)) != NULL && *s == '\0')
				;
			while(s != NULL) {
				if (addfile(s, 0) == 0)
					return NULL;
				while ((s = strsep(&p, SEPERATORS)) != NULL && *s == '\0')
					;
			}
		} else 
			if (addfile(line, type) == 0)
				return NULL;
		file = va_arg(ap, char *);
		type = va_arg(ap, int);
	}
	addfile(NULL, -1);
	return path;
}
