/* pam_misc.c -- This is random stuff */

/* $Id: pam_misc.c,v 1.9 1997/04/05 06:56:19 morgan Exp $
 * $FreeBSD$
 *
 * $Log: pam_misc.c,v $
 * Revision 1.9  1997/04/05 06:56:19  morgan
 * enforce AUTHTOK restrictions
 *
 * Revision 1.8  1997/02/15 15:59:46  morgan
 * modified ..strCMP comment
 *
 * Revision 1.7  1996/12/01 03:14:13  morgan
 * use _pam_macros.h
 *
 * Revision 1.6  1996/11/10 20:05:52  morgan
 * name convention _pam_ enforced. Also modified _pam_strdup()
 *
 * Revision 1.5  1996/07/07 23:57:14  morgan
 * deleted debuggin function and replaced it with a static function
 * defined in pam_private.h
 *
 * Revision 1.4  1996/06/02 08:00:56  morgan
 * added StrTok function
 *
 * Revision 1.3  1996/05/21 04:36:58  morgan
 * added debugging information
 * replaced the _pam_log need for a local buffer with a call to vsyslog()
 * [Al Longyear had some segfaulting problems related to this]
 *
 * Revision 1.2  1996/03/16 21:55:13  morgan
 * changed pam_mkargv to _pam_mkargv
 *
 */

#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <ctype.h>

#include "pam_private.h"

/* caseless string comparison: POSIX does not define this.. */
int _pam_strCMP(const char *s, const char *t)
{
    int cf;

    do {
	cf = tolower(*s) - tolower(*t);
	++t;
    } while (!cf && *s++);

    return cf;
}

char *_pam_StrTok(char *from, const char *format, char **next)
/*
 * this function is a variant of the standard strtok, it differs in that
 * it takes an additional argument and doesn't nul terminate tokens until
 * they are actually reached.
 */
{
     char table[256], *end;
     int i;

     if (from == NULL && (from = *next) == NULL)
	  return from;

     /* initialize table */
     for (i=1; i<256; table[i++] = '\0');
     for (i=0; format[i] ; table[(int)format[i++]] = 'y');

     /* look for first non-blank char */
     while (*from && table[(int)*from]) {
	  ++from;
     }

     if (*from == '[') {
	 /*
	  * special case, "[...]" is considered to be a single
	  * object.  Note, however, if one of the format[] chars is
	  * '[' this single string will not be read correctly.
	  */
	 for (end=++from; *end && *end != ']'; ++end) {
	     if (*end == '\\' && end[1] == ']')
		 ++end;
	 }
	 /* note, this string is stripped of its edges: "..." is what
            remains */
     } else if (*from) {
	 /* simply look for next blank char */
	 for (end=from; *end && !table[(int)*end]; ++end);
     } else {
	 return (*next = NULL);                    /* no tokens left */
     }

     /* now terminate what we have */
     if (*end)
	 *end++ = '\0';

     /* indicate what it left */
     if (*end) {
	 *next = end;
     } else {
	 *next = NULL;                      /* have found last token */
     }

     /* return what we have */
     return from;
}

/*
 * Safe duplication of character strings. "Paranoid"; don't leave
 * evidence of old token around for later stack analysis.
 */

char *_pam_strdup(const char *x)
{
     register char *new=NULL;

     if (x != NULL) {
	  register int i;

	  for (i=0; x[i]; ++i);                       /* length of string */
	  if ((new = malloc(++i)) == NULL) {
	       i = 0;
	       pam_system_log(NULL, NULL, LOG_CRIT,
			      "_pam_strdup: failed to get memory");
	  } else {
	       while (i-- > 0) {
		    new[i] = x[i];
	       }
	  }
	  x = NULL;
     }

     return new;                 /* return the duplicate or NULL on error */
}

/* Generate argv, argc from s */
/* caller must free(argv)     */

int _pam_mkargv(char *s, char ***argv, int *argc)
{
    int l;
    int argvlen = 0;
    char *sbuf, *sbuf_start;
    char **our_argv = NULL;
    char **argvbuf;
    char *argvbufp;
#ifdef DEBUG
    int count=0;
#endif

    D(("_pam_mkargv called: %s",s));

    *argc = 0;

    l = strlen(s);
    if (l) {
	if ((sbuf = sbuf_start = _pam_strdup(s)) == NULL) {
	    pam_system_log(NULL, NULL, LOG_CRIT,
			   "pam_mkargv: null returned by _pam_strdup");
	    D(("arg NULL"));
	} else {
	    /* Overkill on the malloc, but not large */
	    argvlen = (l + 1) * ((sizeof(char)) + sizeof(char *));
	    if ((our_argv = argvbuf = malloc(argvlen)) == NULL) {
		pam_system_log(NULL, NULL, LOG_CRIT,
			       "pam_mkargv: null returned by malloc");
	    } else {
		char *tmp=NULL;

		argvbufp = (char *) argvbuf + (l * sizeof(char *));
		D(("[%s]",sbuf));
		while ((sbuf = _pam_StrTok(sbuf, " \n\t", &tmp))) {
		    D(("arg #%d",++count));
		    D(("->[%s]",sbuf));
		    strcpy(argvbufp, sbuf);
		    D(("copied token"));
		    *argvbuf = argvbufp;
		    argvbufp += strlen(argvbufp) + 1;
		    D(("stepped in argvbufp"));
		    (*argc)++;
		    argvbuf++;
		    sbuf = NULL;
		    D(("loop again?"));
		}
		_pam_drop(sbuf_start);
	    }
	}
    }
    
    *argv = our_argv;

    D(("_pam_mkargv returned"));

    return(argvlen);
}

/*
 * this function is used to protect the modules from accidental or
 * semi-mallicious harm that an application may do to confuse the API.
 */

void _pam_sanitize(pam_handle_t *pamh)
{
    /*
     * this is for security. We reset the auth-tokens here.
     */
    pam_set_item(pamh,PAM_AUTHTOK,NULL);
    pam_set_item(pamh,PAM_OLDAUTHTOK,NULL);
}

/*
 * This function scans the array and replaces the _PAM_ACTION_UNDEF
 * entries with the default action.
 */

void _pam_set_default_control(int *control_array, int default_action)
{
    int i;

    for (i=0; i<_PAM_RETURN_VALUES; ++i) {
	if (control_array[i] == _PAM_ACTION_UNDEF) {
	    control_array[i] = default_action;
	}
    }
}

/*
 * This function is used to parse a control string.  This string is a
 * series of tokens of the following form:
 *
 *               "[ ]*return_code[ ]*=[ ]*action/[ ]".
 */

#include "pam_tokens.h"

void _pam_parse_control(int *control_array, char *tok)
{
    const char *error;
    int ret;

    while (*tok) {
	int act, len;

	/* skip leading space */
	while (isspace(*tok) && *++tok);
	if (!*tok)
	    break;

	/* identify return code */
	for (ret=0; ret<=_PAM_RETURN_VALUES; ++ret) {
	    len = strlen(_pam_token_returns[ret]);
	    if (!strncmp(_pam_token_returns[ret], tok, len)) {
		break;
	    }
	}
	if (ret > _PAM_RETURN_VALUES || !*(tok += len)) {
	    error = "expecting return value";
	    goto parse_error;
	}

	/* observe '=' */
	while (isspace(*tok) && *++tok);
	if (!*tok || *tok++ != '=') {
	    error = "expecting '='";
	    goto parse_error;
	}
	
	/* skip leading space */
	while (isspace(*tok) && *++tok);
	if (!*tok) {
	    error = "expecting action";
	    goto parse_error;
	}

	/* observe action type */
	for (act=0; act < (-(_PAM_ACTION_UNDEF)); ++act) {
	    len = strlen(_pam_token_actions[act]);
	    if (!strncmp(_pam_token_actions[act], tok, len)) {
		act *= -1;
		tok += len;
		break;
	    }
	}
 	if (act > 0) {
	    /*
	     * Either we have a number or we have hit an error.  In
	     * principle, there is nothing to stop us accepting
	     * negative offsets. (Although we would have to think of
	     * another way of encoding the tokens.)  However, I really
	     * think this would be both hard to administer and easily
	     * cause looping problems.  So, for now, we will just
	     * allow forward jumps.  (AGM 1998/1/7)
	     */
	    if (!isdigit(*tok)) {
		error = "expecting jump number";
		goto parse_error;
	    }
	    /* parse a number */
	    act = 0;
	    do {
		act *= 10;
		act += *tok - '0';      /* XXX - this assumes ascii behavior */
	    } while (*++tok && isdigit(*tok));
	    if (! act) {
		/* we do not allow 0 jumps.  There is a token ('ignore')
                   for that */
		error = "expecting non-zero";
		goto parse_error;
	    }
	}

	/* set control_array element */
	if (ret != _PAM_RETURN_VALUES) {
	    control_array[ret] = act;
	} else {
	    /* set the default to 'act' */
	    _pam_set_default_control(control_array, act);
	}
    }

    /* that was a success */
    return;

parse_error:
    /* treat everything as bad */
    pam_system_log(NULL, NULL, LOG_ERR, "pam_parse: %s; [...%s]", error, tok);
    for (ret=0; ret<_PAM_RETURN_VALUES; control_array[ret++]=_PAM_ACTION_BAD);

}
