/* $RCSfile: util.h,v $$Revision: 1.1.1.1 $$Date: 1998/09/09 07:00:04 $
 *
 *    Copyright (c) 1991-1997, Larry Wall
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 * $Log: util.h,v $
 * Revision 1.1.1.1  1998/09/09 07:00:04  markm
 * Initial import of Perl5. The king is dead; long live the king!
 *
 */

/* is the string for makedir a directory name or a filename? */

#define fatal Myfatal

#define MD_DIR 0
#define MD_FILE 1

#ifdef SETUIDGID
    int		eaccess();
#endif

char	*getwd();
int	makedir();

char * cpy2 _(( char *to, char *from, int delim ));
char * cpytill _(( char *to, char *from, int delim ));
void growstr _(( char **strptr, int *curlen, int newlen ));
char * instr _(( char *big, char *little ));
char * safecpy _(( char *to, char *from, int len ));
char * savestr _(( char *str ));
void croak _(( char *pat, ... ));
void fatal _(( char *pat, ... ));
void warn  _(( char *pat, ... ));
int prewalk _(( int numit, int level, int node, int *numericptr ));

Malloc_t safemalloc _((MEM_SIZE nbytes));
Malloc_t safecalloc _((MEM_SIZE elements, MEM_SIZE size));
Malloc_t saferealloc _((Malloc_t where, MEM_SIZE nbytes));
Free_t   safefree _((Malloc_t where));
