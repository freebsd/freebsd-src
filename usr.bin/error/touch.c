/*
 * Copyright (c) 1980, 1993
 *	The Regents of the University of California.  All rights reserved.
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
#if 0
static char sccsid[] = "@(#)touch.c	8.1 (Berkeley) 6/6/93";
#endif
static const char rcsid[] =
	"$Id$";
#endif /* not lint */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <ctype.h>
#include <err.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "error.h"
#include "pathnames.h"

/*
 *	Iterate through errors
 */
#define EITERATE(p, fv, i)	for (p = fv[i]; p < fv[i+1]; p++)
#define	ECITERATE(ei, p, lb)	for (ei = lb; p = errors[ei],ei < nerrors; ei++)

#define	FILEITERATE(fi, lb)	for (fi = lb; fi <= nfiles; fi++)
int	touchstatus = Q_YES;

int countfiles __P((Eptr *));
boolean edit __P((char *));
void errorprint __P((FILE *, Eptr, boolean));
void execvarg __P((int, int *, char ***));
void diverterrors __P((char *, int, Eptr **, int, boolean, int));
void hackfile __P((char *, Eptr **, int, int));
void insert __P((int));
int mustoverwrite __P((FILE *, FILE *));
int mustwrite __P((char *, int, FILE *));
int nopertain __P((Eptr **));
int oktotouch __P((char *));
boolean preview __P((char *, int, Eptr **, int));
int settotouch __P((char *));
void text __P((Eptr, boolean));
boolean writetouched __P((int));

void
findfiles(nerrors, errors, r_nfiles, r_files)
		int	nerrors;
	Eptr	*errors;
		int	*r_nfiles;
	Eptr	***r_files;
{
		int	nfiles;
	Eptr	**files;

		char	*name;
	reg	int	ei;
		int	fi;
	reg	Eptr	errorp;

	nfiles = countfiles(errors);

	files = (Eptr**)Calloc(nfiles + 3, sizeof (Eptr*));
	touchedfiles = (boolean	*)Calloc(nfiles+3, sizeof(boolean));
	/*
	 *	Now, partition off the error messages
	 *	into those that are synchronization, discarded or
	 *	not specific to any file, and those that were
	 *	nulled or true errors.
	 */
	files[0] = &errors[0];
	ECITERATE(ei, errorp, 0){
		if ( ! (NOTSORTABLE(errorp->error_e_class)))
			break;
	}
	/*
	 *	Now, and partition off all error messages
	 *	for a given file.
	 */
	files[1] = &errors[ei];
	touchedfiles[0] = touchedfiles[1] = FALSE;
	name = "\1";
	fi = 1;
	ECITERATE(ei, errorp, ei){
		if (   (errorp->error_e_class == C_NULLED)
		    || (errorp->error_e_class == C_TRUE) ){
			if (strcmp(errorp->error_text[0], name) != 0){
				name = errorp->error_text[0];
				touchedfiles[fi] = FALSE;
				files[fi] = &errors[ei];
				fi++;
			}
		}
	}
	files[fi] = &errors[nerrors];
	*r_nfiles = nfiles;
	*r_files = files;
}

int countfiles(errors)
	Eptr	*errors;
{
	char	*name;
	int	ei;
	reg	Eptr	errorp;

	int	nfiles;
	nfiles = 0;
	name = "\1";
	ECITERATE(ei, errorp, 0){
		if (SORTABLE(errorp->error_e_class)){
			if (strcmp(errorp->error_text[0],name) != 0){
				nfiles++;
				name = errorp->error_text[0];
			}
		}
	}
	return(nfiles);
}
char	*class_table[] = {
	/*C_UNKNOWN	0	*/	"Unknown",
	/*C_IGNORE	1	*/	"ignore",
	/*C_SYNC	2	*/	"synchronization",
	/*C_DISCARD	3	*/	"discarded",
	/*C_NONSPEC	4	*/	"non specific",
	/*C_THISFILE	5	*/	"specific to this file",
	/*C_NULLED	6	*/	"nulled",
	/*C_TRUE	7	*/	"true",
	/*C_DUPL	8	*/	"duplicated"
};

int	class_count[C_LAST - C_FIRST] = {0};

void
filenames(nfiles, files)
	int	nfiles;
	Eptr	**files;
{
	reg	int	fi;
		char	*sep = " ";
	extern	char	*class_table[];
		int	someerrors;

	/*
	 *	first, simply dump out errors that
	 *	don't pertain to any file
	 */
	someerrors = nopertain(files);

	if (nfiles){
		someerrors++;
		fprintf(stdout, terse
			? "%d file%s"
			: "%d file%s contain%s errors",
			nfiles, plural(nfiles), verbform(nfiles));
		if (!terse){
			FILEITERATE(fi, 1){
				fprintf(stdout, "%s\"%s\" (%d)",
					sep, (*files[fi])->error_text[0],
					files[fi+1] - files[fi]);
				sep = ", ";
			}
		}
		fprintf(stdout, "\n");
	}
	if (!someerrors)
		fprintf(stdout, "No errors.\n");
}

/*
 *	Dump out errors that don't pertain to any file
 */
int nopertain(files)
	Eptr	**files;
{
	int	type;
	int	someerrors = 0;
	reg	Eptr	*erpp;
	reg	Eptr	errorp;

	if (files[1] - files[0] <= 0)
		return(0);
	for(type = C_UNKNOWN; NOTSORTABLE(type); type++){
		if (class_count[type] <= 0)
			continue;
		if (type > C_SYNC)
			someerrors++;
		if (terse){
			fprintf(stdout, "\t%d %s errors NOT PRINTED\n",
				class_count[type], class_table[type]);
		} else {
			fprintf(stdout, "\n\t%d %s errors follow\n",
				class_count[type], class_table[type]);
			EITERATE(erpp, files, 0){
				errorp = *erpp;
				if (errorp->error_e_class == type){
					errorprint(stdout, errorp, TRUE);
				}
			}
		}
	}
	return(someerrors);
}

extern	boolean	notouch;

boolean touchfiles(nfiles, files, r_edargc, r_edargv)
	int	nfiles;
	Eptr	**files;
	int	*r_edargc;
	char	***r_edargv;
{
		char	*name;
	reg	Eptr	errorp;
	reg	int	fi;
	reg	Eptr	*erpp;
		int		ntrueerrors;
		boolean		scribbled;
		int		n_pissed_on;	/* # of file touched*/
		int	spread;

	FILEITERATE(fi, 1){
		name = (*files[fi])->error_text[0];
		spread = files[fi+1] - files[fi];
		fprintf(stdout, terse
			? "\"%s\" has %d error%s, "
			: "\nFile \"%s\" has %d error%s.\n"
			, name ,spread ,plural(spread));
		/*
		 *	First, iterate through all error messages in this file
		 *	to see how many of the error messages really will
		 *	get inserted into the file.
		 */
		ntrueerrors = 0;
		EITERATE(erpp, files, fi){
			errorp = *erpp;
			if (errorp->error_e_class == C_TRUE)
				ntrueerrors++;
		}
		fprintf(stdout, terse
		  ? "insert %d\n"
		  : "\t%d of these errors can be inserted into the file.\n",
			ntrueerrors);

		hackfile(name, files, fi, ntrueerrors);
	}
	scribbled = FALSE;
	n_pissed_on = 0;
	FILEITERATE(fi, 1){
		scribbled |= touchedfiles[fi];
		n_pissed_on++;
	}
	if (scribbled){
		/*
		 *	Construct an execv argument
		 */
		execvarg(n_pissed_on, r_edargc, r_edargv);
		return(TRUE);
	} else {
		if (!terse)
			fprintf(stdout, "You didn't touch any files.\n");
		return(FALSE);
	}
}

void
hackfile(name, files, ix, nerrors)
	char	*name;
	Eptr	**files;
	int	ix;
{
	boolean	previewed;
	int	errordest;	/* where errors go */

	if (!oktotouch(name)) {
		previewed = FALSE;
		errordest = TOSTDOUT;
	} else {
		previewed = preview(name, nerrors, files, ix);
		errordest = settotouch(name);
	}

	if (errordest != TOSTDOUT)
		touchedfiles[ix] = TRUE;

	if (previewed && (errordest == TOSTDOUT))
		return;

	diverterrors(name, errordest, files, ix, previewed, nerrors);

	if (errordest == TOTHEFILE){
		/*
		 *	overwrite the original file
		 */
		writetouched(1);
	}
}

boolean preview(name, nerrors, files, ix)
	char	*name;
	int	nerrors;
	Eptr	**files;
	int	ix;
{
	int	back;
	reg	Eptr	*erpp;

	if (nerrors <= 0)
		return(FALSE);
	back = FALSE;
	if(query){
		switch(inquire(terse
		    ? "Preview? "
		    : "Do you want to preview the errors first? ")){
		case Q_YES:
		case Q_yes:
			back = TRUE;
			EITERATE(erpp, files, ix){
				errorprint(stdout, *erpp, TRUE);
			}
			if (!terse)
				fprintf(stdout, "\n");
		default:
			break;
		}
	}
	return(back);
}

int settotouch(name)
	char	*name;
{
	int	dest = TOSTDOUT;

	if (query){
		switch(touchstatus = inquire(terse
			? "Touch? "
			: "Do you want to touch file \"%s\"? ",
			name)){
		case Q_NO:
		case Q_no:
			return(dest);
		default:
			break;
		}
	}

	switch(probethisfile(name)){
	case F_NOTREAD:
		dest = TOSTDOUT;
		fprintf(stdout, terse
			? "\"%s\" unreadable\n"
			: "File \"%s\" is unreadable\n",
			name);
		break;
	case F_NOTWRITE:
		dest = TOSTDOUT;
		fprintf(stdout, terse
			? "\"%s\" unwritable\n"
			: "File \"%s\" is unwritable\n",
			name);
		break;
	case F_NOTEXIST:
		dest = TOSTDOUT;
		fprintf(stdout, terse
			? "\"%s\" not found\n"
			: "Can't find file \"%s\" to insert error messages into.\n",
			name);
		break;
	default:
		dest = edit(name) ? TOSTDOUT : TOTHEFILE;
		break;
	}
	return(dest);
}

void
diverterrors(name, dest, files, ix, previewed, nterrors)
	char	*name;
	int	dest;
	Eptr	**files;
	int	ix;
	boolean	previewed;
	int	nterrors;
{
	int	nerrors;
	reg	Eptr	*erpp;
	reg	Eptr	errorp;

	nerrors = files[ix+1] - files[ix];

	if (   (nerrors != nterrors)
	    && (!previewed) ){
		fprintf(stdout, terse
			? "Uninserted errors\n"
			: ">>Uninserted errors for file \"%s\" follow.\n",
			name);
	}

	EITERATE(erpp, files, ix){
		errorp = *erpp;
		if (errorp->error_e_class != C_TRUE){
			if (previewed || touchstatus == Q_NO)
				continue;
			errorprint(stdout, errorp, TRUE);
			continue;
		}
		switch (dest){
		case TOSTDOUT:
			if (previewed || touchstatus == Q_NO)
				continue;
			errorprint(stdout,errorp, TRUE);
			break;
		case TOTHEFILE:
			insert(errorp->error_line);
			text(errorp, FALSE);
			break;
		}
	}
}

int oktotouch(filename)
	char	*filename;
{
	extern		char	*suffixlist;
	reg	char	*src;
	reg	char	*pat;
			char	*osrc;

	pat = suffixlist;
	if (pat == 0)
		return(0);
	if (*pat == '*')
		return(1);
	while (*pat++ != '.')
		continue;
	--pat;		/* point to the period */

	for (src = &filename[strlen(filename)], --src;
	     (src > filename) && (*src != '.'); --src)
		continue;
	if (*src != '.')
		return(0);

	for (src++, pat++, osrc = src; *src && *pat; src = osrc, pat++){
		for (;   *src			/* not at end of the source */
		      && *pat			/* not off end of pattern */
		      && *pat != '.'		/* not off end of sub pattern */
		      && *pat != '*'		/* not wild card */
		      && *src == *pat;		/* and equal... */
		      src++, pat++)
			continue;
		if (*src == 0 && (*pat == 0 || *pat == '.' || *pat == '*'))
			return(1);
		if (*src != 0 && *pat == '*')
			return(1);
		while (*pat && *pat != '.')
			pat++;
		if (! *pat)
			return(0);
	}
	return(0);
}
/*
 *	Construct an execv argument
 *	We need 1 argument for the editor's name
 *	We need 1 argument for the initial search string
 *	We need n_pissed_on arguments for the file names
 *	We need 1 argument that is a null for execv.
 *	The caller fills in the editor's name.
 *	We fill in the initial search string.
 *	We fill in the arguments, and the null.
 */
void
execvarg(n_pissed_on, r_argc, r_argv)
	int	n_pissed_on;
	int	*r_argc;
	char	***r_argv;
{
	Eptr	p;
	char	*sep;
	int	fi;

	(*r_argv) = (char **)Calloc(n_pissed_on + 3, sizeof(char *));
	(*r_argc) =  n_pissed_on + 2;
	(*r_argv)[1] = "+1;/###/";
	n_pissed_on = 2;
	if (!terse){
		fprintf(stdout, "You touched file(s):");
		sep = " ";
	}
	FILEITERATE(fi, 1){
		if (!touchedfiles[fi])
			continue;
		p = *(files[fi]);
		if (!terse){
			fprintf(stdout,"%s\"%s\"", sep, p->error_text[0]);
			sep = ", ";
		}
		(*r_argv)[n_pissed_on++] = p->error_text[0];
	}
	if (!terse)
		fprintf(stdout, "\n");
	(*r_argv)[n_pissed_on] = 0;
}

FILE	*o_touchedfile;	/* the old file */
FILE	*n_touchedfile;	/* the new file */
char	*o_name;
char	n_name[MAXPATHLEN];
char	*canon_name = _PATH_TMP;
int	o_lineno;
int	n_lineno;
boolean	tempfileopen = FALSE;
/*
 *	open the file; guaranteed to be both readable and writable
 *	Well, if it isn't, then return TRUE if something failed
 */
boolean edit(name)
	char	*name;
{
	int fd;
	
	o_name = name;
	if ( (o_touchedfile = fopen(name, "r")) == NULL){
		warnx("can't open file \"%s\" to touch (read)", name);
		return(TRUE);
	}
	(void)strcpy(n_name, canon_name);
	(void)strcat(n_name,"error.XXXXXX");
	fd = mkstemp(n_name);
	if ( fd < 0 || (n_touchedfile = fdopen(fd, "w")) == NULL) {
		if (fd >= 0)
			close(fd);
		warnx("can't open file \"%s\" to touch (write)", name);
		return(TRUE);
	}
	tempfileopen = TRUE;
	n_lineno = 0;
	o_lineno = 0;
	return(FALSE);
}
/*
 *	Position to the line (before, after) the line given by place
 */
char	edbuf[BUFSIZ];

void
insert(place)
	int	place;
{
	--place;	/* always insert messages before the offending line*/
	for(; o_lineno < place; o_lineno++, n_lineno++){
		if(fgets(edbuf, BUFSIZ, o_touchedfile) == NULL)
			return;
		fputs(edbuf, n_touchedfile);
	}
}

void
text(p, use_all)
	reg	Eptr	p;
		boolean	use_all;
{
	int	offset = use_all ? 0 : 2;

	fputs(lang_table[p->error_language].lang_incomment, n_touchedfile);
	fprintf(n_touchedfile, "%d [%s] ",
		p->error_line,
		lang_table[p->error_language].lang_name);
	wordvprint(n_touchedfile, p->error_lgtext-offset, p->error_text+offset);
	fputs(lang_table[p->error_language].lang_outcomment,n_touchedfile);
	n_lineno++;
}

/*
 *	write the touched file to its temporary copy,
 *	then bring the temporary in over the local file
 */
boolean
writetouched(overwrite)
	int	overwrite;
{
	reg	int	nread;
	reg	FILE	*localfile;
	reg	FILE	*tmpfile;
		int	botch;
		int	oktorm;

	botch = 0;
	oktorm = 1;
	while((nread = fread(edbuf, 1, sizeof(edbuf), o_touchedfile)) != 0) {
		if (nread != fwrite(edbuf, 1, nread, n_touchedfile)){
			/*
			 *	Catastrophe in temporary area: file system full?
			 */
			botch = 1;
			warnx("write failure: no errors inserted in \"%s\"",
			      o_name);
		}
	}
	fclose(n_touchedfile);
	fclose(o_touchedfile);
	/*
	 *	Now, copy the temp file back over the original
	 *	file, thus preserving links, etc
	 */
	if (botch == 0 && overwrite){
		botch = 0;
		localfile = NULL;
		tmpfile = NULL;
		if ((localfile = fopen(o_name, "w")) == NULL){
			warnx("can't open file \"%s\" to overwrite", o_name);
			botch++;
		}
		if ((tmpfile = fopen(n_name, "r")) == NULL){
			warnx("can't open file \"%s\" to read", n_name);
			botch++;
		}
		if (!botch)
			oktorm = mustoverwrite(localfile, tmpfile);
		if (localfile != NULL)
			fclose(localfile);
		if (tmpfile != NULL)
			fclose(tmpfile);
	}
	if (oktorm == 0)
		errx(1, "catastrophe: a copy of \"%s\" was saved in \"%s\"",
			o_name, n_name);
	/*
	 *	Kiss the temp file good bye
	 */
	unlink(n_name);
	tempfileopen = FALSE;
	return(TRUE);
}
/*
 *	return 1 if the tmpfile can be removed after writing it out
 */
int
mustoverwrite(preciousfile, tmpfile)
	FILE	*preciousfile;
	FILE	*tmpfile;
{
	int	nread;

	while((nread = fread(edbuf, 1, sizeof(edbuf), tmpfile)) != 0) {
		if (mustwrite(edbuf, nread, preciousfile) == 0)
			return(0);
	}
	return(1);
}
/*
 *	return 0 on catastrophe
 */
int
mustwrite(base, n, preciousfile)
	char	*base;
	int	n;
	FILE	*preciousfile;
{
	int	nwrote;

	if (n <= 0)
		return(1);
	nwrote = fwrite(base, 1, n, preciousfile);
	if (nwrote == n)
		return(1);
	warn("fwrite");
	switch(inquire(terse
	    ? "Botch overwriting: retry? "
	    : "Botch overwriting the source file: retry? ")){
	case Q_YES:
	case Q_yes:
		mustwrite(base + nwrote, n - nwrote, preciousfile);
		return(1);
	case Q_NO:
	case Q_no:
		switch(inquire("Are you sure? ")){
		case Q_YES:
		case Q_yes:
			return(0);
		case Q_NO:
		case Q_no:
			mustwrite(base + nwrote, n - nwrote, preciousfile);
			return(1);
		}
	default:
		return(0);
	}
}

void
onintr()
{
	switch(inquire(terse
	    ? "\nContinue? "
	    : "\nInterrupt: Do you want to continue? ")){
	case Q_YES:
	case Q_yes:
		signal(SIGINT, onintr);
		return;
	default:
		if (tempfileopen){
			/*
			 *	Don't overwrite the original file!
			 */
			writetouched(0);
		}
		exit(1);
	}
	/*NOTREACHED*/
}

void
errorprint(place, errorp, print_all)
	FILE	*place;
	Eptr	errorp;
	boolean	print_all;
{
	int	offset = print_all ? 0 : 2;

	if (errorp->error_e_class == C_IGNORE)
		return;
	fprintf(place, "[%s] ", lang_table[errorp->error_language].lang_name);
	wordvprint(place,errorp->error_lgtext-offset,errorp->error_text+offset);
	putc('\n', place);
}

int inquire(fmt, a1, a2)
	char	*fmt;
	/*VARARGS1*/
{
	char	buffer[128];

	if (queryfile == NULL)
		return(0);
	for(;;){
		do{
			fflush(stdout);
			fprintf(stderr, fmt, a1, a2);
			fflush(stderr);
		} while (fgets(buffer, 127, queryfile) == NULL);
		switch(buffer[0]){
		case 'Y':	return(Q_YES);
		case 'y':	return(Q_yes);
		case 'N':	return(Q_NO);
		case 'n':	return(Q_no);
		default:	fprintf(stderr, "Yes or No only!\n");
		}
	}
}

int probethisfile(name)
	char	*name;
{
	struct stat statbuf;
	if (stat(name, &statbuf) < 0)
		return(F_NOTEXIST);
	if((statbuf.st_mode & S_IREAD) == 0)
		return(F_NOTREAD);
	if((statbuf.st_mode & S_IWRITE) == 0)
		return(F_NOTWRITE);
	return(F_TOUCHIT);
}
