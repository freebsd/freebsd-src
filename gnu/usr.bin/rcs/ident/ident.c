/* Copyright (C) 1982, 1988, 1989 Walter Tichy
   Copyright 1990, 1991 by Paul Eggert
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
along with RCS; see the file COPYING.  If not, write to
the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.

Report problems and direct all questions to:

    rcs-bugs@cs.purdue.edu

*/

/*
 *                     RCS identification operation
 */

/* $Log: ident.c,v $
 * Revision 5.3  1991/09/10  22:15:46  eggert
 * Open files with FOPEN_R, not FOPEN_R_WORK,
 * because they might be executables, not working files.
 *
 * Revision 5.2  1991/08/19  03:13:55  eggert
 * Report read errors immediately.
 *
 * Revision 5.1  1991/02/25  07:12:37  eggert
 * Don't report empty keywords.  Check for I/O errors.
 *
 * Revision 5.0  1990/08/22  08:12:37  eggert
 * Don't limit output to known keywords.
 * Remove arbitrary limits and lint.  Ansify and Posixate.
 *
 * Revision 4.5  89/05/01  15:11:54  narten
 * changed copyright header to reflect current distribution rules
 * 
 * Revision 4.4  87/10/23  17:09:57  narten
 * added exit(0) so exit return code would be non random
 * 
 * Revision 4.3  87/10/18  10:23:55  narten
 * Updating version numbers. Changes relative to 1.1 are actually relative
 * to 4.1
 * 
 * Revision 1.3  87/07/09  09:20:52  trinkle
 * Added check to make sure there is at least one arg before comparing argv[1]
 * with "-q".  This necessary on machines that don't allow dereferncing null
 * pointers (i.e. Suns).
 * 
 * Revision 1.2  87/03/27  14:21:47  jenkins
 * Port to suns
 * 
 * Revision 4.1  83/05/10  16:31:02  wft
 * Added option -q and input from reading stdin.
 * Marker matching is now done with trymatch() (independent of keywords).
 * 
 * Revision 3.4  83/02/18  17:37:49  wft
 * removed printing of new line after last file.
 *
 * Revision 3.3  82/12/04  12:48:55  wft
 * Added LOCKER.
 *
 * Revision 3.2  82/11/28  18:24:17  wft
 * removed Suffix; added ungetc to avoid skipping over trailing KDELIM.
 *
 * Revision 3.1  82/10/13  15:58:51  wft
 * fixed type of variables receiving from getc() (char-->int).
*/

#include  "rcsbase.h"

static int match P((FILE*));
static void scanfile P((FILE*,char const*,int));

mainProg(identId, "ident", "$Id: ident.c,v 5.3 1991/09/10 22:15:46 eggert Exp $")
/*  Ident searches the named files for all occurrences
 *  of the pattern $keyword:...$, where the keywords are
 *  Author, Date, Header, Id, Log, RCSfile, Revision, Source, and State.
 */

{
   FILE *fp;
   int quiet;
   int status = EXIT_SUCCESS;

   if ((quiet  =  argc > 1 && strcmp("-q",argv[1])==0)) {
        argc--; argv++;
   }

   if (argc<2)
	scanfile(stdin, (char*)0, quiet);

   while ( --argc > 0 ) {
      if (!(fp = fopen(*++argv, FOPEN_R))) {
	 VOID fprintf(stderr,  "%s error: can't open %s\n", cmdid, *argv);
	 status = EXIT_FAILURE;
      } else {
	 scanfile(fp, *argv, quiet);
	 if (argc>1) VOID putchar('\n');
      }
   }
   if (ferror(stdout) || fclose(stdout)!=0) {
      VOID fprintf(stderr,  "%s error: write error\n", cmdid);
      status = EXIT_FAILURE;
   }
   exitmain(status);
}

#if lint
	exiting void identExit() { _exit(EXIT_FAILURE); }
#endif


	static void
scanfile(file, name, quiet)
	register FILE *file;
	char const *name;
	int quiet;
/* Function: scan an open file with descriptor file for keywords.
 * Return false if there's a read error.
 */
{
   register int c;

   if (name)
      VOID printf("%s:\n", name);
   else
      name = "input";
   c = 0;
   for (;;) {
      if (c < 0) {
	 if (feof(file))
	    break;
	 if (ferror(file))
	    goto read_error;
      }
      if (c == KDELIM) {
	 if ((c = match(file)))
	    continue;
	 quiet = true;
      }
      c = getc(file);
   }
   if (!quiet)
      VOID fprintf(stderr, "%s warning: no id keywords in %s\n", cmdid, name);
   if (fclose(file) == 0)
      return;

 read_error:
   VOID fprintf(stderr, "%s error: %s: read error\n", cmdid, name);
   exit(EXIT_FAILURE);
}



	static int
match(fp)   /* group substring between two KDELIM's; then do pattern match */
   register FILE *fp;
{
   char line[BUFSIZ];
   register int c;
   register char * tp;

   tp = line;
   while ((c = getc(fp)) != VDELIM) {
      if (c < 0)
	 return c;
      switch (ctab[c]) {
	 case LETTER: case Letter:
	    *tp++ = c;
	    if (tp < line+sizeof(line)-4)
	       break;
	    /* fall into */
	 default:
	    return c ? c : '\n'/* anything but 0 or KDELIM or EOF */;
      }
   }
   if (tp == line)
      return c;
   *tp++ = c;
   if ((c = getc(fp)) != ' ')
      return c ? c : '\n';
   *tp++ = c;
   while( (c = getc(fp)) != KDELIM ) {
      if (c < 0  &&  feof(fp) | ferror(fp))
	    return c;
      switch (ctab[c]) {
	 default:
	    *tp++ = c;
	    if (tp < line+sizeof(line)-2)
	       break;
	    /* fall into */
	 case NEWLN: case UNKN:
	    return c ? c : '\n';
      }
   }
   if (tp[-1] != ' ')
      return c;
   *tp++ = c;     /*append trailing KDELIM*/
   *tp   = '\0';
   VOID fprintf(stdout, "     %c%s\n", KDELIM, line);
   return 0;
}
