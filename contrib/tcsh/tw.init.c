/* $Header: /src/pub/tcsh/tw.init.c,v 3.29 2002/06/25 19:02:12 christos Exp $ */
/*
 * tw.init.c: Handle lists of things to complete
 */
/*-
 * Copyright (c) 1980, 1991 The Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
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
#include "sh.h"

RCSID("$Id: tw.init.c,v 3.29 2002/06/25 19:02:12 christos Exp $")

#include "tw.h"
#include "ed.h"
#include "tc.h"
#include "sh.proc.h"

#if !defined(NSIG) && defined(SIGMAX)
# define NSIG (SIGMAX+1)
#endif /* !NSIG && SIGMAX */
#if !defined(NSIG) && defined(_NSIG)
# define NSIG _NSIG
#endif /* !NSIG && _NSIG */

#define TW_INCR	128

typedef struct {
    Char **list, 			/* List of command names	*/
	  *buff;			/* Space holding command names	*/
    int    nlist, 			/* Number of items		*/
           nbuff,			/* Current space in name buf	*/
           tlist,			/* Total space in list		*/
	   tbuff;			/* Total space in name buf	*/
} stringlist_t;


static struct varent *tw_vptr = NULL;	/* Current shell variable 	*/
static Char **tw_env = NULL;		/* Current environment variable */
static Char  *tw_word;			/* Current word pointer		*/
static struct KeyFuncs *tw_bind = NULL;	/* List of the bindings		*/
#ifndef HAVENOLIMIT
static struct limits *tw_limit = NULL;	/* List of the resource limits	*/
#endif /* HAVENOLIMIT */
static int tw_index = 0;		/* signal and job index		*/
static DIR   *tw_dir_fd = NULL;		/* Current directory descriptor	*/
static Char   tw_retname[MAXPATHLEN+1];	/* Return buffer		*/
static int    tw_cmd_got = 0;		/* What we need to do		*/
static stringlist_t tw_cmd  = { NULL, NULL, 0, 0, 0, 0 };
static stringlist_t tw_item = { NULL, NULL, 0, 0, 0, 0 };
#define TW_FL_CMD	0x01
#define TW_FL_ALIAS	0x02
#define TW_FL_BUILTIN	0x04
#define TW_FL_SORT	0x08
#define TW_FL_REL	0x10

static struct {				/* Current element pointer	*/
    int    cur;				/* Current element number	*/
    Char **pathv;			/* Current element in path	*/
    DIR   *dfd;				/* Current directory descriptor	*/
} tw_cmd_state;


#ifdef BSDSIGS
static sigmask_t tw_omask;
# define TW_HOLD()	tw_omask = sigblock(sigmask(SIGINT))
# define TW_RELS()	(void) sigsetmask(tw_omask)
#else /* !BSDSIGS */
# define TW_HOLD()	(void) sighold(SIGINT)
# define TW_RELS()	(void) sigrelse(SIGINT)
#endif /* BSDSIGS */

#define SETDIR(dfd) \
    { \
	tw_dir_fd = dfd; \
	if (tw_dir_fd != NULL) \
	    rewinddir(tw_dir_fd); \
    }

#define CLRDIR(dfd) \
    if (dfd != NULL) { \
	TW_HOLD(); \
	(void) closedir(dfd); \
	dfd = NULL; \
	TW_RELS(); \
    }

static Char	*tw_str_add		__P((stringlist_t *, int));
static void	 tw_str_free		__P((stringlist_t *));
static Char     *tw_dir_next		__P((DIR *));
static void	 tw_cmd_add 		__P((Char *name));
static void 	 tw_cmd_cmd		__P((void));
static void	 tw_cmd_builtin		__P((void));
static void	 tw_cmd_alias		__P((void));
static void	 tw_cmd_sort		__P((void));
static void 	 tw_vptr_start		__P((struct varent *));


/* tw_str_add():
 *	Add an item to the string list
 */
static Char *
tw_str_add(sl, len)
    stringlist_t *sl;
    int len;
{
    Char *ptr;

    if (sl->tlist <= sl->nlist) {
	TW_HOLD();
	sl->tlist += TW_INCR;
	sl->list = sl->list ? 
		    (Char **) xrealloc((ptr_t) sl->list, 
				       (size_t) (sl->tlist * sizeof(Char *))) :
		    (Char **) xmalloc((size_t) (sl->tlist * sizeof(Char *)));
	TW_RELS();
    }
    if (sl->tbuff <= sl->nbuff + len) {
	int i;
	ptr = sl->buff;

	TW_HOLD();
	sl->tbuff += TW_INCR + len;
	sl->buff = sl->buff ? 
		    (Char *) xrealloc((ptr_t) sl->buff, 
				      (size_t) (sl->tbuff * sizeof(Char))) :
		    (Char *) xmalloc((size_t) (sl->tbuff * sizeof(Char)));
	/* Re-thread the new pointer list, if changed */
	if (ptr != NULL && ptr != sl->buff) {
	    int offs = (int) (sl->buff - ptr);
	    for (i = 0; i < sl->nlist; i++)
		sl->list[i] += offs;
	}
	TW_RELS();
    }
    ptr = sl->list[sl->nlist++] = &sl->buff[sl->nbuff];
    sl->nbuff += len;
    return ptr;
} /* tw_str_add */


/* tw_str_free():
 *	Free a stringlist
 */
static void
tw_str_free(sl)
    stringlist_t *sl;
{
    TW_HOLD();
    if (sl->list) {
	xfree((ptr_t) sl->list);
	sl->list = NULL;
	sl->tlist = sl->nlist = 0;
    }
    if (sl->buff) {
	xfree((ptr_t) sl->buff);
	sl->buff = NULL;
	sl->tbuff = sl->nbuff = 0;
    }
    TW_RELS();
} /* end tw_str_free */


static Char *
tw_dir_next(dfd)
    DIR *dfd;
{
    register struct dirent *dirp;

    if (dfd == NULL)
	return NULL;

    if ((dirp = readdir(dfd)) != NULL) {
	(void) Strcpy(tw_retname, str2short(dirp->d_name));
	return (tw_retname);
    }
    return NULL;
} /* end tw_dir_next */


/* tw_cmd_add():
 *	Add the name to the command list
 */
static void
tw_cmd_add(name)
    Char *name;
{
    int len;

    len = (int) Strlen(name) + 2;
    (void) Strcpy(tw_str_add(&tw_cmd, len), name);
} /* end tw_cmd_add */


/* tw_cmd_free():
 *	Free the command list
 */
void
tw_cmd_free()
{
    CLRDIR(tw_dir_fd)
    tw_str_free(&tw_cmd);
    tw_cmd_got = 0;
} /* end tw_cmd_free */

/* tw_cmd_cmd():
 *	Add system commands to the command list
 */
static void
tw_cmd_cmd()
{
    register DIR *dirp;
    register struct dirent *dp;
    register Char *dir = NULL, *name;
    register Char **pv;
    struct varent *v = adrof(STRpath);
    struct varent *recexec = adrof(STRrecognize_only_executables);
    int len;


    if (v == NULL || v->vec == NULL) /* if no path */
	return;

    for (pv = v->vec; *pv; pv++) {
	if (pv[0][0] != '/') {
	    tw_cmd_got |= TW_FL_REL;
	    continue;
	}

	if ((dirp = opendir(short2str(*pv))) == NULL)
	    continue;

	if (recexec)
	    dir = Strspl(*pv, STRslash);
	while ((dp = readdir(dirp)) != NULL) {
#if defined(_UWIN) || defined(__CYGWIN__)
	    /* Turn foo.{exe,com,bat} into foo since UWIN's readdir returns
	     * the file with the .exe, .com, .bat extension
	     */
	    size_t ext = strlen(dp->d_name) - 4;
	    if ((ext > 0) && (strcmp(&dp->d_name[ext], ".exe") == 0 ||
		strcmp(&dp->d_name[ext], ".bat") == 0 ||
		strcmp(&dp->d_name[ext], ".com") == 0))
		dp->d_name[ext] = '\0';
#endif /* _UWIN || __CYGWIN__ */
	    /* the call to executable() may make this a bit slow */
	    name = str2short(dp->d_name);
	    if (dp->d_ino == 0 || (recexec && !executable(dir, name, 0)))
		continue;
            len = (int) Strlen(name) + 2;
            if (name[0] == '#' ||	/* emacs temp files	*/
		name[0] == '.' ||	/* .files		*/
		name[len - 3] == '~' ||	/* emacs backups	*/
		name[len - 3] == '%')	/* textedit backups	*/
                continue;		/* Ignore!		*/
            tw_cmd_add(name);
	}
	(void) closedir(dirp);
	if (recexec)
	    xfree((ptr_t) dir);
    }
} /* end tw_cmd_cmd */


/* tw_cmd_builtin():
 *	Add builtins to the command list
 */
static void
tw_cmd_builtin()
{
    register struct biltins *bptr;

    for (bptr = bfunc; bptr < &bfunc[nbfunc]; bptr++)
	if (bptr->bname)
	    tw_cmd_add(str2short(bptr->bname));
#ifdef WINNT_NATIVE
    for (bptr = nt_bfunc; bptr < &nt_bfunc[nt_nbfunc]; bptr++)
	if (bptr->bname)
	    tw_cmd_add(str2short(bptr->bname));
#endif /* WINNT_NATIVE*/
} /* end tw_cmd_builtin */


/* tw_cmd_alias():
 *	Add aliases to the command list
 */
static void
tw_cmd_alias()
{
    register struct varent *p;
    register struct varent *c;

    p = &aliases;
    for (;;) {
	while (p->v_left)
	    p = p->v_left;
x:
	if (p->v_parent == 0) /* is it the header? */
	    return;
	if (p->v_name)
	    tw_cmd_add(p->v_name);
	if (p->v_right) {
	    p = p->v_right;
	    continue;
	}
	do {
	    c = p;
	    p = p->v_parent;
	} while (p->v_right == c);
	goto x;
    }
} /* end tw_cmd_alias */


/* tw_cmd_sort():
 *	Sort the command list removing duplicate elements
 */
static void
tw_cmd_sort()
{
    int fwd, i;

    TW_HOLD();
    /* sort the list. */
    qsort((ptr_t) tw_cmd.list, (size_t) tw_cmd.nlist, sizeof(Char *), 
	  (int (*) __P((const void *, const void *))) fcompare);

    /* get rid of multiple entries */
    for (i = 0, fwd = 0; i < tw_cmd.nlist - 1; i++) {
	if (Strcmp(tw_cmd.list[i], tw_cmd.list[i + 1]) == 0) /* garbage */
	    fwd++;		/* increase the forward ref. count */
	else if (fwd) 
	    tw_cmd.list[i - fwd] = tw_cmd.list[i];
    }
    /* Fix fencepost error -- Theodore Ts'o <tytso@athena.mit.edu> */
    if (fwd)
	tw_cmd.list[i - fwd] = tw_cmd.list[i];
    tw_cmd.nlist -= fwd;
    TW_RELS();
} /* end tw_cmd_sort */


/* tw_cmd_start():
 *	Get the command list and sort it, if not done yet.
 *	Reset the current pointer to the beginning of the command list
 */
/*ARGSUSED*/
void
tw_cmd_start(dfd, pat)
    DIR *dfd;
    Char *pat;
{
    static Char *defpath[] = { STRNULL, 0 };
    USE(pat);
    SETDIR(dfd)
    if ((tw_cmd_got & TW_FL_CMD) == 0) {
	tw_cmd_free();
	tw_cmd_cmd();
	tw_cmd_got |= TW_FL_CMD;
    }
    if ((tw_cmd_got & TW_FL_ALIAS) == 0) {
	tw_cmd_alias();
	tw_cmd_got &= ~TW_FL_SORT;
	tw_cmd_got |= TW_FL_ALIAS;
    }
    if ((tw_cmd_got & TW_FL_BUILTIN) == 0) {
	tw_cmd_builtin();
	tw_cmd_got &= ~TW_FL_SORT;
	tw_cmd_got |= TW_FL_BUILTIN;
    }
    if ((tw_cmd_got & TW_FL_SORT) == 0) {
	tw_cmd_sort();
	tw_cmd_got |= TW_FL_SORT;
    }

    tw_cmd_state.cur = 0;
    CLRDIR(tw_cmd_state.dfd)
    if (tw_cmd_got & TW_FL_REL) {
	struct varent *vp = adrof(STRpath);
	if (vp && vp->vec)
	    tw_cmd_state.pathv = vp->vec;
	else
	    tw_cmd_state.pathv = defpath;
    }
    else 
	tw_cmd_state.pathv = defpath;
} /* tw_cmd_start */


/* tw_cmd_next():
 *	Return the next element in the command list or
 *	Look for commands in the relative path components
 */
Char *
tw_cmd_next(dir, flags)
    Char *dir;
    int  *flags;
{
    Char *ptr = NULL;

    if (tw_cmd_state.cur < tw_cmd.nlist) {
	*flags = TW_DIR_OK;
	return tw_cmd.list[tw_cmd_state.cur++];
    }

    /*
     * We need to process relatives in the path.
     */
    while (((tw_cmd_state.dfd == NULL) ||
	    ((ptr = tw_dir_next(tw_cmd_state.dfd)) == NULL)) &&
	   (*tw_cmd_state.pathv != NULL)) {

        CLRDIR(tw_cmd_state.dfd)

	while (*tw_cmd_state.pathv && tw_cmd_state.pathv[0][0] == '/')
	    tw_cmd_state.pathv++;
	if ((ptr = *tw_cmd_state.pathv) != 0) {
	    /*
	     * We complete directories only on '.' should that
	     * be changed?
	     */
	    if (ptr[0] == '\0' || (ptr[0] == '.' && ptr[1] == '\0')) {
		*dir = '\0';
		tw_cmd_state.dfd = opendir(".");
		*flags = TW_DIR_OK | TW_EXEC_CHK;	
	    }
	    else {
		copyn(dir, *tw_cmd_state.pathv, FILSIZ);
		catn(dir, STRslash, FILSIZ);
		tw_cmd_state.dfd = opendir(short2str(*tw_cmd_state.pathv));
		*flags = TW_EXEC_CHK;
	    }
	    tw_cmd_state.pathv++;
	}
    }
    return ptr;
} /* end tw_cmd_next */


/* tw_vptr_start():
 *	Find the first variable in the variable list
 */
static void
tw_vptr_start(c)
    struct varent *c;
{
    tw_vptr = c;		/* start at beginning of variable list */

    for (;;) {
	while (tw_vptr->v_left)
	    tw_vptr = tw_vptr->v_left;
x:
	if (tw_vptr->v_parent == 0) {	/* is it the header? */
	    tw_vptr = NULL;
	    return;
	}
	if (tw_vptr->v_name)
	    return;		/* found first one */
	if (tw_vptr->v_right) {
	    tw_vptr = tw_vptr->v_right;
	    continue;
	}
	do {
	    c = tw_vptr;
	    tw_vptr = tw_vptr->v_parent;
	} while (tw_vptr->v_right == c);
	goto x;
    }
} /* end tw_shvar_start */


/* tw_shvar_next():
 *	Return the next shell variable
 */
/*ARGSUSED*/
Char *
tw_shvar_next(dir, flags)
    Char *dir;
    int	 *flags;
{
    register struct varent *p;
    register struct varent *c;
    register Char *cp;

    USE(flags);
    USE(dir);
    if ((p = tw_vptr) == NULL)
	return (NULL);		/* just in case */

    cp = p->v_name;		/* we know that this name is here now */

    /* now find the next one */
    for (;;) {
	if (p->v_right) {	/* if we can go right */
	    p = p->v_right;
	    while (p->v_left)
		p = p->v_left;
	}
	else {			/* else go up */
	    do {
		c = p;
		p = p->v_parent;
	    } while (p->v_right == c);
	}
	if (p->v_parent == 0) {	/* is it the header? */
	    tw_vptr = NULL;
	    return (cp);
	}
	if (p->v_name) {
	    tw_vptr = p;	/* save state for the next call */
	    return (cp);
	}
    }
} /* end tw_shvar_next */


/* tw_envvar_next():
 *	Return the next environment variable
 */
/*ARGSUSED*/
Char *
tw_envvar_next(dir, flags)
    Char *dir;
    int *flags;
{
    Char   *ps, *pd;

    USE(flags);
    USE(dir);
    if (tw_env == NULL || *tw_env == NULL)
	return (NULL);
    for (ps = *tw_env, pd = tw_retname;
	 *ps && *ps != '=' && pd <= &tw_retname[MAXPATHLEN]; *pd++ = *ps++)
	continue;
    *pd = '\0';
    tw_env++;
    return (tw_retname);
} /* end tw_envvar_next */


/* tw_var_start():
 *	Begin the list of the shell and environment variables
 */
/*ARGSUSED*/
void
tw_var_start(dfd, pat)
    DIR *dfd;
    Char *pat;
{
    USE(pat);
    SETDIR(dfd)
    tw_vptr_start(&shvhed);
    tw_env = STR_environ;
} /* end tw_var_start */


/* tw_alias_start():
 *	Begin the list of the shell aliases
 */
/*ARGSUSED*/
void
tw_alias_start(dfd, pat)
    DIR *dfd;
    Char *pat;
{
    USE(pat);
    SETDIR(dfd)
    tw_vptr_start(&aliases);
    tw_env = NULL;
} /* tw_alias_start */


/* tw_complete_start():
 *	Begin the list of completions
 */
/*ARGSUSED*/
void
tw_complete_start(dfd, pat)
    DIR *dfd;
    Char *pat;
{
    extern struct varent completions;

    USE(pat);
    SETDIR(dfd)
    tw_vptr_start(&completions);
    tw_env = NULL;
} /* end tw_complete_start */


/* tw_var_next():
 *	Return the next shell or environment variable
 */
Char *
tw_var_next(dir, flags)
    Char *dir;
    int  *flags;
{
    Char *ptr = NULL;

    if (tw_vptr)
	ptr = tw_shvar_next(dir, flags);
    if (!ptr && tw_env)
	ptr = tw_envvar_next(dir, flags);
    return ptr;
} /* end tw_var_next */


/* tw_logname_start():
 *	Initialize lognames to the beginning of the list
 */
/*ARGSUSED*/
void 
tw_logname_start(dfd, pat)
    DIR *dfd;
    Char *pat;
{
    USE(pat);
    SETDIR(dfd)
#if !defined(_VMS_POSIX) && !defined(WINNT_NATIVE)
    (void) setpwent();	/* Open passwd file */
#endif /* !_VMS_POSIX && !WINNT_NATIVE */
} /* end tw_logname_start */


/* tw_logname_next():
 *	Return the next entry from the passwd file
 */
/*ARGSUSED*/
Char *
tw_logname_next(dir, flags)
    Char *dir;
    int  *flags;
{
    static Char retname[MAXPATHLEN];
    struct passwd *pw;
    /*
     * We don't want to get interrupted inside getpwent()
     * because the yellow pages code is not interruptible,
     * and if we call endpwent() immediatetely after
     * (in pintr()) we may be freeing an invalid pointer
     */
    USE(flags);
    USE(dir);
    TW_HOLD();
#if !defined(_VMS_POSIX) && !defined(WINNT_NATIVE)
    /* ISC does not declare getpwent()? */
    pw = (struct passwd *) getpwent();
#else /* _VMS_POSIX || WINNT_NATIVE */
    pw = NULL;
#endif /* !_VMS_POSIX && !WINNT_NATIVE */
    TW_RELS();

    if (pw == NULL) {
#ifdef YPBUGS
	fix_yp_bugs();
#endif
	return (NULL);
    }
    (void) Strcpy(retname, str2short(pw->pw_name));
    return (retname);
} /* end tw_logname_next */


/* tw_logname_end():
 *	Close the passwd file to finish the logname list
 */
void
tw_logname_end()
{
#ifdef YPBUGS
    fix_yp_bugs();
#endif
#if !defined(_VMS_POSIX) && !defined(WINNT_NATIVE)
   (void) endpwent();
#endif /* !_VMS_POSIX && !WINNT_NATIVE */
} /* end tw_logname_end */


/* tw_grpname_start():
 *	Initialize grpnames to the beginning of the list
 */
/*ARGSUSED*/
void 
tw_grpname_start(dfd, pat)
    DIR *dfd;
    Char *pat;
{
    USE(pat);
    SETDIR(dfd)
#if !defined(_VMS_POSIX) && !defined(_OSD_POSIX) && !defined(WINNT_NATIVE)
    (void) setgrent();	/* Open group file */
#endif /* !_VMS_POSIX && !_OSD_POSIX && !WINNT_NATIVE */
} /* end tw_grpname_start */


/* tw_grpname_next():
 *	Return the next entry from the group file
 */
/*ARGSUSED*/
Char *
tw_grpname_next(dir, flags)
    Char *dir;
    int  *flags;
{
    static Char retname[MAXPATHLEN];
    struct group *gr;
    /*
     * We don't want to get interrupted inside getgrent()
     * because the yellow pages code is not interruptible,
     * and if we call endgrent() immediatetely after
     * (in pintr()) we may be freeing an invalid pointer
     */
    USE(flags);
    USE(dir);
    TW_HOLD();
#if !defined(_VMS_POSIX) && !defined(_OSD_POSIX) && !defined(WINNT_NATIVE)
    gr = (struct group *) getgrent();
#else /* _VMS_POSIX || _OSD_POSIX || WINNT_NATIVE */
    gr = NULL;
#endif /* !_VMS_POSIX && !_OSD_POSIX && !WINNT_NATIVE */
    TW_RELS();

    if (gr == NULL) {
#ifdef YPBUGS
	fix_yp_bugs();
#endif
	return (NULL);
    }
    (void) Strcpy(retname, str2short(gr->gr_name));
    return (retname);
} /* end tw_grpname_next */


/* tw_grpname_end():
 *	Close the group file to finish the groupname list
 */
void
tw_grpname_end()
{
#ifdef YPBUGS
    fix_yp_bugs();
#endif
#if !defined(_VMS_POSIX) && !defined(_OSD_POSIX) && !defined(WINNT_NATIVE)
   (void) endgrent();
#endif /* !_VMS_POSIX && !_OSD_POSIX && !WINNT_NATIVE */
} /* end tw_grpname_end */

/* tw_file_start():
 *	Initialize the directory for the file list
 */
/*ARGSUSED*/
void
tw_file_start(dfd, pat)
    DIR *dfd;
    Char *pat;
{
    struct varent *vp;
    USE(pat);
    SETDIR(dfd)
    if ((vp = adrof(STRcdpath)) != NULL)
	tw_env = vp->vec;
} /* end tw_file_start */


/* tw_file_next():
 *	Return the next file in the directory 
 */
Char *
tw_file_next(dir, flags)
    Char *dir;
    int  *flags;
{
    Char *ptr = tw_dir_next(tw_dir_fd);
    if (ptr == NULL && (*flags & TW_DIR_OK) != 0) {
	CLRDIR(tw_dir_fd)
	while (tw_env && *tw_env)
	    if ((tw_dir_fd = opendir(short2str(*tw_env))) != NULL)
		break;
	    else
		tw_env++;
		
	if (tw_dir_fd) {
	    copyn(dir, *tw_env++, MAXPATHLEN);
	    catn(dir, STRslash, MAXPATHLEN);
	    ptr = tw_dir_next(tw_dir_fd);
	}
    }
    return ptr;
} /* end tw_file_next */


/* tw_dir_end():
 *	Clear directory related lists
 */
void
tw_dir_end()
{
   CLRDIR(tw_dir_fd)
   CLRDIR(tw_cmd_state.dfd)
} /* end tw_dir_end */


/* tw_item_free():
 *	Free the item list
 */
void
tw_item_free()
{
    tw_str_free(&tw_item);
} /* end tw_item_free */


/* tw_item_get(): 
 *	Return the list of items 
 */
Char **
tw_item_get()
{
    return tw_item.list;
} /* end tw_item_get */


/* tw_item_add():
 *	Return a new item
 */
Char *
tw_item_add(len)
    int len;
{
     return tw_str_add(&tw_item, len);
} /* tw_item_add */


/* tw_item_find():
 *      Find the string if it exists in the item list 
 *	end return it.
 */
Char *
tw_item_find(str)
    Char    *str;
{
    int i;

    if (tw_item.list == NULL || str == NULL)
	return NULL;

    for (i = 0; i < tw_item.nlist; i++)
	if (tw_item.list[i] != NULL && Strcmp(tw_item.list[i], str) == 0)
	    return tw_item.list[i];
    return NULL;
} /* end tw_item_find */


/* tw_vl_start():
 *	Initialize a variable list
 */
void
tw_vl_start(dfd, pat)
    DIR *dfd;
    Char *pat;
{
    SETDIR(dfd)
    if ((tw_vptr = adrof(pat)) != NULL) {
	tw_env = tw_vptr->vec;
	tw_vptr = NULL;
    }
    else
	tw_env = NULL;
} /* end tw_vl_start */


/*
 * Initialize a word list
 */
void
tw_wl_start(dfd, pat)
    DIR *dfd;
    Char *pat;
{
    SETDIR(dfd);
    tw_word = pat;
} /* end tw_wl_start */


/*
 * Return the next word from the word list
 */
/*ARGSUSED*/
Char *
tw_wl_next(dir, flags)
    Char *dir;
    int *flags;
{
    USE(flags);
    if (tw_word == NULL || tw_word[0] == '\0')
	return NULL;
    
    while (*tw_word && Isspace(*tw_word)) tw_word++;

    for (dir = tw_word; *tw_word && !Isspace(*tw_word); tw_word++)
	continue;
    if (*tw_word)
	*tw_word++ = '\0';
    return *dir ? dir : NULL;
} /* end tw_wl_next */


/* tw_bind_start():
 *	Begin the list of the shell bindings
 */
/*ARGSUSED*/
void
tw_bind_start(dfd, pat)
    DIR *dfd;
    Char *pat;
{
    USE(pat);
    SETDIR(dfd)
    tw_bind = FuncNames;
} /* end tw_bind_start */


/* tw_bind_next():
 *	Begin the list of the shell bindings
 */
/*ARGSUSED*/
Char *
tw_bind_next(dir, flags)
    Char *dir;
    int *flags;
{
    char *ptr;
    USE(flags);
    if (tw_bind && tw_bind->name) {
	for (ptr = tw_bind->name, dir = tw_retname;
	     (*dir++ = (Char) *ptr++) != '\0';)
	    continue;
	tw_bind++;
	return(tw_retname);
    }
    return NULL;
} /* end tw_bind_next */


/* tw_limit_start():
 *	Begin the list of the shell limitings
 */
/*ARGSUSED*/
void
tw_limit_start(dfd, pat)
    DIR *dfd;
    Char *pat;
{
    USE(pat);
    SETDIR(dfd)
#ifndef HAVENOLIMIT
    tw_limit = limits;
#endif /* ! HAVENOLIMIT */
} /* end tw_limit_start */


/* tw_limit_next():
 *	Begin the list of the shell limitings
 */
/*ARGSUSED*/
Char *
tw_limit_next(dir, flags)
    Char *dir;
    int *flags;
{
#ifndef HAVENOLIMIT
    char *ptr;
    if (tw_limit && tw_limit->limname) {
	for (ptr = tw_limit->limname, dir = tw_retname; 
	     (*dir++ = (Char) *ptr++) != '\0';)
	    continue;
	tw_limit++;
	return(tw_retname);
    }
#endif /* ! HAVENOLIMIT */
    USE(flags);
    return NULL;
} /* end tw_limit_next */


/* tw_sig_start():
 *	Begin the list of the shell sigings
 */
/*ARGSUSED*/
void
tw_sig_start(dfd, pat)
    DIR *dfd;
    Char *pat;
{
    USE(pat);
    SETDIR(dfd)
    tw_index = 0;
} /* end tw_sig_start */


/* tw_sig_next():
 *	Begin the list of the shell sigings
 */
/*ARGSUSED*/
Char *
tw_sig_next(dir, flags)
    Char *dir;
    int *flags;
{
    char *ptr;
    extern int nsig;
    USE(flags);
    for (;tw_index < nsig; tw_index++) {

	if (mesg[tw_index].iname == NULL)
	    continue;

	for (ptr = mesg[tw_index].iname, dir = tw_retname; 
	     (*dir++ = (Char) *ptr++) != '\0';)
	    continue;
	tw_index++;
	return(tw_retname);
    }
    return NULL;
} /* end tw_sig_next */


/* tw_job_start():
 *	Begin the list of the shell jobings
 */
/*ARGSUSED*/
void
tw_job_start(dfd, pat)
    DIR *dfd;
    Char *pat;
{
    USE(pat);
    SETDIR(dfd)
    tw_index = 1;
} /* end tw_job_start */


/* tw_job_next():
 *	Begin the list of the shell jobings
 */
/*ARGSUSED*/
Char *
tw_job_next(dir, flags)
    Char *dir;
    int *flags;
{
    Char *ptr;
    struct process *j;

    USE(flags);
    for (;tw_index <= pmaxindex; tw_index++) {
	for (j = proclist.p_next; j != NULL; j = j->p_next)
	    if (j->p_index == tw_index && j->p_procid == j->p_jobid)
		break;
	if (j == NULL) 
	    continue;
	for (ptr = j->p_command, dir = tw_retname; (*dir++ = *ptr++) != '\0';)
	    continue;
	*dir = '\0';
	tw_index++;
	return(tw_retname);
    }
    return NULL;
} /* end tw_job_next */
