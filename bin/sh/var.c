/*-
 * Copyright (c) 1991 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Kenneth Almquist.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef lint
/*static char sccsid[] = "from: @(#)var.c	5.3 (Berkeley) 4/12/91";*/
static char rcsid[] = "var.c,v 1.4 1993/08/01 18:57:58 mycroft Exp";
#endif /* not lint */

/*
 * Shell variables.
 */

#include "shell.h"
#include "output.h"
#include "expand.h"
#include "nodes.h"	/* for other headers */
#include "eval.h"	/* defines cmdenviron */
#include "exec.h"
#include "syntax.h"
#include "options.h"
#include "mail.h"
#include "var.h"
#include "memalloc.h"
#include "error.h"
#include "mystring.h"


#define VTABSIZE 39


struct varinit {
	struct var *var;
	int flags;
	char *text;
};


#if ATTY
struct var vatty;
#endif
struct var vifs;
struct var vmail;
struct var vmpath;
struct var vpath;
struct var vps1;
struct var vps2;
struct var vvers;
#if ATTY
struct var vterm;
#endif

const struct varinit varinit[] = {
#if ATTY
	{&vatty,	VSTRFIXED|VTEXTFIXED|VUNSET,	"ATTY="},
#endif
	{&vifs,	VSTRFIXED|VTEXTFIXED,		"IFS= \t\n"},
	{&vmail,	VSTRFIXED|VTEXTFIXED|VUNSET,	"MAIL="},
	{&vmpath,	VSTRFIXED|VTEXTFIXED|VUNSET,	"MAILPATH="},
	{&vpath,	VSTRFIXED|VTEXTFIXED,		"PATH=:/bin:/usr/bin"},
	/* 
	 * vps1 depends on uid
	 */
	{&vps2,	VSTRFIXED|VTEXTFIXED,		"PS2=> "},
#if ATTY
	{&vterm,	VSTRFIXED|VTEXTFIXED|VUNSET,	"TERM="},
#endif
	{NULL,	0,				NULL}
};

struct var *vartab[VTABSIZE];

STATIC void unsetvar __P((char *));
STATIC struct var **hashvar __P((char *));
STATIC int varequal __P((char *, char *));

/*
 * Initialize the varable symbol tables and import the environment
 */

#ifdef mkinit
INCLUDE "var.h"
INIT {
	char **envp;
	extern char **environ;

	initvar();
	for (envp = environ ; *envp ; envp++) {
		if (strchr(*envp, '=')) {
			setvareq(*envp, VEXPORT|VTEXTFIXED);
		}
	}
}
#endif


/*
 * This routine initializes the builtin variables.  It is called when the
 * shell is initialized and again when a shell procedure is spawned.
 */

void
initvar() {
	const struct varinit *ip;
	struct var *vp;
	struct var **vpp;

	for (ip = varinit ; (vp = ip->var) != NULL ; ip++) {
		if ((vp->flags & VEXPORT) == 0) {
			vpp = hashvar(ip->text);
			vp->next = *vpp;
			*vpp = vp;
			vp->text = ip->text;
			vp->flags = ip->flags;
		}
	}
	/*
	 * PS1 depends on uid
	 */
	if ((vps1.flags & VEXPORT) == 0) {
		vpp = hashvar("PS1=");
		vps1.next = *vpp;
		*vpp = &vps1;
		vps1.text = getuid() ? "PS1=$ " : "PS1=# ";
		vps1.flags = VSTRFIXED|VTEXTFIXED;
	}
}

/*
 * Set the value of a variable.  The flags argument is ored with the
 * flags of the variable.  If val is NULL, the variable is unset.
 */

void
setvar(name, val, flags)
	char *name, *val;
	{
	char *p, *q;
	int len;
	int namelen;
	char *nameeq;
	int isbad;

	isbad = 0;
	p = name;
	if (! is_name(*p++))
		isbad = 1;
	for (;;) {
		if (! is_in_name(*p)) {
			if (*p == '\0' || *p == '=')
				break;
			isbad = 1;
		}
		p++;
	}
	namelen = p - name;
	if (isbad)
		error("%.*s: is read only", namelen, name);
	len = namelen + 2;		/* 2 is space for '=' and '\0' */
	if (val == NULL) {
		flags |= VUNSET;
	} else {
		len += strlen(val);
	}
	p = nameeq = ckmalloc(len);
	q = name;
	while (--namelen >= 0)
		*p++ = *q++;
	*p++ = '=';
	*p = '\0';
	if (val)
		scopy(val, p);
	setvareq(nameeq, flags);
}



/*
 * Same as setvar except that the variable and value are passed in
 * the first argument as name=value.  Since the first argument will
 * be actually stored in the table, it should not be a string that
 * will go away.
 */

void
setvareq(s, flags)
	char *s;
	{
	struct var *vp, **vpp;

	vpp = hashvar(s);
	for (vp = *vpp ; vp ; vp = vp->next) {
		if (varequal(s, vp->text)) {
			if (vp->flags & VREADONLY) {
				int len = strchr(s, '=') - s;
				error("%.*s: is read only", len, s);
			}
			INTOFF;
			if (vp == &vpath)
				changepath(s + 5);	/* 5 = strlen("PATH=") */
			if ((vp->flags & (VTEXTFIXED|VSTACK)) == 0)
				ckfree(vp->text);
			vp->flags &=~ (VTEXTFIXED|VSTACK|VUNSET);
			vp->flags |= flags;
			vp->text = s;
			if (vp == &vmpath || (vp == &vmail && ! mpathset()))
				chkmail(1);
			INTON;
			return;
		}
	}
	/* not found */
	vp = ckmalloc(sizeof (*vp));
	vp->flags = flags;
	vp->text = s;
	vp->next = *vpp;
	*vpp = vp;
}



/*
 * Process a linked list of variable assignments.
 */

void
listsetvar(list)
	struct strlist *list;
	{
	struct strlist *lp;

	INTOFF;
	for (lp = list ; lp ; lp = lp->next) {
		setvareq(savestr(lp->text), 0);
	}
	INTON;
}



/*
 * Find the value of a variable.  Returns NULL if not set.
 */

char *
lookupvar(name)
	char *name;
	{
	struct var *v;

	for (v = *hashvar(name) ; v ; v = v->next) {
		if (varequal(v->text, name)) {
			if (v->flags & VUNSET)
				return NULL;
			return strchr(v->text, '=') + 1;
		}
	}
	return NULL;
}



/*
 * Search the environment of a builtin command.  If the second argument
 * is nonzero, return the value of a variable even if it hasn't been
 * exported.
 */

char *
bltinlookup(name, doall)
	char *name;
	{
	struct strlist *sp;
	struct var *v;

	for (sp = cmdenviron ; sp ; sp = sp->next) {
		if (varequal(sp->text, name))
			return strchr(sp->text, '=') + 1;
	}
	for (v = *hashvar(name) ; v ; v = v->next) {
		if (varequal(v->text, name)) {
			if (v->flags & VUNSET
			 || ! doall && (v->flags & VEXPORT) == 0)
				return NULL;
			return strchr(v->text, '=') + 1;
		}
	}
	return NULL;
}



/*
 * Generate a list of exported variables.  This routine is used to construct
 * the third argument to execve when executing a program.
 */

char **
environment() {
	int nenv;
	struct var **vpp;
	struct var *vp;
	char **env, **ep;

	nenv = 0;
	for (vpp = vartab ; vpp < vartab + VTABSIZE ; vpp++) {
		for (vp = *vpp ; vp ; vp = vp->next)
			if (vp->flags & VEXPORT)
				nenv++;
	}
	ep = env = stalloc((nenv + 1) * sizeof *env);
	for (vpp = vartab ; vpp < vartab + VTABSIZE ; vpp++) {
		for (vp = *vpp ; vp ; vp = vp->next)
			if (vp->flags & VEXPORT)
				*ep++ = vp->text;
	}
	*ep = NULL;
	return env;
}


/*
 * Called when a shell procedure is invoked to clear out nonexported
 * variables.  It is also necessary to reallocate variables of with
 * VSTACK set since these are currently allocated on the stack.
 */

#ifdef mkinit
MKINIT void shprocvar();

SHELLPROC {
	shprocvar();
}
#endif

void
shprocvar() {
	struct var **vpp;
	struct var *vp, **prev;

	for (vpp = vartab ; vpp < vartab + VTABSIZE ; vpp++) {
		for (prev = vpp ; (vp = *prev) != NULL ; ) {
			if ((vp->flags & VEXPORT) == 0) {
				*prev = vp->next;
				if ((vp->flags & VTEXTFIXED) == 0)
					ckfree(vp->text);
				if ((vp->flags & VSTRFIXED) == 0)
					ckfree(vp);
			} else {
				if (vp->flags & VSTACK) {
					vp->text = savestr(vp->text);
					vp->flags &=~ VSTACK;
				}
				prev = &vp->next;
			}
		}
	}
	initvar();
}



/*
 * Command to list all variables which are set.  Currently this command
 * is invoked from the set command when the set command is called without
 * any variables.
 */

int
showvarscmd(argc, argv)  char **argv; {
	struct var **vpp;
	struct var *vp;

	for (vpp = vartab ; vpp < vartab + VTABSIZE ; vpp++) {
		for (vp = *vpp ; vp ; vp = vp->next) {
			if ((vp->flags & VUNSET) == 0)
				out1fmt("%s\n", vp->text);
		}
	}
	return 0;
}



/*
 * The export and readonly commands.
 */

int
exportcmd(argc, argv)  char **argv; {
	struct var **vpp;
	struct var *vp;
	char *name;
	char *p;
	int flag = argv[0][0] == 'r'? VREADONLY : VEXPORT;

	listsetvar(cmdenviron);
	if (argc > 1) {
		while ((name = *argptr++) != NULL) {
			if ((p = strchr(name, '=')) != NULL) {
				p++;
			} else {
				vpp = hashvar(name);
				for (vp = *vpp ; vp ; vp = vp->next) {
					if (varequal(vp->text, name)) {
						vp->flags |= flag;
						goto found;
					}
				}
			}
			setvar(name, p, flag);
found:;
		}
	} else {
		for (vpp = vartab ; vpp < vartab + VTABSIZE ; vpp++) {
			for (vp = *vpp ; vp ; vp = vp->next) {
				if (vp->flags & flag) {
					for (p = vp->text ; *p != '=' ; p++)
						out1c(*p);
					out1c('\n');
				}
			}
		}
	}
	return 0;
}


/*
 * The "local" command.
 */

localcmd(argc, argv)  char **argv; {
	char *name;

	if (! in_function())
		error("Not in a function");
	while ((name = *argptr++) != NULL) {
		mklocal(name);
	}
	return 0;
}


/*
 * Make a variable a local variable.  When a variable is made local, it's
 * value and flags are saved in a localvar structure.  The saved values
 * will be restored when the shell function returns.  We handle the name
 * "-" as a special case.
 */

void
mklocal(name)
	char *name;
	{
	struct localvar *lvp;
	struct var **vpp;
	struct var *vp;

	INTOFF;
	lvp = ckmalloc(sizeof (struct localvar));
	if (name[0] == '-' && name[1] == '\0') {
		lvp->text = ckmalloc(sizeof optval);
		bcopy(optval, lvp->text, sizeof optval);
		vp = NULL;
	} else {
		vpp = hashvar(name);
		for (vp = *vpp ; vp && ! varequal(vp->text, name) ; vp = vp->next);
		if (vp == NULL) {
			if (strchr(name, '='))
				setvareq(savestr(name), VSTRFIXED);
			else
				setvar(name, NULL, VSTRFIXED);
			vp = *vpp;	/* the new variable */
			lvp->text = NULL;
			lvp->flags = VUNSET;
		} else {
			lvp->text = vp->text;
			lvp->flags = vp->flags;
			vp->flags |= VSTRFIXED|VTEXTFIXED;
			if (strchr(name, '='))
				setvareq(savestr(name), 0);
		}
	}
	lvp->vp = vp;
	lvp->next = localvars;
	localvars = lvp;
	INTON;
}


/*
 * Called after a function returns.
 */

void
poplocalvars() {
	struct localvar *lvp;
	struct var *vp;

	while ((lvp = localvars) != NULL) {
		localvars = lvp->next;
		vp = lvp->vp;
		if (vp == NULL) {	/* $- saved */
			bcopy(lvp->text, optval, sizeof optval);
			ckfree(lvp->text);
		} else if ((lvp->flags & (VUNSET|VSTRFIXED)) == VUNSET) {
			unsetvar(vp->text);
		} else {
			if ((vp->flags & VTEXTFIXED) == 0)
				ckfree(vp->text);
			vp->flags = lvp->flags;
			vp->text = lvp->text;
		}
		ckfree(lvp);
	}
}


setvarcmd(argc, argv)  char **argv; {
	if (argc <= 2)
		return unsetcmd(argc, argv);
	else if (argc == 3)
		setvar(argv[1], argv[2], 0);
	else
		error("List assignment not implemented");
	return 0;
}


/*
 * The unset builtin command.  We unset the function before we unset the
 * variable to allow a function to be unset when there is a readonly variable
 * with the same name.
 */

unsetcmd(argc, argv)  char **argv; {
	char **ap;

	for (ap = argv + 1 ; *ap ; ap++) {
		unsetfunc(*ap);
		unsetvar(*ap);
	}
	return 0;
}


/*
 * Unset the specified variable.
 */

STATIC void
unsetvar(s)
	char *s;
	{
	struct var **vpp;
	struct var *vp;

	vpp = hashvar(s);
	for (vp = *vpp ; vp ; vpp = &vp->next, vp = *vpp) {
		if (varequal(vp->text, s)) {
			INTOFF;
			if (*(strchr(vp->text, '=') + 1) != '\0'
			 || vp->flags & VREADONLY) {
				setvar(s, nullstr, 0);
			}
			vp->flags &=~ VEXPORT;
			vp->flags |= VUNSET;
			if ((vp->flags & VSTRFIXED) == 0) {
				if ((vp->flags & VTEXTFIXED) == 0)
					ckfree(vp->text);
				*vpp = vp->next;
				ckfree(vp);
			}
			INTON;
			return;
		}
	}
}



/*
 * Find the appropriate entry in the hash table from the name.
 */

STATIC struct var **
hashvar(p)
	register char *p;
	{
	unsigned int hashval;

	hashval = *p << 4;
	while (*p && *p != '=')
		hashval += *p++;
	return &vartab[hashval % VTABSIZE];
}



/*
 * Returns true if the two strings specify the same varable.  The first
 * variable name is terminated by '='; the second may be terminated by
 * either '=' or '\0'.
 */

STATIC int
varequal(p, q)
	register char *p, *q;
	{
	while (*p == *q++) {
		if (*p++ == '=')
			return 1;
	}
	if (*p == '=' && *(q - 1) == '\0')
		return 1;
	return 0;
}
