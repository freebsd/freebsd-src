/* expand_path.c -- expand environmental variables in passed in string
 	The main routine is expand_pathname, it is the routine
	that handles the '~' character in four forms:
		~name
		~name/
		~/
		~
	and handles environment variables contained within the pathname
	which are defined by:
		c is some character
		${var_name}	var_name is the name of the environ variable
		$var_name	var_name ends with a non ascii character
	char *expand_pathname(char *name)
	This routine will expand the pathname to account for ~
	and $ characters as described above.If an error occurs, NULL
	is returned.
	Will only expand Built in CVS variables all others are ignored.
 */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include "cvs.h"
#include <stdio.h>
#include <ctype.h>
#include <sys/types.h>
#if HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
static char *expand_variable PROTO((char *env));
extern char *xmalloc ();
extern void  free ();
char *
expand_path (name)
    char *name;
{
	char *s;
	char *d;
	char  mybuf[PATH_MAX];
	char  buf[PATH_MAX];
	char *result;
	s = name;
	d = mybuf;
	while ((*d++ = *s))
		if (*s++ == '$')
		{
			char *p = d;
			char *e;
			int flag = (*s == '{');
			
			for (; (*d++ = *s); s++)
			    if (flag ? *s =='}' :
					isalnum (*s) == 0 && *s!='_' )
				break;
			*--d = 0;
			e = expand_variable (&p[flag]);
			
			if (e)
			{
			    for (d = &p[-1]; (*d++ = *e++);)
				;
			    --d;
			    if (flag && *s)
				s++;
			}
			else
			    return NULL;	/* no env variable */
		}
	*d = 0;
	 s = mybuf;
	 d = buf;
	/* If you don't want ~username ~/ to be expanded simply remove
	 * This entire if statement including the else portion
	 */
	if (*s++ == '~')
	{
		char *t;
		char *p=s;
		if (*s=='/' || *s==0)
		    t = getenv ("HOME");
		else
		{
			struct passwd *ps;
			for (; *p!='/' && *p; p++)
			    ;
			*p = 0;
			ps = getpwnam (s);
			if (ps == 0)
				return NULL;   /* no such user */
			t = ps->pw_dir;
		    }
		while ((*d++ = *t++))
		    ;
		--d;
		if (*p == 0)
			*p = '/';	       /* always add / */
		s=p;
	}
	else
	    --s;
	    /* Kill up to here */
	while ((*d++ = *s++))
	    ;
	*d=0;
	result = xmalloc (sizeof(char) * strlen(buf)+1);
	strcpy (result, buf);
	return result;
}
static char *
expand_variable (name)
	char *name;
{
	/* There is nothing expanding this function to allow it
	 * to read a file in the $CVSROOT/CVSROOT directory that
	 * says which environmental variables could be expanded
	 * or just say everything is fair game to be expanded
	 */
	if ( strcmp (name, CVSROOT_ENV) == 0 )
		return CVSroot;
	else
	if ( strcmp (name, RCSBIN_ENV)  == 0 )
		return Rcsbin;
	else
	if ( strcmp (name, EDITOR1_ENV) == 0 )
		return Editor;
	else
	if ( strcmp (name, EDITOR2_ENV) == 0 )
		return Editor;
	else
	if ( strcmp (name, EDITOR3_ENV) == 0 )
		return Editor;
	else
		return NULL;
		/* The code here could also just
		 * return whatever getenv would
		 * return.
		 */
}
