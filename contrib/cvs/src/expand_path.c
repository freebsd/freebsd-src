/* expand_path.c -- expand environmental variables in passed in string
 *
 * The main routine is expand_path(), it is the routine that handles
 * the '~' character in four forms: 
 *     ~name
 *     ~name/
 *     ~/
 *     ~
 * and handles environment variables contained within the pathname
 * which are defined by:
 *     ${var_name}   (var_name is the name of the environ variable)
 *     $var_name     (var_name ends w/ non-alphanumeric char other than '_')
 */

#include "cvs.h"
#include <sys/types.h>

static char *expand_variable PROTO((char *env, char *file, int line));


/* User variables.  */

List *variable_list = NULL;

static void variable_delproc PROTO ((Node *));

static void
variable_delproc (node)
    Node *node;
{
    free (node->data);
}

/* Currently used by -s option; we might want a way to set user
   variables in a file in the $CVSROOT/CVSROOT directory too.  */

void
variable_set (nameval)
    char *nameval;
{
    char *p;
    char *name;
    Node *node;

    p = nameval;
    while (isalnum (*p) || *p == '_')
	++p;
    if (*p != '=')
	error (1, 0, "illegal character in user variable name in %s", nameval);
    if (p == nameval)
	error (1, 0, "empty user variable name in %s", nameval);
    name = xmalloc (p - nameval + 1);
    strncpy (name, nameval, p - nameval);
    name[p - nameval] = '\0';
    /* Make p point to the value.  */
    ++p;
    if (strchr (p, '\012') != NULL)
	error (1, 0, "linefeed in user variable value in %s", nameval);

    if (variable_list == NULL)
	variable_list = getlist ();

    node = findnode (variable_list, name);
    if (node == NULL)
    {
	node = getnode ();
	node->type = VARIABLE;
	node->delproc = variable_delproc;
	node->key = name;
	node->data = xstrdup (p);
	(void) addnode (variable_list, node);
    }
    else
    {
	/* Replace the old value.  For example, this means that -s
	   options on the command line override ones from .cvsrc.  */
	free (node->data);
	node->data = xstrdup (p);
	free (name);
    }
}

/* This routine will expand the pathname to account for ~ and $
    characters as described above.  If an error occurs, an error
    message is printed via error() and NULL is returned.  FILE and
    LINE are the filename and linenumber to include in the error
    message.  */
char *
expand_path (name, file, line)
    char *name;
    char *file;
    int line;
{
    char *s;
    char *d;
    /* FIXME: arbitrary limit.  */
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
		if (flag
		    ? *s =='}'
		    : isalnum (*s) == 0 && *s != '_')
		    break;
	    *--d = 0;
	    e = expand_variable (&p[flag], file, line);

	    if (e)
	    {
		for (d = &p[-1]; (*d++ = *e++);)
		    ;
		--d;
		if (flag && *s)
		    s++;
	    }
	    else
		/* expand_variable has already printed an error message.  */
		return NULL;
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
	    t = get_homedir ();
	else
	{
	    struct passwd *ps;
	    for (; *p!='/' && *p; p++)
		;
	    *p = 0;
	    ps = getpwnam (s);
	    if (ps == 0)
	    {
		if (line != 0)
		    error (0, 0, "%s:%d: no such user %s",
			   file, line, s);
		else
		    error (0, 0, "%s: no such user %s", file, s);
		return NULL;
	    }
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
expand_variable (name, file, line)
    char *name;
    char *file;
    int line;
{
    if (strcmp (name, CVSROOT_ENV) == 0)
	return CVSroot;
    else if (strcmp (name, RCSBIN_ENV)  == 0)
	return Rcsbin;
    else if (strcmp (name, EDITOR1_ENV) == 0)
	return Editor;
    else if (strcmp (name, EDITOR2_ENV) == 0)
	return Editor;
    else if (strcmp (name, EDITOR3_ENV) == 0)
	return Editor;
    else if (strcmp (name, "USER") == 0)
	return getcaller ();
    else if (isalpha (name[0]))
    {
	/* These names are reserved for future versions of CVS,
	   so that is why it is an error.  */
	if (line != 0)
	    error (0, 0, "%s:%d: no such internal variable $%s",
		   file, line, name);
	else
	    error (0, 0, "%s: no such internal variable $%s",
		   file, name);
	return NULL;
    }
    else if (name[0] == '=')
    {
	Node *node;
	/* Crazy syntax for a user variable.  But we want
	   *something* that lets the user name a user variable
	   anything he wants, without interference from
	   (existing or future) internal variables.  */
	node = findnode (variable_list, name + 1);
	if (node == NULL)
	{
	    if (line != 0)
		error (0, 0, "%s:%d: no such user variable ${%s}",
		       file, line, name);
	    else
		error (0, 0, "%s: no such user variable ${%s}",
		       file, name);
	    return NULL;
	}
	return node->data;
    }
    else
    {
	/* It is an unrecognized character.  We return an error to
	   reserve these for future versions of CVS; it is plausible
	   that various crazy syntaxes might be invented for inserting
	   information about revisions, branches, etc.  */
	if (line != 0)
	    error (0, 0, "%s:%d: unrecognized varaible syntax %s",
		   file, line, name);
	else
	    error (0, 0, "%s: unrecognized varaible syntax %s",
		   file, name);
	return NULL;
    }
}
