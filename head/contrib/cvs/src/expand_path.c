/* expand_path.c -- expand environmental variables in passed in string
 *
 * Copyright (C) 1995-2005 The Free Software Foundation, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
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

static char *expand_variable PROTO((const char *env, const char *file,
                                    int line));



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
    while (isalnum ((unsigned char) *p) || *p == '_')
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
   characters as described above.  Returns a pointer to a newly
   malloc'd string.  If an error occurs, an error message is printed
   via error() and NULL is returned.  FILE and LINE are the filename
   and linenumber to include in the error message.  FILE must point
   to something; LINE can be zero to indicate the line number is not
   known.  */
char *
expand_path (name, file, line)
    const char *name;
    const char *file;
    int line;
{
    size_t s, d, p;
    char *e;

    char *mybuf = NULL;
    size_t mybuf_size = 0;
    char *buf = NULL;
    size_t buf_size = 0;

    char *result;

    /* Sorry this routine is so ugly; it is a head-on collision
       between the `traditional' unix *d++ style and the need to
       dynamically allocate.  It would be much cleaner (and probably
       faster, not that this is a bottleneck for CVS) with more use of
       strcpy & friends, but I haven't taken the effort to rewrite it
       thusly.  */

    /* First copy from NAME to MYBUF, expanding $<foo> as we go.  */
    s = d = 0;
    while (name[s] != '\0')
    {
	if (name[s] == '$')
	{
	    p = d;
	    if (name[++s] == '{')
	    {
		while (name[++s] != '}' && name[s] != '\0')
		{
		    expand_string (&mybuf, &mybuf_size, p + 1);
		    mybuf[p++] = name[s];
		}
		if (name[s] != '\0') ++s;
	    }
	    else
	    {
		while (isalnum ((unsigned char) name[s]) || name[s] == '_')
		{
		    expand_string (&mybuf, &mybuf_size, p + 1);
		    mybuf[p++] = name[s++];
		}
	    }
	    expand_string (&mybuf, &mybuf_size, p + 1);
	    mybuf[p] = '\0';
	    e = expand_variable (mybuf + d, file, line);

	    if (e)
	    {
		p = strlen(e);
		expand_string (&mybuf, &mybuf_size, d + p);
		memcpy(mybuf + d, e, p);
		d += p;
	    }
	    else
		/* expand_variable has already printed an error message.  */
		goto error_exit;
	}
	else
	{
	    expand_string (&mybuf, &mybuf_size, d + 1);
	    mybuf[d++] = name[s++];
	}
    }
    expand_string (&mybuf, &mybuf_size, d + 1);
    mybuf[d++] = '\0';

    /* Then copy from MYBUF to BUF, expanding ~.  */
    s = d = 0;
    /* If you don't want ~username ~/ to be expanded simply remove
     * This entire if statement including the else portion
     */
    if (mybuf[s] == '~')
    {
	p = d;
	while (mybuf[++s] != '/' && mybuf[s] != '\0')
	{
	    expand_string (&buf, &buf_size, p + 1);
	    buf[p++] = name[s];
	}
	expand_string (&buf, &buf_size, p + 1);
	buf[p] = '\0';

	if (p == d)
	    e = get_homedir ();
	else
	{
#ifdef GETPWNAM_MISSING
	    if (line != 0)
		error (0, 0,
		       "%s:%d:tilde expansion not supported on this system",
		       file, line);
	    else
		error (0, 0, "%s:tilde expansion not supported on this system",
		       file);
	    goto error_exit;
#else
	    struct passwd *ps;
	    ps = getpwnam (buf + d);
	    if (ps == NULL)
	    {
		if (line != 0)
		    error (0, 0, "%s:%d: no such user %s",
			   file, line, buf + d);
		else
		    error (0, 0, "%s: no such user %s", file, buf + d);
		goto error_exit;
	    }
	    e = ps->pw_dir;
#endif
	}
	if (e == NULL)
	    error (1, 0, "cannot find home directory");

	p = strlen(e);
	expand_string (&buf, &buf_size, d + p);
	memcpy(buf + d, e, p);
	d += p;
    }
    /* Kill up to here */
    p = strlen(mybuf + s) + 1;
    expand_string (&buf, &buf_size, d + p);
    memcpy(buf + d, mybuf + s, p);

    /* OK, buf contains the value we want to return.  Clean up and return
       it.  */
    free (mybuf);
    /* Save a little memory with xstrdup; buf will tend to allocate
       more than it needs to.  */
    result = xstrdup (buf);
    free (buf);
    return result;

 error_exit:
    if (mybuf != NULL)
	free (mybuf);
    if (buf != NULL)
	free (buf);
    return NULL;
}

static char *
expand_variable (name, file, line)
    const char *name;
    const char *file;
    int line;
{
    if (strcmp (name, CVSROOT_ENV) == 0)
	return current_parsed_root->directory;
    else if (strcmp (name, "RCSBIN") == 0)
    {
	error (0, 0, "RCSBIN internal variable is no longer supported");
	return NULL;
    }
    else if (strcmp (name, EDITOR1_ENV) == 0)
	return Editor;
    else if (strcmp (name, EDITOR2_ENV) == 0)
	return Editor;
    else if (strcmp (name, EDITOR3_ENV) == 0)
	return Editor;
    else if (strcmp (name, "USER") == 0)
	return getcaller ();
    else if (isalpha ((unsigned char) name[0]))
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
	    error (0, 0, "%s:%d: unrecognized variable syntax %s",
		   file, line, name);
	else
	    error (0, 0, "%s: unrecognized variable syntax %s",
		   file, name);
	return NULL;
    }
}
