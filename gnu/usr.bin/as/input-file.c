/* input_file.c - Deal with Input Files -
   Copyright (C) 1987, 1990, 1991, 1992 Free Software Foundation, Inc.

   This file is part of GAS, the GNU Assembler.

   GAS is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   GAS is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with GAS; see the file COPYING.  If not, write to
   the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.  */

/*
 * Confines all details of reading source bytes to this module.
 * All O/S specific crocks should live here.
 * What we lose in "efficiency" we gain in modularity.
 * Note we don't need to #include the "as.h" file. No common coupling!
 */

#ifndef lint
static char rcsid[] = "$FreeBSD$";
#endif

#ifdef USG
#define setbuffer(stream, buf, size) setvbuf((stream), (buf), _IOFBF, (size))
#endif

#include <stdio.h>
#include <string.h>

#include "as.h"
#include "input-file.h"

/* This variable is non-zero if the file currently being read should be
   preprocessed by app.  It is zero if the file can be read straight in.
   */
int preprocess = 0;

/*
 * This code opens a file, then delivers BUFFER_SIZE character
 * chunks of the file on demand.
 * BUFFER_SIZE is supposed to be a number chosen for speed.
 * The caller only asks once what BUFFER_SIZE is, and asks before
 * the nature of the input files (if any) is known.
 */

#define BUFFER_SIZE (32 * 1024)

/*
 * We use static data: the data area is not sharable.
 */

FILE *f_in;
/* static JF remove static so app.c can use file_name */
char *	file_name;

/* Struct for saving the state of this module for file includes.  */
struct saved_file {
	FILE *f_in;
	char *file_name;
	int	preprocess;
	char *app_save;
};

/* These hooks accomodate most operating systems. */

void input_file_begin() {
	f_in = (FILE *)0;
}

void input_file_end () { }

/* Return BUFFER_SIZE. */
int input_file_buffer_size() {
	return (BUFFER_SIZE);
}

int input_file_is_open() {
	return f_in != (FILE *)0;
}

/* Push the state of our input, returning a pointer to saved info that
   can be restored with input_file_pop ().  */
char *input_file_push () {
	register struct saved_file *saved;

	saved = (struct saved_file *)xmalloc (sizeof *saved);

	saved->f_in		= f_in;
	saved->file_name	= file_name;
	saved->preprocess	= preprocess;
	if (preprocess)
	    saved->app_save	= app_push ();

	input_file_begin ();	/* Initialize for new file */

	return (char *)saved;
}

void
    input_file_pop (arg)
char *arg;
{
	register struct saved_file *saved = (struct saved_file *)arg;

	input_file_end ();	/* Close out old file */

	f_in			= saved->f_in;
	file_name		= saved->file_name;
	preprocess		= saved->preprocess;
	if (preprocess)
	    app_pop	 	 (saved->app_save);

	free(arg);
}

#ifdef DONTDEF		/* JF save old version in case we need it */
void
    input_file_open (filename, preprocess, debugging)
char *	filename;	/* "" means use stdin. Must not be 0. */
int	preprocess;	/* TRUE if needs app. */
int	debugging;	/* TRUE if we are debugging assembler. */
{
	assert( filename != 0 );	/* Filename may not be NULL. */
	if (filename[0])
	    {				/* We have a file name. Suck it and see. */
		    file_handle = open (filename, O_RDONLY, 0);
		    file_name = filename;
	    }
	else
	    {				/* use stdin for the input file. */
		    file_handle = fileno (stdin);
		    file_name = "{standard input}"; /* For error messages. */
	    }
	if (file_handle < 0)
	    as_perror ("Can't open %s for reading", file_name);
	if ( preprocess )
	    {
		    /*
		     * This code was written in haste for a frobbed BSD 4.2.
		     * I have a flight to catch: will someone please do proper
		     * error checks? - Dean.
		     */
		    int	pid;
		    char temporary_file_name[12];
		    int	fd;
		    union wait	status;

		    (void)strcpy (temporary_file_name, "#appXXXXXX");
		    (void)mktemp (temporary_file_name);
		    pid = vfork ();
		    if (pid == -1)
			{
				as_perror ("Vfork failed", file_name);
				_exit (144);
			}
		    if (pid == 0)
			{
				(void)dup2 (file_handle, fileno(stdin));
				fd = open (temporary_file_name, O_WRONLY + O_TRUNC + O_CREAT, 0666);
				if (fd == -1)
				    {
					    (void)write(2,"Can't open temporary\n",21);
					    _exit (99);
				    }
				(void)dup2 (fd, fileno(stdout));
				/* JF for testing #define PREPROCESSOR "/lib/app" */
#define PREPROCESSOR "./app"
				execl (PREPROCESSOR, PREPROCESSOR, (char *)0);
				execl ("app","app",(char *)0);
				(void)write(2,"Exec of app failed.  Get help.\n",31);
				(void)unlink(temporary_file_name);
				_exit (11);
			}
		    (void)wait (& status);
		    if (status.w_status & 0xFF00)		/* JF was 0xF000, was wrong */
			{
				file_handle = -1;
				as_bad( "Can't preprocess file \"%s\", status = %xx", file_name, status.w_status );
			}
		    else
			{
				file_handle = open (temporary_file_name, O_RDONLY, 0);
				if ( ! debugging && unlink(temporary_file_name))
				    as_perror ("Can't delete temp file %s", temporary_file_name);
			}
		    if (file_handle == -1)
			as_perror ("Can't retrieve temp file %s", temporary_file_name);
	    }
}
#else

void
    input_file_open (filename,pre)
char *	filename;	/* "" means use stdin. Must not be 0. */
int pre;
{
	int	c;
	char	buf[80];

	preprocess = pre;

	assert( filename != 0 );	/* Filename may not be NULL. */
	if (filename[0]) {	/* We have a file name. Suck it and see. */
		f_in=fopen(filename,"r");
		file_name=filename;
	} else {			/* use stdin for the input file. */
		f_in = stdin;
		file_name = "{standard input}"; /* For error messages. */
	}
	if (f_in == (FILE *)0) {
		as_perror ("Can't open %s for reading", file_name);
		return;
	}

#ifndef HO_VMS
	/* Ask stdio to buffer our input at BUFFER_SIZE, with a dynamically
	   allocated buffer.  */
	setvbuf(f_in, (char *)NULL, _IOFBF, BUFFER_SIZE);
#endif /* HO_VMS */

	c = getc(f_in);
	if (c == '#') {	/* Begins with comment, may not want to preprocess */
		c = getc(f_in);
		if (c == 'N') {
			fgets(buf,80,f_in);
			if (!strcmp(buf,"O_APP\n"))
			    preprocess=0;
			if (!strchr(buf,'\n'))
			    ungetc('#',f_in);	/* It was longer */
			else
			    ungetc('\n',f_in);
		} else if (c == '\n')
		    ungetc('\n',f_in);
		else
		    ungetc('#',f_in);
	} else
	    ungetc(c,f_in);

#ifdef DONTDEF
	if ( preprocess ) {
		char temporary_file_name[17];
		FILE	*f_out;

		(void)strcpy (temporary_file_name, "/tmp/#appXXXXXX");
		(void)mktemp (temporary_file_name);
		f_out=fopen(temporary_file_name,"w+");
		if (f_out == (FILE *)0)
		    as_perror("Can't open temp file %s",temporary_file_name);

		/* JF this will have to be moved on any system that
		   does not support removal of open files.  */
		(void)unlink(temporary_file_name);/* JF do it NOW */
		do_scrub(f_in,f_out);
		(void)fclose(f_in);	/* All done with it */
		(void)rewind(f_out);
		f_in=f_out;
	}
#endif
}
#endif

/* Close input file.  */
void input_file_close() {
	if (f_in != NULL) {
		fclose (f_in);
	} /* don't close a null file pointer */
	f_in = 0;
} /* input_file_close() */

char *
    input_file_give_next_buffer (where)
char *		where;	/* Where to place 1st character of new buffer. */
{
	char *	return_value;	/* -> Last char of what we read, + 1. */
	register int	size;

	if (f_in == (FILE *)0)
	    return 0;
	/*
	 * fflush (stdin); could be done here if you want to synchronise
	 * stdin and stdout, for the case where our input file is stdin.
	 * Since the assembler shouldn't do any output to stdout, we
	 * don't bother to synch output and input.
	 */
	if (preprocess) {
		char *p;
		int n;
		int ch;
		extern FILE *scrub_file;

		scrub_file=f_in;
		for (p = where, n = BUFFER_SIZE; n; --n) {

			ch = do_scrub_next_char(scrub_from_file, scrub_to_file);
			if (ch == EOF)
			    break;
			*p++=ch;
		}
		size=BUFFER_SIZE-n;
	} else
	    size= fread(where,sizeof(char),BUFFER_SIZE,f_in);
	if (size < 0)
	    {
		    as_perror ("Can't read from %s", file_name);
		    size = 0;
	    }
	if (size)
	    return_value = where + size;
	else
	    {
		    if (fclose (f_in))
			as_perror ("Can't close %s", file_name);
		    f_in = (FILE *)0;
		    return_value = 0;
	    }
	return (return_value);
}

/* end of input-file.c */
