/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1992, 1993, 1994, 1995, 1996
 *	Keith Bostic.  All rights reserved.
 * Copyright (c) 1995
 *	George V. Neville-Neil. All rights reserved.
 * Copyright (c) 1996
 *	Sven Verdoolaege. All rights reserved.
 *
 * See the LICENSE file for redistribution information.
 */

#include "config.h"

#ifndef lint
static const char sccsid[] = "@(#)perl.xs	8.27 (Berkeley) 10/16/96";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <ctype.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "../common/common.h"

#include <EXTERN.h>
#include <perl.h>
#include <XSUB.h>

#include "perl_extern.h"

static void msghandler __P((SCR *, mtype_t, char *, size_t));

extern GS *__global_list;			/* XXX */

static char *errmsg = 0;

/*
 * INITMESSAGE --
 *	Macros to point messages at the Perl message handler.
 */
#define	INITMESSAGE							\
	scr_msg = __global_list->scr_msg;				\
	__global_list->scr_msg = msghandler;
#define	ENDMESSAGE							\
	__global_list->scr_msg = scr_msg;				\
	if (rval) croak(errmsg);

static void xs_init __P((void));

/*
 * perl_end --
 *	Clean up perl interpreter
 *
 * PUBLIC: int perl_end __P((GS *));
 */
int
perl_end(gp)
	GS *gp;
{
	/*
	 * Call perl_run and perl_destuct to call END blocks and DESTROY
	 * methods.
	 */
	if (gp->perl_interp) {
		/*Irestartop = 0;            			/ * XXX */
		perl_run(gp->perl_interp);
		perl_destruct(gp->perl_interp);
#if defined(DEBUG) || defined(PURIFY) || defined(LIBRARY)
		perl_free(gp->perl_interp);
#endif
	}
}

/*
 * perl_eval
 *	Evaluate a string
 * 	We don't use mortal SVs because no one will clean up after us
 */
static void 
perl_eval(string)
	char *string;
{
#ifdef HAVE_PERL_5_003_01
	SV* sv = newSVpv(string, 0);

	perl_eval_sv(sv, G_DISCARD | G_NOARGS);
	SvREFCNT_dec(sv);
#else
	char *argv[2];

	argv[0] = string;
	argv[1] = NULL;
	perl_call_argv("_eval_", G_EVAL | G_DISCARD | G_KEEPERR, argv);
#endif
}

/*
 * perl_init --
 *	Create the perl commands used by nvi.
 *
 * PUBLIC: int perl_init __P((SCR *));
 */
int
perl_init(scrp)
	SCR *scrp;
{
	AV * av;
	GS *gp;
	char *bootargs[] = { "VI", NULL };
#ifndef USE_SFIO
	SV *svcurscr;
#endif

#ifndef HAVE_PERL_5_003_01
	static char *args[] = { "", "-e", "sub _eval_ { eval $_[0] }" };
#else
	static char *args[] = { "", "-e", "" };
#endif
	STRLEN length;
	char *file = __FILE__;

	gp = scrp->gp;
	gp->perl_interp = perl_alloc();
  	perl_construct(gp->perl_interp);
	if (perl_parse(gp->perl_interp, xs_init, 3, args, 0)) {
		perl_destruct(gp->perl_interp);
		perl_free(gp->perl_interp);
		gp->perl_interp = NULL;
		return 1;
	}
        perl_call_argv("VI::bootstrap", G_DISCARD, bootargs);
	perl_eval("$SIG{__WARN__}='VI::Warn'");

	av_unshift(av = GvAVn(incgv), 1);
	av_store(av, 0, newSVpv(_PATH_PERLSCRIPTS,
				sizeof(_PATH_PERLSCRIPTS)-1));

#ifdef USE_SFIO
	sfdisc(PerlIO_stdout(), sfdcnewnvi(scrp));
	sfdisc(PerlIO_stderr(), sfdcnewnvi(scrp));
#else
	svcurscr = perl_get_sv("curscr", TRUE);
	sv_magic((SV *)gv_fetchpv("STDOUT",TRUE, SVt_PVIO), svcurscr,
		 	'q', Nullch, 0);
	sv_magic((SV *)gv_fetchpv("STDERR",TRUE, SVt_PVIO), svcurscr,
		 	'q', Nullch, 0);
#endif /* USE_SFIO */
	return (0);
}

/*
 * perl_screen_end
 *	Remove all refences to the screen to be destroyed
 *
 * PUBLIC: int perl_screen_end __P((SCR*));
 */
int
perl_screen_end(scrp)
	SCR *scrp;
{
	if (scrp->perl_private) {
		sv_setiv((SV*) scrp->perl_private, 0);
	}
	return 0;
}

static void
my_sighandler(i)
	int i;
{
	croak("Perl command interrupted by SIGINT");
}

/* Create a new reference to an SV pointing to the SCR structure
 * The perl_private part of the SCR structure points to the SV,
 * so there can only be one such SV for a particular SCR structure.
 * When the last reference has gone (DESTROY is called),
 * perl_private is reset; When the screen goes away before
 * all references are gone, the value of the SV is reset;
 * any subsequent use of any of those reference will produce
 * a warning. (see typemap)
 */
static SV *
newVIrv(rv, screen)
	SV *rv;
	SCR *screen;
{
	sv_upgrade(rv, SVt_RV);
	if (!screen->perl_private) {
		screen->perl_private = newSV(0);
		sv_setiv(screen->perl_private, (IV) screen);
	} 
	else SvREFCNT_inc(screen->perl_private);
	SvRV(rv) = screen->perl_private;
	SvROK_on(rv);
	return sv_bless(rv, gv_stashpv("VI", TRUE));
}


/* 
 * perl_ex_perl -- :[line [,line]] perl [command]
 *	Run a command through the perl interpreter.
 *
 * PUBLIC: int perl_ex_perl __P((SCR*, CHAR_T *, size_t, recno_t, recno_t));
 */
int 
perl_ex_perl(scrp, cmdp, cmdlen, f_lno, t_lno)
	SCR *scrp;
	CHAR_T *cmdp;
	size_t cmdlen;
	recno_t f_lno, t_lno;
{
	static SV *svcurscr = 0, *svstart, *svstop, *svid;
	GS *gp;
	STRLEN length;
	size_t len;
	char *err;
	Signal_t (*istat)();

	/* Initialize the interpreter. */
	gp = scrp->gp;
	if (!svcurscr) {
		if (gp->perl_interp == NULL && perl_init(scrp))
			return (1);
		SvREADONLY_on(svcurscr = perl_get_sv("curscr", TRUE));
		SvREADONLY_on(svstart = perl_get_sv("VI::StartLine", TRUE));
		SvREADONLY_on(svstop = perl_get_sv("VI::StopLine", TRUE));
		SvREADONLY_on(svid = perl_get_sv("VI::ScreenId", TRUE));
	}

	sv_setiv(svstart, f_lno);
	sv_setiv(svstop, t_lno);
	newVIrv(svcurscr, scrp);
	/* Backwards compatibility. */
	newVIrv(svid, scrp);

	istat = signal(SIGINT, my_sighandler);
	perl_eval(cmdp);
	signal(SIGINT, istat);

	SvREFCNT_dec(SvRV(svcurscr));
	SvROK_off(svcurscr);
	SvREFCNT_dec(SvRV(svid));
	SvROK_off(svid);

	err = SvPV(GvSV(errgv), length);
	if (!length)
		return (0);

	err[length - 1] = '\0';
	msgq(scrp, M_ERR, "perl: %s", err);
	return (1);
}

/*
 * replace_line
 *	replace a line with the contents of the perl variable $_
 *	lines are split at '\n's
 *	if $_ is undef, the line is deleted
 *	returns possibly adjusted linenumber
 */
static int 
replace_line(scrp, line, t_lno)
	SCR *scrp;
	recno_t line, *t_lno;
{
	char *str, *next;
	size_t len;

	if (SvOK(GvSV(defgv))) {
		str = SvPV(GvSV(defgv),len);
		next = memchr(str, '\n', len);
		api_sline(scrp, line, str, next ? (next - str) : len);
		while (next++) {
			len -= next - str;
			next = memchr(str = next, '\n', len);
			api_iline(scrp, ++line, str, next ? (next - str) : len);
			(*t_lno)++;
		}
	} else {
		api_dline(scrp, line--);
		(*t_lno)--;
	}
	return line;
}

/* 
 * perl_ex_perldo -- :[line [,line]] perl [command]
 *	Run a set of lines through the perl interpreter.
 *
 * PUBLIC: int perl_ex_perldo __P((SCR*, CHAR_T *, size_t, recno_t, recno_t));
 */
int 
perl_ex_perldo(scrp, cmdp, cmdlen, f_lno, t_lno)
	SCR *scrp;
	CHAR_T *cmdp;
	size_t cmdlen;
	recno_t f_lno, t_lno;
{
	static SV *svcurscr = 0, *svstart, *svstop, *svid;
	CHAR_T *p;
	GS *gp;
	STRLEN length;
	size_t len;
	recno_t i;
	char *str;
#ifndef HAVE_PERL_5_003_01
	char *argv[2];
#else
	SV* sv;
#endif
	dSP;

	/* Initialize the interpreter. */
	gp = scrp->gp;
	if (!svcurscr) {
		if (gp->perl_interp == NULL && perl_init(scrp))
			return (1);
		SPAGAIN;
		SvREADONLY_on(svcurscr = perl_get_sv("curscr", TRUE));
		SvREADONLY_on(svstart = perl_get_sv("VI::StartLine", TRUE));
		SvREADONLY_on(svstop = perl_get_sv("VI::StopLine", TRUE));
		SvREADONLY_on(svid = perl_get_sv("VI::ScreenId", TRUE));
	}

#ifndef HAVE_PERL_5_003_01
	argv[0] = cmdp;
	argv[1] = NULL;
#else
	length = strlen(cmdp);
	sv = newSV(length + sizeof("sub VI::perldo {")-1 + 1 /* } */);
	sv_setpvn(sv, "sub VI::perldo {", sizeof("sub VI::perldo {")-1); 
	sv_catpvn(sv, cmdp, length);
	sv_catpvn(sv, "}", 1);
	perl_eval_sv(sv, G_DISCARD | G_NOARGS);
	SvREFCNT_dec(sv);
	str = SvPV(GvSV(errgv),length);
	if (length)
		goto err;
#endif

	newVIrv(svcurscr, scrp);
	/* Backwards compatibility. */
	newVIrv(svid, scrp);

	ENTER;
	SAVETMPS;
	for (i = f_lno; i <= t_lno && !api_gline(scrp, i, &str, &len); i++) {
		sv_setpvn(GvSV(defgv),str,len);
		sv_setiv(svstart, i);
		sv_setiv(svstop, i);
#ifndef HAVE_PERL_5_003_01
		perl_call_argv("_eval_", G_SCALAR | G_EVAL | G_KEEPERR, argv);
#else
		PUSHMARK(sp);
                perl_call_pv("VI::perldo", G_SCALAR | G_EVAL);
#endif
		str = SvPV(GvSV(errgv), length);
		if (length) break;
		SPAGAIN;
		if(SvTRUEx(POPs)) 
			i = replace_line(scrp, i, &t_lno);
		PUTBACK;
	}
	FREETMPS;
	LEAVE;

	SvREFCNT_dec(SvRV(svcurscr));
	SvROK_off(svcurscr);
	SvREFCNT_dec(SvRV(svid));
	SvROK_off(svid);

	if (!length)
		return (0);

err:	str[length - 1] = '\0';
	msgq(scrp, M_ERR, "perl: %s", str);
	return (1);
}

/*
 * msghandler --
 *	Perl message routine so that error messages are processed in
 *	Perl, not in nvi.
 */
static void
msghandler(sp, mtype, msg, len)
	SCR *sp;
	mtype_t mtype;
	char *msg;
	size_t len;
{
	/* Replace the trailing <newline> with an EOS. */
	/* Let's do that later instead */
	if (errmsg) free (errmsg);
	errmsg = malloc(len + 1);
	memcpy(errmsg, msg, len);
	errmsg[len] = '\0';
}

/* Register any extra external extensions */

extern void boot_DynaLoader _((CV* cv));
extern void boot_VI _((CV* cv));

static void
xs_init()
{
#ifdef HAVE_PERL_5_003_01
	dXSUB_SYS;
#endif
	char *file = __FILE__;

	newXS("DynaLoader::boot_DynaLoader", boot_DynaLoader, file);
	newXS("VI::bootstrap", boot_VI, file);
}

typedef SCR *	VI;
typedef SCR *	VI__OPT;
typedef SCR *	VI__MAP;
typedef SCR * 	VI__MARK;
typedef AV *	AVREF;

MODULE = VI	PACKAGE = VI

# msg --
#	Set the message line to text.
#
# Perl Command: VI::Msg
# Usage: VI::Msg screenId text

void
Msg(screen, text)
	VI          screen
	char *      text
 
	ALIAS:
	PRINT = 1

	CODE:
	api_imessage(screen, text);

# XS_VI_escreen --
#	End a screen.
#
# Perl Command: VI::EndScreen
# Usage: VI::EndScreen screenId

void
EndScreen(screen)
	VI	screen

	PREINIT:
	void (*scr_msg) __P((SCR *, mtype_t, char *, size_t));
	int rval;

	CODE:
	INITMESSAGE;
	rval = api_escreen(screen);
	ENDMESSAGE;

# XS_VI_iscreen --
#	Create a new screen.  If a filename is specified then the screen
#	is opened with that file.
#
# Perl Command: VI::NewScreen
# Usage: VI::NewScreen screenId [file]

VI
Edit(screen, ...)
	VI screen

	ALIAS:
	NewScreen = 1

	PROTOTYPE: $;$
	PREINIT:
	void (*scr_msg) __P((SCR *, mtype_t, char *, size_t));
	int rval;
	char *file;
	SCR *nsp;

	CODE:
	file = (items == 1) ? NULL : (char *)SvPV(ST(1),na);
	INITMESSAGE;
	rval = api_edit(screen, file, &nsp, ix);
	ENDMESSAGE;
	
	RETVAL = ix ? nsp : screen;

	OUTPUT:
	RETVAL

# XS_VI_fscreen --
#	Return the screen id associated with file name.
#
# Perl Command: VI::FindScreen
# Usage: VI::FindScreen file

VI
FindScreen(file)
	char *file

	PREINIT:
	SCR *fsp;
	CODE:
	RETVAL = api_fscreen(0, file);

# XS_VI_aline --
#	-- Append the string text after the line in lineNumber.
#
# Perl Command: VI::AppendLine
# Usage: VI::AppendLine screenId lineNumber text

void
AppendLine(screen, linenumber, text)
	VI screen
	int linenumber
	char *text

	PREINIT:
	void (*scr_msg) __P((SCR *, mtype_t, char *, size_t));
	int rval;
	STRLEN length;

	CODE:
	SvPV(ST(2), length);
	INITMESSAGE;
	rval = api_aline(screen, linenumber, text, length);
	ENDMESSAGE;

# XS_VI_dline --
#	Delete lineNum.
#
# Perl Command: VI::DelLine
# Usage: VI::DelLine screenId lineNum

void 
DelLine(screen, linenumber)
	VI screen
	int linenumber

	PREINIT:
	void (*scr_msg) __P((SCR *, mtype_t, char *, size_t));
	int rval;

	CODE:
	INITMESSAGE;
	rval = api_dline(screen, (recno_t)linenumber);
	ENDMESSAGE;

# XS_VI_gline --
#	Return lineNumber.
#
# Perl Command: VI::GetLine
# Usage: VI::GetLine screenId lineNumber

char *
GetLine(screen, linenumber)
	VI screen
	int linenumber

	PREINIT:
	size_t len;
	void (*scr_msg) __P((SCR *, mtype_t, char *, size_t));
	int rval;
	char *line, *p;

	PPCODE:
	INITMESSAGE;
	rval = api_gline(screen, (recno_t)linenumber, &p, &len);
	ENDMESSAGE;

	EXTEND(sp,1);
        PUSHs(sv_2mortal(newSVpv(p, len)));

# XS_VI_sline --
#	Set lineNumber to the text supplied.
#
# Perl Command: VI::SetLine
# Usage: VI::SetLine screenId lineNumber text

void
SetLine(screen, linenumber, text)
	VI screen
	int linenumber
	char *text

	PREINIT:
	void (*scr_msg) __P((SCR *, mtype_t, char *, size_t));
	int rval;
	STRLEN length;

	CODE:
	SvPV(ST(2), length);
	INITMESSAGE;
	rval = api_sline(screen, linenumber, text, length);
	ENDMESSAGE;

# XS_VI_iline --
#	Insert the string text before the line in lineNumber.
#
# Perl Command: VI::InsertLine
# Usage: VI::InsertLine screenId lineNumber text

void
InsertLine(screen, linenumber, text)
	VI screen
	int linenumber
	char *text

	PREINIT:
	void (*scr_msg) __P((SCR *, mtype_t, char *, size_t));
	int rval;
	STRLEN length;

	CODE:
	SvPV(ST(2), length);
	INITMESSAGE;
	rval = api_iline(screen, linenumber, text, length);
	ENDMESSAGE;

# XS_VI_lline --
#	Return the last line in the screen.
#
# Perl Command: VI::LastLine
# Usage: VI::LastLine screenId

int 
LastLine(screen)
	VI screen

	PREINIT:
	recno_t last;
	void (*scr_msg) __P((SCR *, mtype_t, char *, size_t));
	int rval;

	CODE:
	INITMESSAGE;
	rval = api_lline(screen, &last);
	ENDMESSAGE;
	RETVAL=last;

	OUTPUT:
	RETVAL

# XS_VI_getmark --
#	Return the mark's cursor position as a list with two elements.
#	{line, column}.
#
# Perl Command: VI::GetMark
# Usage: VI::GetMark screenId mark

void
GetMark(screen, mark)
	VI screen
	char mark

	PREINIT:
	struct _mark cursor;
	void (*scr_msg) __P((SCR *, mtype_t, char *, size_t));
	int rval;

	PPCODE:
	INITMESSAGE;
	rval = api_getmark(screen, (int)mark, &cursor);
	ENDMESSAGE;

	EXTEND(sp,2);
        PUSHs(sv_2mortal(newSViv(cursor.lno)));
        PUSHs(sv_2mortal(newSViv(cursor.cno)));

# XS_VI_setmark --
#	Set the mark to the line and column numbers supplied.
#
# Perl Command: VI::SetMark
# Usage: VI::SetMark screenId mark line column

void
SetMark(screen, mark, line, column)
	VI screen
	char mark
	int line
	int column

	PREINIT:
	struct _mark cursor;
	void (*scr_msg) __P((SCR *, mtype_t, char *, size_t));
	int rval;

	CODE:
	INITMESSAGE;
	cursor.lno = line;
	cursor.cno = column;
	rval = api_setmark(screen, (int)mark, &cursor);
	ENDMESSAGE;

# XS_VI_getcursor --
#	Return the current cursor position as a list with two elements.
#	{line, column}.
#
# Perl Command: VI::GetCursor
# Usage: VI::GetCursor screenId

void
GetCursor(screen)
	VI screen

	PREINIT:
	struct _mark cursor;
	void (*scr_msg) __P((SCR *, mtype_t, char *, size_t));
	int rval;

	PPCODE:
	INITMESSAGE;
	rval = api_getcursor(screen, &cursor);
	ENDMESSAGE;

	EXTEND(sp,2);
        PUSHs(sv_2mortal(newSViv(cursor.lno)));
        PUSHs(sv_2mortal(newSViv(cursor.cno)));

# XS_VI_setcursor --
#	Set the cursor to the line and column numbers supplied.
#
# Perl Command: VI::SetCursor
# Usage: VI::SetCursor screenId line column

void
SetCursor(screen, line, column)
	VI screen
	int line
	int column

	PREINIT:
	struct _mark cursor;
	void (*scr_msg) __P((SCR *, mtype_t, char *, size_t));
	int rval;

	CODE:
	INITMESSAGE;
	cursor.lno = line;
	cursor.cno = column;
	rval = api_setcursor(screen, &cursor);
	ENDMESSAGE;

# XS_VI_swscreen --
#	Change the current focus to screen.
#
# Perl Command: VI::SwitchScreen
# Usage: VI::SwitchScreen screenId screenId

void
SwitchScreen(screenFrom, screenTo)
	VI screenFrom
	VI screenTo

	PREINIT:
	void (*scr_msg) __P((SCR *, mtype_t, char *, size_t));
	int rval;

	CODE:
	INITMESSAGE;
	rval = api_swscreen(screenFrom, screenTo);
	ENDMESSAGE;

# XS_VI_map --
#	Associate a key with a perl procedure.
#
# Perl Command: VI::MapKey
# Usage: VI::MapKey screenId key perlproc

void
MapKey(screen, key, perlproc)
	VI screen
	char *key
	SV *perlproc

	PREINIT:
	void (*scr_msg) __P((SCR *, mtype_t, char *, size_t));
	int rval;
	int length;
	char *command;
	SV *svc;

	CODE:
	INITMESSAGE;
	svc = sv_2mortal(newSVpv(":perl ", 6));
	sv_catsv(svc, perlproc);
	command = SvPV(svc, length);
	rval = api_map(screen, key, command, length);
	ENDMESSAGE;

# XS_VI_unmap --
#	Unmap a key.
#
# Perl Command: VI::UnmapKey
# Usage: VI::UnmmapKey screenId key

void
UnmapKey(screen, key)
	VI screen
	char *key

	PREINIT:
	void (*scr_msg) __P((SCR *, mtype_t, char *, size_t));
	int rval;

	CODE:
	INITMESSAGE;
	rval = api_unmap(screen, key);
	ENDMESSAGE;

# XS_VI_opts_set --
#	Set an option.
#
# Perl Command: VI::SetOpt
# Usage: VI::SetOpt screenId setting

void
SetOpt(screen, setting)
	VI screen
	char *setting

	PREINIT:
	void (*scr_msg) __P((SCR *, mtype_t, char *, size_t));
	int rval;
	SV *svc;

	CODE:
	INITMESSAGE;
	svc = sv_2mortal(newSVpv(":set ", 5));
	sv_catpv(svc, setting);
	rval = api_run_str(screen, SvPV(svc, na));
	ENDMESSAGE;

# XS_VI_opts_get --
#	Return the value of an option.
#	
# Perl Command: VI::GetOpt
# Usage: VI::GetOpt screenId option

void
GetOpt(screen, option)
	VI screen
	char *option

	PREINIT:
	void (*scr_msg) __P((SCR *, mtype_t, char *, size_t));
	int rval;
	char *value;

	PPCODE:
	INITMESSAGE;
	rval = api_opts_get(screen, option, &value, NULL);
	ENDMESSAGE;

	EXTEND(SP,1);
	PUSHs(sv_2mortal(newSVpv(value, 0)));
	free(value);

# XS_VI_run --
#	Run the ex command cmd.
#
# Perl Command: VI::Run
# Usage: VI::Run screenId cmd

void
Run(screen, command)
	VI screen
	char *command;

	PREINIT:
	void (*scr_msg) __P((SCR *, mtype_t, char *, size_t));
	int rval;

	CODE:
	INITMESSAGE;
	rval = api_run_str(screen, command);
	ENDMESSAGE;

void 
DESTROY(screen)
	VI screen

	CODE:
	screen->perl_private = 0;

void
Warn(warning)
	char *warning;

	PREINIT:
	int i;
	CODE:
	sv_catpv(GvSV(errgv),warning);

#define TIED(package) \
	sv_magic((SV *) (hv = \
	    (HV *)sv_2mortal((SV *)newHV())), \
		sv_setref_pv(sv_newmortal(), package, \
			newVIrv(newSV(0), screen)),\
		'P', Nullch, 0);\
	RETVAL = newRV((SV *)hv)

SV *
Opt(screen)
	VI screen;
	PREINIT:
	HV *hv;
	CODE:
	TIED("VI::OPT");
	OUTPUT:
	RETVAL

SV *
Map(screen)
	VI screen;
	PREINIT:
	HV *hv;
	CODE:
	TIED("VI::MAP");
	OUTPUT:
	RETVAL

SV *
Mark(screen)
	VI screen
	PREINIT:
	HV *hv;
	CODE:
	TIED("VI::MARK");
	OUTPUT:
	RETVAL

MODULE = VI	PACKAGE = VI::OPT

void 
DESTROY(screen)
	VI::OPT screen

	CODE:
	# typemap did all the checking
	SvREFCNT_dec((SV*)SvIV((SV*)SvRV(ST(0))));

void
FETCH(screen, key)
	VI::OPT screen
	char *key

	PREINIT:
	void (*scr_msg) __P((SCR *, mtype_t, char *, size_t));
	int rval;
	char *value;
	int boolvalue;

	PPCODE:
	INITMESSAGE;
	rval = api_opts_get(screen, key, &value, &boolvalue);
	if (!rval) {
		EXTEND(SP,1);
		PUSHs(sv_2mortal((boolvalue == -1) ? newSVpv(value, 0)
						   : newSViv(boolvalue)));
		free(value);
	} else ST(0) = &sv_undef;
	rval = 0;
	ENDMESSAGE;

void
STORE(screen, key, value)
	VI::OPT	screen
	char	*key
	SV	*value

	PREINIT:
	void (*scr_msg) __P((SCR *, mtype_t, char *, size_t));
	int rval;

	CODE:
	INITMESSAGE;
	rval = api_opts_set(screen, key, SvPV(value, na), SvIV(value), 
                                         SvTRUEx(value));
	ENDMESSAGE;

MODULE = VI	PACKAGE = VI::MAP

void 
DESTROY(screen)
	VI::MAP screen

	CODE:
	# typemap did all the checking
	SvREFCNT_dec((SV*)SvIV((SV*)SvRV(ST(0))));

void
STORE(screen, key, perlproc)
	VI::MAP screen
	char *key
	SV *perlproc

	PREINIT:
	void (*scr_msg) __P((SCR *, mtype_t, char *, size_t));
	int rval;
	int length;
	char *command;
	SV *svc;

	CODE:
	INITMESSAGE;
	svc = sv_2mortal(newSVpv(":perl ", 6));
	sv_catsv(svc, perlproc);
	command = SvPV(svc, length);
	rval = api_map(screen, key, command, length);
	ENDMESSAGE;

void
DELETE(screen, key)
	VI::MAP screen
	char *key

	PREINIT:
	void (*scr_msg) __P((SCR *, mtype_t, char *, size_t));
	int rval;

	CODE:
	INITMESSAGE;
	rval = api_unmap(screen, key);
	ENDMESSAGE;

MODULE = VI	PACKAGE = VI::MARK

void 
DESTROY(screen)
	VI::MARK screen

	CODE:
	# typemap did all the checking
	SvREFCNT_dec((SV*)SvIV((SV*)SvRV(ST(0))));

AV *
FETCH(screen, mark)
	VI::MARK screen
	char mark

	PREINIT:
	struct _mark cursor;
	void (*scr_msg) __P((SCR *, mtype_t, char *, size_t));
	int rval;

	CODE:
	INITMESSAGE;
	rval = api_getmark(screen, (int)mark, &cursor);
	ENDMESSAGE;
	RETVAL = newAV();
	av_push(RETVAL, newSViv(cursor.lno));
	av_push(RETVAL, newSViv(cursor.cno));

	OUTPUT:
	RETVAL

void
STORE(screen, mark, pos)
	VI::MARK screen
	char mark
	AVREF pos

	PREINIT:
	struct _mark cursor;
	void (*scr_msg) __P((SCR *, mtype_t, char *, size_t));
	int rval;

	CODE:
	if (av_len(pos) < 1) 
	    croak("cursor position needs 2 elements");
	INITMESSAGE;
	cursor.lno = SvIV(*av_fetch(pos, 0, 0));
	cursor.cno = SvIV(*av_fetch(pos, 1, 0));
	rval = api_setmark(screen, (int)mark, &cursor);
	ENDMESSAGE;

void
FIRSTKEY(screen, ...)
	VI::MARK screen

	ALIAS:
	NEXTKEY = 1
	
	PROTOTYPE: $;$

	PREINIT:
	struct _mark cursor;
	void (*scr_msg) __P((SCR *, mtype_t, char *, size_t));
	int next;
	char key[] = {0, 0};

	PPCODE:
	if (items == 2) {
		next = 1;
		*key = *(char *)SvPV(ST(1),na);
	} else next = 0;
	if (api_nextmark(screen, next, key) != 1) {
		EXTEND(sp, 1);
        	PUSHs(sv_2mortal(newSVpv(key, 1)));
	} else ST(0) = &sv_undef;
