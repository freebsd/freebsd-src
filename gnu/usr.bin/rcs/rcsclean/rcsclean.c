/* Clean up working files.  */

/* Copyright 1991, 1992, 1993, 1994, 1995 Paul Eggert
   Distributed under license by the Free Software Foundation, Inc.

This file is part of RCS.

RCS is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

RCS is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with RCS; see the file COPYING.
If not, write to the Free Software Foundation,
59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

Report problems and direct all questions to:

	rcs-bugs@cs.purdue.edu

*/

#include "rcsbase.h"

#if has_dirent
	static int get_directory P((char const*,char***));
#endif

static int unlock P((struct hshentry *));
static void cleanup P((void));

static RILE *workptr;
static int exitstatus;

mainProg(rcscleanId, "rcsclean", "$Id: rcsclean.c,v 5.9 1995/06/16 06:19:24 eggert Exp $")
{
	static char const usage[] =
		"\nrcsclean: usage: rcsclean -ksubst -{nqru}[rev] -T -Vn -xsuff -zzone file ...";

	static struct buf revision;

	char *a, **newargv;
	char const *rev, *p;
	int dounlock, expmode, perform, unlocked, unlockflag, waslocked;
	int Ttimeflag;
	struct hshentries *deltas;
	struct hshentry *delta;
	struct stat workstat;

	setrid();

	expmode = -1;
	rev = 0;
	suffixes = X_DEFAULT;
	perform = true;
	unlockflag = false;
	Ttimeflag = false;

	argc = getRCSINIT(argc, argv, &newargv);
	argv = newargv;
	for (;;) {
		if (--argc < 1) {
#			if has_dirent
				argc = get_directory(".", &newargv);
				argv = newargv;
				break;
#			else
				faterror("no pathnames specified");
#			endif
		}
		a = *++argv;
		if (!*a  ||  *a++ != '-')
			break;
		switch (*a++) {
			case 'k':
				if (0 <= expmode)
					redefined('k');
				if ((expmode = str2expmode(a))  <  0)
					goto unknown;
				break;

			case 'n':
				perform = false;
				goto handle_revision;

			case 'q':
				quietflag = true;
				/* fall into */
			case 'r':
			handle_revision:
				if (*a) {
					if (rev)
						warn("redefinition of revision number");
					rev = a;
				}
				break;

			case 'T':
				if (*a)
					goto unknown;
				Ttimeflag = true;
				break;

			case 'u':
				unlockflag = true;
				goto handle_revision;

			case 'V':
				setRCSversion(*argv);
				break;

			case 'x':
				suffixes = a;
				break;

			case 'z':
				zone_set(a);
				break;

			default:
			unknown:
				error("unknown option: %s%s", *argv, usage);
		}
	}

	dounlock = perform & unlockflag;

	if (nerror)
	  cleanup();
	else
	  for (;  0 < argc;  cleanup(), ++argv, --argc) {

		ffree();

		if (!(
			0 < pairnames(
				argc, argv,
				dounlock ? rcswriteopen : rcsreadopen,
				true, true
			) &&
			(workptr = Iopen(workname, FOPEN_R_WORK, &workstat))
		))
			continue;

		if (same_file(RCSstat, workstat, 0)) {
			rcserror("RCS file is the same as working file %s.",
				workname
			);
			continue;
		}

		gettree();

		p = 0;
		if (rev) {
			if (!fexpandsym(rev, &revision, workptr))
				continue;
			p = revision.string;
		} else if (Head)
			switch (unlockflag ? findlock(false,&delta) : 0) {
				default:
					continue;
				case 0:
					p = Dbranch ? Dbranch : "";
					break;
				case 1:
					p = delta->num;
					break;
			}
		delta = 0;
		deltas = 0;  /* Keep lint happy.  */
		if (p  &&  !(delta = genrevs(p,(char*)0,(char*)0,(char*)0,&deltas)))
			continue;

		waslocked = delta && delta->lockedby;
		locker_expansion = unlock(delta);
		unlocked = locker_expansion & unlockflag;
		if (unlocked<waslocked  &&  workstat.st_mode&(S_IWUSR|S_IWGRP|S_IWOTH))
			continue;

		if (unlocked && !checkaccesslist())
			continue;

		if (dorewrite(dounlock, unlocked) != 0)
			continue;

		if (0 <= expmode)
			Expand = expmode;
		else if (
			waslocked  &&
			Expand == KEYVAL_EXPAND  &&
			WORKMODE(RCSstat.st_mode,true) == workstat.st_mode
		)
			Expand = KEYVALLOCK_EXPAND;

		getdesc(false);

		if (
			!delta ? workstat.st_size!=0 :
			0 < rcsfcmp(
				workptr, &workstat,
				buildrevision(deltas, delta, (FILE*)0, false),
				delta
			)
		)
			continue;

		if (quietflag < unlocked)
			aprintf(stdout, "rcs -u%s %s\n", delta->num, RCSname);

		if (perform & unlocked) {
			if_advise_access(deltas->first != delta, finptr, MADV_SEQUENTIAL);
			if (donerewrite(true,
				Ttimeflag ? RCSstat.st_mtime : (time_t)-1
			) != 0)
				continue;
		}

		if (!quietflag)
			aprintf(stdout, "rm -f %s\n", workname);
		Izclose(&workptr);
		if (perform  &&  un_link(workname) != 0)
			eerror(workname);

	  }

	tempunlink();
	if (!quietflag)
		Ofclose(stdout);
	exitmain(exitstatus);
}

	static void
cleanup()
{
	if (nerror) exitstatus = EXIT_FAILURE;
	Izclose(&finptr);
	Izclose(&workptr);
	Ozclose(&fcopy);
	ORCSclose();
	dirtempunlink();
}

#if RCS_lint
#	define exiterr rcscleanExit
#endif
	void
exiterr()
{
	ORCSerror();
	dirtempunlink();
	tempunlink();
	_exit(EXIT_FAILURE);
}

	static int
unlock(delta)
	struct hshentry *delta;
{
	register struct rcslock **al, *l;

	if (delta && delta->lockedby && strcmp(getcaller(),delta->lockedby)==0)
		for (al = &Locks;  (l = *al);  al = &l->nextlock)
			if (l->delta == delta) {
				*al = l->nextlock;
				delta->lockedby = 0;
				return true;
			}
	return false;
}

#if has_dirent
	static int
get_directory(dirname, aargv)
	char const *dirname;
	char ***aargv;
/*
 * Put a vector of all DIRNAME's directory entries names into *AARGV.
 * Ignore names of RCS files.
 * Yield the number of entries found.  Terminate the vector with 0.
 * Allocate the storage for the vector and entry names.
 * Do not sort the names.  Do not include '.' and '..'.
 */
{
	int i, entries = 0, entries_max = 64;
	size_t chars = 0, chars_max = 1024;
	size_t *offset = tnalloc(size_t, entries_max);
	char *a = tnalloc(char, chars_max), **p;
	DIR *d;
	struct dirent *e;

	if (!(d = opendir(dirname)))
		efaterror(dirname);
	while ((errno = 0,  e = readdir(d))) {
		char const *en = e->d_name;
		size_t s = strlen(en) + 1;
		if (en[0]=='.'   &&   (!en[1]  ||  (en[1]=='.' && !en[2])))
			continue;
		if (rcssuffix(en))
			continue;
		while (chars_max < s + chars)
			a = trealloc(char, a, chars_max<<=1);
		if (entries == entries_max)
			offset = trealloc(size_t, offset, entries_max<<=1);
		offset[entries++] = chars;
		VOID strcpy(a+chars, en);
		chars += s;
	}
#	if void_closedir
#		define close_directory(d) (closedir(d), 0)
#	else
#		define close_directory(d) closedir(d)
#	endif
	if (errno  ||  close_directory(d) != 0)
		efaterror(dirname);
	if (chars)
		a = trealloc(char, a, chars);
	else
		tfree(a);
	*aargv = p = tnalloc(char*, entries+1);
	for (i=0; i<entries; i++)
		*p++ = a + offset[i];
	*p = 0;
	tfree(offset);
	return entries;
}
#endif
