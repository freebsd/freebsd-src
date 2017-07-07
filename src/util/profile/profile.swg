%{
/*
 * Copyright 2004, 2008 by the Massachusetts Institute of Technology.
 * All Rights Reserved.
 *
 * Export of this software from the United States of America may
 *   require a specific license from the United States Government.
 *   It is the responsibility of any person or organization contemplating
 *   export to obtain such a license before exporting.
 * 
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of M.I.T. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  Furthermore if you modify this software you must label
 * your software as modified software and not distribute it in such a
 * fashion that it might be confused with the original M.I.T. software.
 * M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 *
 *
 * Input for wrapper generator program SWIG for profile routines.
 */
#include <errno.h>
#include "com_err.h"
#include "profile.h"

#ifdef SWIGTCL
/* Reduce warnings about cast discarding const to just this one, from
   every SWIG-generated call to Tcl_SetResult.  */
static void my_tcl_setresult(Tcl_Interp *i, const char *str, Tcl_FreeProc *f)
{
    Tcl_SetResult(i, (char *) str, f);
}
#undef Tcl_SetResult
#define Tcl_SetResult my_tcl_setresult
#endif
%}

%include "typemaps.i"

/* These should perhaps be part of the general SWIG package, maybe?  */
%typemap(in,numinputs=0) SWIGTYPE *OUTPUT ($1_basetype tmp) {
    /*generic swigtype hack*/ $1 = &tmp;
}
%typemap(tcl8,argout) SWIGTYPE *OUTPUT
  "/*generic swigtype hack*/ Tcl_SetObjResult(interp,SWIG_NewInstanceObj((void *) *$1, $*1_descriptor,0));";
%typemap(python,argout) SWIGTYPE *OUTPUT
  "/*generic swigtype hack*/ resultobj = SWIG_NewPointerObj((void *) *$1, $*1_descriptor,0);";

%module profile

typedef long errcode_t;
%inline %{
typedef void **iter_t; /* ick */
%}

/* As a hack, if we have too much trouble trying to manage output
   arguments for functions returning error codes, this output argument
   type will let us twist it around into a function returning the
   interesting type, and incidentally possibly raising an error.  */
%typemap(in,numinputs=0) errcode_t * (errcode_t tmp) {
    /* in errcode_t * */
    tmp = 0;
    $1 = &tmp;
}
%typemap(tcl8,argout) errcode_t* {
    /* argout errcode_t * */
    if (*$1) {
	/* There could be a memory leak here in the SWIG-Tcl layer,
	   I'm not sure.  Not going to worry about it though.  */
	Tcl_SetResult(interp, (char *) error_message(*$1), TCL_STATIC);
	SWIG_fail;
    }
}
/* returning errcode_t */
%typemap(tcl8,out) errcode_t {
    /* out errcode_t $1 */
    if ($1) {
	/* There could be a memory leak here in the SWIG-Tcl layer,
	   I'm not sure.  Not going to worry about it though.  */
	Tcl_SetResult(interp, (char *) error_message($1), TCL_STATIC);
	SWIG_fail;
    }
}
%typemap(python,argout) errcode_t* {
    /* do something with *($1) */ abort();
}
%typemap(python,out) errcode_t {
    /* do something with $1 */ abort();
}

/* "char **OUTPUT" : Supply a place for the function to stuff one
   string pointer.  */
%typemap(in,numinputs=0) char **OUTPUT (char * tmp) {
    /* in char **OUTPUT */
    tmp = NULL;
    $1 = &tmp;
}
%typemap(tcl8,argout) char **OUTPUT {
    /* argout char **OUTPUT */
/*    Tcl_SetResult(interp, *$1, TCL_DYNAMIC); */
    char *s = ($1 && *$1) ? *$1 : "";
    Tcl_ListObjAppendElement(interp, Tcl_GetObjResult(interp),
			     Tcl_NewStringObj(s, strlen(s)));
}
%typemap(freearg) char **OUTPUT {
    /* There may be a memory leak here.  Investigate later, if anyone
       cares.  */
/*    profile_release_string(*$1); */
}

/* "char **nullterm" : Null-terminated list of strings, from a single
   input value which is a list.  */
%typemap(tcl8,in) char **nullterm {
    /* in char **nullterm */
    int n;
    if (Tcl_SplitList(interp, Tcl_GetStringFromObj($input,NULL), &n, (const char ***) &$1) == TCL_ERROR) SWIG_fail;
}
%typemap(tcl8,freearg) char **nullterm {
    /* freearg char **nullterm */
    if ($1) { Tcl_Free((char *)$1); $1 = (char **) NULL; }
}

/* "char ***OUTPUT" : Supply a place for the function to stuff a
   pointer to a list of strings, which will be combined into a list to
   return, and the data from the function itself freed before
   returning.  */
%typemap(in,numinputs=0) char ***OUTPUT (char ** tmp) {
    /* in char ***OUTPUT */
    tmp = NULL;
    $1 = &tmp;
}
%typemap(tcl8,argout) char ***OUTPUT {
    /* argout char ***OUTPUT */
    int i;
    for (i = 0; (*$1)[i]; i++)
	Tcl_AppendElement(interp, (*$1)[i]);
}
%typemap(tcl8,freearg) char ***OUTPUT {
    /* freearg char ***OUTPUT */
    profile_free_list(*$1);
}

typedef struct _profile_t *profile_t;

errcode_t profile_init_path(const char *path = NULL, profile_t *OUTPUT);
errcode_t profile_init(const char **nullterm = NULL, profile_t *OUTPUT);
errcode_t profile_flush(profile_t);
errcode_t profile_flush_to_file(profile_t, const char *path);
/* Nota bene: There is nothing at all in this code to prevent a script
   from accessing a profile object after calling one of these routines
   to destroy it!  */
void profile_abandon(profile_t);
void profile_release(profile_t);

errcode_t profile_get_values(profile_t p, const char **nullterm,
			     char ***OUTPUT);

/* XXX Because of the way this is specified, the default can only be
   given if you're actually using all three names (e.g., for realm
   data).  SWIG currently doesn't support a non-optional argument (at
   the scripting-language level -- the output-only argument doesn't
   count) after an optional one.  */
extern errcode_t profile_get_string(profile_t p,
				    const char *name,
				    const char *subname,
				    const char *subsubname = NULL,
				    const char *defval = NULL,
				    char **OUTPUT);

errcode_t profile_get_integer(profile_t p,
			      const char *name,
			      const char *subname,
			      const char *subsubname = NULL,
			      int defval = 0,
			      int *OUTPUT);
errcode_t profile_get_boolean(profile_t p,
			      const char *name,
			      const char *subname,
			      const char *subsubname = NULL,
			      int defval = 0,
			      int *OUTPUT);
errcode_t profile_get_relation_names(profile_t p,
				     const char **nullterm,
				     char ***OUTPUT);
errcode_t profile_get_subsection_names(profile_t p,
				       const char **nullterm,
				       char ***OUTPUT);

%rename("profile_iterator_create") iter_create;
%rename("profile_iterator_free") iter_free;
%inline %{
static errcode_t iter_create(profile_t p, const char **nullterm,
			     int flags, iter_t *OUTPUT)
{
    iter_t it;
    errcode_t err;
    const char **args;

    it = malloc(sizeof(*it));
    if (it == NULL)
	return ENOMEM;
    {
	/* Memory leak!

	   The profile code seems to assume that I'll keep the string
	   array around for as long as the iterator is valid; I can't
	   create the iterator and then throw them away.

	   But right now, I can't be bothered to track the necessary
	   information to do the cleanup later.  */
	int count, j;
	for (count = 0; nullterm[count]; count++) ;
	args = calloc(count+1, sizeof(char *));
	if (args == NULL)
	    return ENOMEM;
	for (j = 0; j < count; j++) {
	    args[j] = strdup(nullterm[j]);
	    if (args[j] == NULL)
		return ENOMEM;
	}
	args[j] = NULL;
    }
    err = profile_iterator_create(p, args, flags, it);
    if (err)
	free(it);
    else
	*OUTPUT = it;
    return err;
}
static void iter_free(iter_t i)
{
    profile_iterator_free(i);
    free(i);
}
%}
errcode_t profile_iterator(iter_t, char **OUTPUT, char **OUTPUT);


errcode_t profile_update_relation(profile_t p, const char **nullterm,
				  const char *oldval,
				  const char *newval = NULL);
errcode_t profile_clear_relation(profile_t p, const char **nullterm);
errcode_t profile_rename_section(profile_t p, const char **nullterm,
				 const char *new_name = NULL);
errcode_t profile_add_relation(profile_t p, const char **nullterm,
			       const char *new_val = NULL);
/* XXX Should be using profile_free_buffer blah.  */
errcode_t profile_flush_to_buffer(profile_t p, char **OUTPUT);

#ifdef SWIGTCL
%include "tclsh.i"
#endif
