/* Set.c */

/*  $RCSfile: set.c,v $
 *  $Revision: 14020.12 $
 *  $Date: 93/07/09 11:45:48 $
 */

#include "sys.h"

#include <ctype.h>

#include "util.h"
#include "cmds.h"
#include "main.h"
#include "set.h"
#include "defaults.h"
#include "copyright.h"

#ifdef TERM_FTP
extern int compress_toggle;
#endif

/* Set.c globals: */
char *verbose_msgs[4] = {
	"Not printing anything.\n",
	"Only printing necessary error messages.\n",
	"Printing error messages and announcements from the remote host.\n",
	"Printing all messages, errors, acknowledgments, and announcements.\n"
};

char *short_verbose_msgs[4] = {
	"Quiet (-1)",
	"Errors Only (0)",
	"Terse (1)",
	"Verbose (2)"
};

string						vstr;

/* Set.c externs: */
extern int					progress_meter, connected;
extern int					parsing_rc, keep_recent;
extern string				pager, anon_password, prompt;
extern str32				curtypename;
extern long					logsize;
extern FILE					*logf;
extern longstring			rcname, logfname, lcwd;
extern int					auto_binary, ansi_escapes, debug;
extern int					mprompt, remote_is_unix, verbose;
extern int					startup_msg, anon_open, passivemode;
extern int					restricted_data_ports;
#ifndef NO_TIPS
extern int					tips;
#endif
#ifdef GATEWAY
extern string				gateway, gate_login;
#endif

/* The variables must be sorted in alphabetical order, or else
 * match_var() will choke.
 */
struct var vars[] = {
	VARENTRY("anon-open",		BOOL, 0, &anon_open,	NULL),
	VARENTRY("anon-password",	STR,  0, anon_password,	NULL),
	VARENTRY("ansi-escapes",	BOOL, 0, &ansi_escapes,	NULL),
	VARENTRY("auto-binary",		BOOL, 0, &auto_binary,	NULL),
#ifdef TERM_FTP
	VARENTRY("compress",		INT,  0,
		 &compress_toggle,NULL),
#endif
	VARENTRY("debug",			INT,  0, &debug,		NULL),
#ifdef GATEWAY
	VARENTRY("gateway-login",	STR,  0, gate_login,	set_gatelogin),
	VARENTRY("gateway-host",	STR,  0, gateway,		NULL),
#endif
	VARENTRY("local-dir",		STR,  0, lcwd,			set_ldir),
	VARENTRY("logfile",			STR,  0, logfname,		set_log),
	VARENTRY("logsize",			LONG, 0, &logsize,		NULL),
	VARENTRY("mprompt",			BOOL, 0, &mprompt,		NULL),
	VARENTRY("netrc",			-STR, 0, rcname,		NULL),
	VARENTRY("passive",			BOOL, 0, &passivemode,	NULL),
	VARENTRY("pager",			STR,  0, pager + 1,		set_pager),
	VARENTRY("prompt",			STR,  0, prompt,		set_prompt),
	VARENTRY("progress-reports",INT,  0, &progress_meter,NULL),
	VARENTRY("recent-list",		BOOL, 0, &keep_recent,	NULL),
	VARENTRY("remote-is-unix",	BOOL, 1, &remote_is_unix,NULL),
	VARENTRY("restricted-data-ports",BOOL, 0, &restricted_data_ports, NULL),
	VARENTRY("startup-msg",		BOOL, 0, &startup_msg,	NULL),  /* TAR */
#ifndef NO_TIPS
	VARENTRY("tips",			BOOL, 0, &tips,			NULL),
#endif
	VARENTRY("type",			STR,  1, curtypename,	set_type),
	VARENTRY("verbose",			STR,  0, vstr,			set_verbose),
};


void set_verbose(char *new, int unset)
{
	int i, c;

	if (unset == -1) verbose = !verbose;
	else if (unset || !new) verbose = V_ERRS;
	else {
		if (isalpha(*new)) {
			c = islower(*new) ? toupper(*new) : *new;	
			for (i=0; i<(int)(sizeof(short_verbose_msgs)/sizeof(char *)); i++) {
				if (short_verbose_msgs[i][0] == c)
					verbose = i - 1;
			}
		} else {
			i = atoi(new);
			if (i < V_QUIET) i = V_QUIET;
			else if (i > V_VERBOSE) i = V_VERBOSE;
			verbose = i;
		}
	}
	(void) Strncpy(vstr, short_verbose_msgs[verbose+1]);
	if (!parsing_rc && NOT_VQUIET) 
		(void) fputs(verbose_msgs[verbose+1], stdout);
}	/* set_verbose */




void set_prompt(char *new, int unset)
{
	(void) Strncpy(prompt, (unset || !new) ? dPROMPT : new);
	init_prompt();
}	/* set_prompt */




void set_log(char *fname, int unset)
{
	if (logf) {
		(void) fclose(logf);
		logf = NULL;
	}
	if (!unset && fname) {
		(void) Strncpy(logfname, fname);
		logf = fopen (LocalDotPath(logfname), "a");
	}
}	/* set_log */




void set_pager(char *new, int unset)
{
	if (unset)
		(void) strcpy(pager, "-");
	else {
		if (!new)
			new = dPAGER;
		if (!new[0])
			(void) Strncpy(pager, "-");
		else {
			(void) sprintf(pager, "|%s", (*new == '|' ? new + 1 : new));
			(void) LocalPath(pager + 1);
		}
	}
}	/* set_pager */




void set_type(char *newtype, int unset)
{
	int t = verbose;
	verbose = V_QUIET;
	if (!connected && t > V_QUIET)
		(void) printf("Not connected.\n");
	else if (newtype != NULL && !unset)
		(void) _settype(newtype);
	verbose = t;
}	/* set_type */




void set_ldir(char *ldir, int unset)
{
	int t = verbose;
	char *argv[2];

	if (ldir && !unset) {
		verbose = V_QUIET;
		argv[1] = ldir;
		(void) lcd(2, argv);
		verbose = t;
	}
}	/* set_ldir */




#ifdef GATEWAY
void set_gatelogin(char *glogin, int unset)
{
	if (unset || !glogin) {
		gate_login[0] = gateway[0] = 0;
	} else
		(void) strcpy(gate_login, glogin);
}	/* set_gatelogin */
#endif




struct var *match_var(char *varname)
{
	int i, ambig;
	struct var *v;
	short c;

	c = (short) strlen(varname);
	for (i=0, v=vars; i<NVARS; i++, v++) {
		if (strcmp(v->name, varname) == 0)
			return v;	/* exact match. */
		if (c < v->nmlen) {
			if (strncmp(v->name, varname, (size_t) c) == 0) {
				/* Now make sure that it only matches one var name. */
				if (c >= v[1].nmlen || (i == (NVARS - 1)))
					ambig = 0;
				else
					ambig = !strncmp(v[1].name, varname, (size_t) c);
				if (!ambig)
					return v;
				(void) fprintf(stderr, "%s: ambiguous variable name.\n", varname);
				goto xx;
			}
		}
	}
	(void) fprintf(stderr, "%s: unknown variable.\n", varname);
xx:
	return ((struct var *)0);
}	/* match_var */




void show_var(struct var *v)
{
	int c;

	if (v != (struct var *)0) {
		(void) printf("%-20s= ", v->name);
		c = v->type;
		if (c < 0) c = -c;
		if (v->conn_required && !connected)
			(void) printf("(not connected)\n");
		else switch (c) {
			case INT:
				(void) printf("%d\n", *(int *)v->var); break;
			case LONG:
				(void) printf("%ld\n", *(long *)v->var); break;
			case STR:
				(void) printf("\"%s\"\n", (char *)v->var); break;
			case BOOL:
				(void) printf("%s\n", *(int *)v->var == 0 ? "no" : "yes");
		}
	}
}	/* show_var */




void show(char *varname)
{
	int i;
	struct var *v;

	if ((varname == NULL)	/* (Denotes show all vars) */
		|| (strcmp("all", varname) == 0))
	{
		for (i=0; i<NVARS; i++)
		    show_var(&vars[i]);
	} else {
		if ((v = match_var(varname)) != (struct var *)0)
			show_var(v);
	}
}	/* show */




int do_show(int argc, char **argv)
{
	int i;

	if (argc < 2)
		show(NULL);
	else
		for (i=1; i<argc; i++)
			show(argv[i]);
	return NOERR;
}	/* do_show */




int set(int argc, char **argv)
{
	int unset;
	struct var *v;
	char *var, *val = NULL;

	if (argc < 2 || strncmp(argv[1], "all", (size_t)3) == 0) {
		show(NULL);		/* show all variables. */
	} else {
		unset = argv[0][0] == 'u';
		var = argv[1];
		if (argc > 2) {
			/* could be '= value' or just 'value.' */
			if (*argv[2] == '=') {
				if (argc > 3)
					val = argv[3];
				else return USAGE;	/* can't do 'set var =' */
			} else
				val = argv[2];
			if (val[0] == 0)
				val = NULL;
		}
		v = match_var(var);
		if (v != NULL) {
			if (v->conn_required && !connected)
				(void) fprintf(stderr, "%s: must be connected.\n", var);
			else if (v->type < 0)	
				(void) fprintf(stderr, "%s: read-only variable.\n", var);
			else if (v->proc != (setvarproc) 0) {
				(*v->proc)(val, unset);		/* a custom set proc. */
			} else if (unset) switch(v->type) {
				case BOOL:
				case INT:
					*(int *) v->var = 0; break;
				case LONG:
					*(long *) v->var = 0; break;
				case STR:
					*(char *) v->var = 0; break;
			} else {
				if (val == NULL) switch(v->type) {
					/* User just said "set varname" */
					case BOOL:
					case INT:
						*(int *) v->var = 1; break;
					case LONG:
						*(long *) v->var = 1; break;
					case STR:
						*(char *) v->var = 0; break;
				} else {
					/* User said "set varname = value" */
					switch (v->type) {
						case BOOL:
							*(int *)v->var = StrToBool(val); break;
						case INT:
							(void) sscanf(val, "%d", (int *) v->var); break;
						case LONG:
							(void) sscanf(val, "%ld", (long *) v->var); break;
						case STR:
							(void) strcpy(v->var, val); break;
					}
				}
			}
		}
	}
	return NOERR;
}	/* set */

/* eof Set.c */
