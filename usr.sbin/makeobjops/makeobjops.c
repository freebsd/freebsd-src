/*
 * Copyright (c) 1992, 1993
 *        The Regents of the University of California.  All rights reserved.
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
 *        This product includes software developed by the University of
 *        California, Berkeley and its contributors.
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
 *
 * From @(#)vnode_if.sh        8.1 (Berkeley) 6/10/93
 * From @(#)makedevops.sh 1.1 1998/06/14 13:53:12 dfr Exp $
 * From @(#)makedevops.sh ?.? 1998/10/05
 * From src/sys/kern/makedevops.pl,v 1.12 1999/11/22 14:40:04 n_hibma Exp
 * From FreeBSD: src/sys/kern/makeobjops.pl,v 1.2.2.1 2001/02/02 19:49:13 cg Exp
 *	$Id: makeobjops.c,v 1.3 2001/10/10 21:22:41 db Exp $
 */

/*
 *
 * Script to produce kobj front-end sugar.
 *
 */
/*
 * My personal preference would have been to use yacc/lex etc.
 * However, this is part of the core when we don't even have yacc/lex yet..
 * So, a simple recursive descent it is..
 * -db
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/stat.h>

#include <ctype.h>
#include <err.h>
#include <fcntl.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void process_files(int argc, char *argv[], char *progname);
static void print_headers(char *src, FILE *src_fp, FILE *hfile_fp,
		char *prog_name);
static void process_body(char *src, FILE *src_fp, FILE *cfile_fp,
		FILE *hfile_fp, int line_count);
static char *upper_case(char *name);
static char *add_ext(char *prefix, char *suffix);
static void usage(char *progname);
static char *strip_ext(char *s, char *ext);
static void emit_code_section(char *src, FILE *src_fp, FILE *cfile_fp);
static void emit_c_body(char *src, FILE *src_fp, FILE *cfile_fp, char *mname,
		char *dname);
static void emit_h_body(char *src, FILE *hfile_fp, char *mtype,
		char *mname, int max_list);
static int parse_method(char *src,
		FILE *src_fp, FILE *cfile_fp, FILE *hfile_fp,
		char *input_buffer, int line_count);
static void make_copy(char *fin, char *fout);
static char* first_token(char *s);
static char* next_token(char *s);
static char* trim_name(char *name);
static char* make_strdup(char *s);
static char* make_malloc(int size);

#define	MAXLIST 32

struct type_name {
	int	deref;
	char	*mtype;
	char	*mname;
};

struct type_name type_name_list[MAXLIST];

int debug = 0;
int cfile = 0;          /* by default do not produce any file type */
int hfile = 0;

int keepcurrentdir = 1;
int line_width = 80;

#define	MAXLINE 128

/* Process the command line */
int
main(int argc,char *argv[])
{
	char *progname;
	int ch;
  
	progname = argv[0];

	/* Process the command line */

	opterr = 0;

	while ((ch = getopt(argc, argv, "chdpl:")) != -1) {
	      switch(ch) {

		case 'c':
		if(debug)
			fprintf(stderr, "Producing .c output files\n");
		cfile = 1;
		break;

		case 'h':
		if(debug)
			fprintf(stderr, "Producing .h output files\n");
		hfile = 1;
		break;
        
		case 'd':
		debug = 1;
		break;
  
		case 'p':
		if(debug)
			fprintf(stderr,
		   "Will produce files in original not in current directory\n");
		keepcurrentdir = 0;
		break;          

		case 'l':
		line_width = atoi(optarg);
		break;
  
		case '?':
		default:
		usage(progname);
		break;
		}
	}

	argc -= optind;
	argv += optind;


	if (!cfile && !hfile) {
		usage(progname);
	}

	process_files(argc, argv, progname);
	return (0);
}

/*
 * usage
 *
 * inputs	- program name
 * output	- none
 * side effects	- prints usage summary then exits.
 */
static void
usage(char *progname)
{
/* Validate the command line parameters */

	fprintf(stderr,
	    "usage: %s [-d] [-p] [-l <nr>] [-c|-h] srcfile\n", progname);
	fprintf(stderr,
	    "where -c  produce only .c files\n");
	fprintf(stderr,
	    "      -h  produce only .h files\n");
	fprintf(stderr,
   "      -p  use the path component in the source file for destination dir\n");
	fprintf(stderr,
	    "      -l  set line width for output files [80]\n");
	fprintf(stderr,
	    "      -d  switch on debugging\n");
	exit(0);
}

/*
 * find_tmp
 *
 * inputs	- none
 * output	- pointer to valid tmp dir
 * side effects	- exits if none found
 */
static char *
find_tmp(void)
{
	static char *tmpdir;
	struct stat dstat;

	if ((tmpdir = getenv("TMPDIR")) != NULL)
		return (tmpdir);

	if ((tmpdir = getenv("TMP")) != NULL)
		return (tmpdir);

	if ((tmpdir = getenv("TEMP")) != NULL)
		return (tmpdir);

	tmpdir = "/tmp";
	if (stat (tmpdir, &dstat) >= 0)
		return (tmpdir);

	tmpdir = P_tmpdir;
	if (stat (tmpdir, &dstat) >= 0)
		return (tmpdir);

	tmpdir = ".";
	return (tmpdir);
}

/*
 * mk_tmp
 *
 * inputs	- pointer to tmp dir to use
 *		- pointer to basename
 *		- pointer to temp ext to use
 * output	- pointer to tmp file, tmp file name is formed
 *		- from dirname/basename.ext{pid}
 * side effects	- caller is responsible for freeing memory
 */
static char *
mk_tmp(char *tmpdir, char *name, char *ext)
{
	char *tmpstr;

	asprintf (&tmpstr, "%s/%s.%s%d", tmpdir, name, ext, getuid());
	return (tmpstr);
}

/*
 * process_files
 *
 * inputs	- file count
 *		- pointer to (assumed) list of filenames
 *		- given program name
 * output	- none
 * side effects	- given files are processed
 */
static void
process_files(int argc, char *argv[], char *progname)
{
	char *tmpdir;
	char *ctmpname;
	char *htmpname;
	char *cname;
	char *hname;
	char *src;		/* actual src file name */
	char *src_base;		/* file name minus any extension */
	char *src_path;
	FILE *cfile_fp, *hfile_fp, *src_fp;
	int i, line_count;

	line_count = 0;

	if ((tmpdir = find_tmp()) == NULL)
		err(0, "Cannot find a tmp dir");

	for (i = 0; i < argc; i++) {

		if ((src_base = basename(argv[i])) == NULL)
			err(0, "can't find basename(%s)", argv[i]);

		src_base = strip_ext(src_base, ".m");

		if ((src_path = dirname(argv[i])) == NULL)
			err(0, "can't find dirname(%s)", argv[i]);
			
		ctmpname = mk_tmp(tmpdir, src_base, "ctmp");
		htmpname = mk_tmp(tmpdir, src_base, "htmp");

		/* The makefile wasn't clear... bah
		 * accept both file name with .m or without
		 */

		if (strstr(argv[i], ".m"))
			src = make_strdup(argv[i]);
		else
			src = add_ext(argv[i], "m");

		cname = add_ext(src_base, "c");
		hname = add_ext(src_base, "h");

		if (cfile) {
			if ((cfile_fp = fopen(ctmpname, "w")) == NULL)
				err(0, "Could not open %s", cname);
		}

		if (hfile) {
			if ((hfile_fp = fopen(htmpname, "w")) == NULL)
				err(0, "Could not open %s", cname);
		}

		if ((src_fp = fopen(src, "r")) == NULL)
			err(0, "Could not open %s", src);

		print_headers(src, cfile_fp, hfile_fp, progname);
		
		if (hfile) {
			fprintf(hfile_fp, "#ifndef _%s_h_\n", src_base);
			fprintf(hfile_fp, "#define _%s_h_\n\n", src_base);
		}

		process_body(src, src_fp, cfile_fp, hfile_fp, line_count);

		if (hfile) {
			fprintf(hfile_fp, "\n#endif /* _%s_h_ */\n", src_base);
		}

		if (cfile)
			fclose(cfile_fp);
		if (hfile)
			fclose(hfile_fp);
		fclose(src_fp);

		/* copy files generated into position */

/* XXX */
		if (!keepcurrentdir)
			if (chdir(src_path) < 0)
				err(0, "can't chdir to %s", src_path);
		if (cfile) {
			make_copy(ctmpname, cname);
			(void)unlink(ctmpname);
		}

		if (hfile) {
			make_copy(htmpname, hname);
			(void)unlink(htmpname);
		}

		free(ctmpname);
		free(htmpname);
		free(cname);
		free(hname);
		free(src);
	}
}

/*
 * add_ext 
 *
 * inputs	- pointer to prefix
 *		- pointer to suffix
 * output	- pointer to name as prefix.suffix
 * side effects	- caller is responsible for freeing memory
 */
static char *
add_ext(char *prefix, char *suffix)
{
	char *tmpstr;

	asprintf (&tmpstr, "%s.%s", prefix, suffix);
	return (tmpstr);
}

/*
 * make_strdup
 *
 * inputs	- pointer to string to duplicate
 * output	- pointer duplicated string
 * side effects	- exits if unable to malloc
 */
static char*
make_strdup(char *s)
{
	char *r;

	if ((r = strdup(s)) == NULL)
		err(0, "Out of memory");
	return (r);	
}

/*
 * print_headers
 *
 * inputs	- FILE pointer to cfile
 *		- FILE pointer to hfile
 *		- FILE pointer to source file
 *		- given program name
 * output	- none
 * side effects	- headers are printed to given temp files
 */
static void
print_headers(char *src, FILE *cfile_fp, FILE *hfile_fp, char *prog_name)
{
	if (cfile) {
		/* Produce the header of the C file */

		fprintf(cfile_fp, "/*\n");
		fprintf(cfile_fp, " * This file is produced automatically.\n");
		fprintf(cfile_fp, " * Do not modify anything in here by hand.\n");
		fprintf(cfile_fp, " *\n");
		fprintf(cfile_fp, " * Created from source file\n");
		fprintf(cfile_fp, " *   %s\n",src);
		fprintf(cfile_fp, " * with\n");
		fprintf(cfile_fp, " *   %s\n",prog_name);
		fprintf(cfile_fp, " *\n");
		fprintf(cfile_fp, " * See the source file for legal information\n");
		fprintf(cfile_fp, " */\n\n");
		fprintf(cfile_fp, "#include <sys/param.h>\n");
		fprintf(cfile_fp, "#include <sys/kernel.h>\n");
		fprintf(cfile_fp, "#include <sys/kobj.h>\n");
		fprintf(cfile_fp, "#include <sys/queue.h>\n");
	}

	if (hfile) {
	/* Produce the header of the H file */

		fprintf(hfile_fp, "/*\n");
		fprintf(hfile_fp, " * This file is produced automatically.\n");
		fprintf(hfile_fp, " * Do not modify anything in here by hand.\n");
		fprintf(hfile_fp, " *\n");
		fprintf(hfile_fp, " * Created from source file\n");
		fprintf(hfile_fp, " *   %s\n",src);
		fprintf(hfile_fp, " * with\n");
		fprintf(hfile_fp, " *   %s\n",prog_name);
		fprintf(hfile_fp, " *\n");
		fprintf(hfile_fp, " * See the source file for legal information\n");
		fprintf(hfile_fp, " */\n\n");
	}
}

/*
 * process_body
 *
 * inputs	- filename of src
 *		- FILE pointer of src
 *		- FILE pointer of cfile output
 *		- FILE pointer of hfile output
 * output	- NONE
 * side effects	- exits on error
 */
static void
process_body(char *src, FILE *src_fp, FILE *cfile_fp, FILE *hfile_fp,
    int line_count)
{
	char input_buffer[MAXLINE];
	char *p;
	char *src_noext;
	int myheader;

	myheader=0;

	src_noext = strip_ext(src, ".m");

	while (fgets(input_buffer, MAXLINE-1, src_fp) != NULL) {

		/* strip newlines */
		if ((p = strchr(input_buffer, '\n')) != NULL)
			*p = '\0';
		/* Now, why does someone have carriage returns in here? */
		if ((p = strchr(input_buffer, '\r')) != NULL)
			*p = '\0';

		/* not fancy, but include include files for now */
		if (strncmp(input_buffer, "#include", 8) == 0) {
			if (cfile)
				fprintf(cfile_fp, "%s\n", input_buffer);
			continue;
		}
		/* Ignore comments */
		if (input_buffer[0] == '#')
			continue;

		/* Ignore blank lines */
		if (input_buffer[0] == '\0')
			continue;

		if (strncmp(input_buffer,"CODE", 4) == 0) {
			if (cfile && !myheader) {
				fprintf(cfile_fp, "#include \"%s.h\"\n\n",
				    src_noext);
				myheader = 1;
			}

			emit_code_section(src, src_fp, cfile_fp);

		} else if (strncmp(input_buffer, "METHOD", 6) == 0) {

			if (cfile && !myheader) {
				fprintf(cfile_fp, "#include \"%s.h\"\n\n",
				    src_noext);
				myheader = 1;
			}


			line_count = parse_method(src_noext, src_fp, cfile_fp,
			    hfile_fp, input_buffer, line_count);
		}
	}

	free(src_noext);
}


/*
 * parse_method
 *
 * inputs	- pointer to source file name
 *		- pointer to temp cfile name output
 *		- pointer to temp hfile name output
 *		- pointer to scratch input
 *		- line_count
 *		- given program name
 * output	- new line_count
 * side effects	- METHOD is parsed, correct code is emitted as necessary
 */
static int
parse_method(char *src,
		FILE *src_fp, FILE *cfile_fp, FILE *hfile_fp,
		char *input_buffer, int line_count)
{
	char *token;		/* currently being parsed token */
	char *mtype;
	char *dname;
	char *tmp_type;
	char *mname;		/* method name */
	char *p;
	char *trim_src;
	int cur_list;		/* counter into type/names list */
	int deref;

	deref = cur_list = 0;

	token = first_token(input_buffer);	/* METHOD */
	token = next_token(token); /* type or struct */

	if (strcmp(token, "struct") == 0) {
		token = next_token(token);	/* type */
		mtype = make_malloc(strlen("struct ") + strlen(token) + 1);
		strcpy(mtype, "struct ");
		/* Copy it into place, knowing where end of "struct " is */
		strcpy(mtype + 7, token);
	} else {
		mtype = make_strdup(token);
	}

	token = next_token(token);	/* name */
	deref = 0;
	if (*token == '*') {
		deref = 1;
		mname = make_strdup(token + 1);
	} else {
		if (strcmp(token, "*") == 0) {
			deref = 1;
			if((token = next_token(token)) == NULL) {
				err(0, "Null");
			}
		}
	}
	mname = make_strdup(token);
	if (debug)
		printf("deref %d mtype [%s] mname [%s]\n", deref, mtype, mname);

	while (fgets(input_buffer, MAXLINE-1, src_fp) != NULL) {
		++line_count;
		if (input_buffer[0] == '#') {
			continue;
		}

		trim_src = trim_name(src);

		if (strchr(input_buffer, '}') != NULL) {
			token = first_token(input_buffer);
			token = next_token(token);

			if (token != NULL) {
				if (strcmp(token, "DEFAULT") == 0) {
					dname = next_token(token);
				}
				if ((p = strchr(dname, ';')) != NULL)
					*p = '\0';

			} else
				dname = "0";

			if (cfile)
				emit_c_body(trim_src, src_fp, cfile_fp, mname,
				    dname);
			if (hfile)
				emit_h_body(trim_src, hfile_fp, mtype, mname,
				    cur_list);
			break;
		}

		free(trim_src);

		if ((p = strchr(input_buffer, '\n')) != NULL)
			*p = '\0';

		if ((p = strchr(input_buffer, ';')) != NULL)
			*p = '\0';

		if ((token = first_token(input_buffer)) == NULL) {
			err(0, "parse error line number %d", line_count);
		}

		if (strcmp(token, "struct") == 0) {
			if ((token = next_token(input_buffer)) == NULL) {
				err(0, "parse error line number %d",
				    line_count);
			}
			tmp_type = make_malloc(strlen("struct ") + strlen(token) + 1);
		/* Copy it into place, knowing where end of "struct " is */
			strcpy(tmp_type, "struct ");
			strcpy(tmp_type + 7, token);
		} else {
			tmp_type = make_strdup(token);
		}
		type_name_list[cur_list].mtype = tmp_type;

		if ((token = next_token(token)) == NULL) {
			err(0, "parse error line number %d", line_count);
		}
		if (*token == '*') {
			type_name_list[cur_list].deref = 1;
			type_name_list[cur_list].mname = make_strdup(token+1);
		} else {
			if (strcmp(token, "*") == 0) {
				type_name_list[cur_list].deref = 1;
				if ((token = next_token(token)) == NULL) {
					err(0, "parse error line number %d",
						 line_count);
				}
			}
			type_name_list[cur_list].mname = make_strdup(token);
		}

		cur_list++;
		if (cur_list >= MAXLIST)
			err(0, "parse error MAXLIST exceed line number %d",
				line_count);
	}

	free(mtype);
	free(mname);
	return (line_count);

}

/*
 * emit_code_section
 *
 * inputs	- filename of src
 *		- FILE pointer of src
 *		- FILE pointer of cfile output
 * output	- NONE
 * side effects	- exits on error
 */
static void
emit_code_section(char *src, FILE *src_fp, FILE *cfile_fp)
{
	char input_buffer[MAXLINE];

	while (fgets(input_buffer, MAXLINE - 1, src_fp) != NULL) {

/* XXX can do better then a simple strncmp
 * can strchr both '}' and ';' if needed. 
 * could also count brace depth. i.e. - if } seen if reaches 0, done.
 */
		if (strncmp(input_buffer, "};", 2) == 0)
			return;
		if (cfile)
			fprintf (cfile_fp, "%s", input_buffer);
	}
}

/*
 * upper_case
 *
 * inputs	- pointer to name to upper case
 * output	- pointer to given string as upper case
 * side effects	- caller is responsible for freeing memory
 */
static char *
upper_case(char *name)
{
	static char *upper;
	char *p;

	upper = make_strdup(name);

	for (p = upper; *p; p++) {
		*p = toupper(*p);
	}

	return (upper);
}

/*
 * emit_c_body
 *
 * inputs	- filename of src
 *		- FILE pointer of src
 *		- FILE pointer of cfile output
 *		- method name
 * output	- NONE
 * side effects	- exits on error
 */
static void
emit_c_body(char *src, FILE *src_fp, FILE *cfile_fp, char *mname, char *dname)
{
	if (cfile) {
		fprintf(cfile_fp, "struct kobjop_desc %s_%s_desc = {\n", src,
		    mname);
		fprintf(cfile_fp, "\t0, (kobjop_t) %s\n", dname);
		fprintf(cfile_fp, "};\n\n");
	}

}

/*
 * emit_h_body
 *
 * inputs	- filename of src
 *		- FILE pointer of src
 *		- FILE pointer of hfile output
 *		- method name
 * output	- NONE
 * side effects	- exits on error
 */
static void
emit_h_body(char *src, FILE *hfile_fp, char *mtype, char *mname, int max_list)
{
	char *upper_case_src;
	char *upper_case_mname;
	int i;

	upper_case_src = upper_case(src);
	upper_case_mname = upper_case(mname);

	fprintf(hfile_fp, "extern struct kobjop_desc %s_%s_desc;\n", src,
	    mname);
	fprintf(hfile_fp, "typedef %s %s_%s_t(", mtype, src, mname);

	for (i = 0; i < max_list; i++) {
			if ((i+1) != max_list) {
				fprintf (hfile_fp, "%s %s%s, ",
				    type_name_list[i].mtype,
				    type_name_list[i].deref ? "*" : "",
				    type_name_list[i].mname);

			  } else {
				fprintf (hfile_fp, "%s %s%s);\n",
				    type_name_list[i].mtype,
				    type_name_list[i].deref ? "*" : "",
				    type_name_list[i].mname);
			}
	}

	fprintf(hfile_fp,"static __inline %s %s_%s(", mtype, upper_case_src,
	    upper_case_mname);

	for (i = 0; i < max_list; i++) {
			if ((i+1) != max_list) {
				fprintf (hfile_fp, "%s %s%s, ",
				    type_name_list[i].mtype,
				    type_name_list[i].deref ? "*" : "",
				    type_name_list[i].mname);

			  } else {
				fprintf (hfile_fp, "%s %s%s)\n",
				    type_name_list[i].mtype,
				    type_name_list[i].deref ? "*" : "",
				    type_name_list[i].mname);
			}
	}

	fprintf(hfile_fp, "{\n");
	fprintf(hfile_fp, "\tkobjop_t _m;\n");
	fprintf(hfile_fp, "\tKOBJOPLOOKUP(((kobj_t)%s)->ops,%s_%s);\n",
	    type_name_list[0].mname, src, mname);

	if (strcmp(mtype,"void") != 0) {
		fprintf(hfile_fp, "\treturn ((%s_%s_t *) _m)(", src, mname);
	}

	for (i = 0; i < max_list; i++) {
			if ((i+1) != max_list) {
				fprintf (hfile_fp, "%s, ",
				    type_name_list[i].mname);

			  } else {
				fprintf (hfile_fp, "%s);\n",
				    type_name_list[i].mname);
			}
	}

	fprintf(hfile_fp, "}\n");
	fprintf(hfile_fp, "\n");

	free(upper_case_src);
	free(upper_case_mname);
}


/*
 * make_copy
 *
 * inputs	- file to copy from
 *		- file to copy to
 * output	- NONE
 * side effects	- exits on error
 */
static void
make_copy(char *fin, char *fout)
{
	int fd_in;
	int fd_out;
	char buffer[MAXLINE];
	int nread;

	if ((fd_in = open(fin, O_RDONLY)) < 0)
		err(0, "Cannot open %s for read", fin);

	if ((fd_out = open(fout, O_WRONLY|O_CREAT|O_TRUNC, 0666)) < 0)
		err(0, "Cannot open %s for write", fout);


	while ((nread = read(fd_in, buffer, MAXLINE)) > 0)
		write(fd_out, buffer, nread);

	close(fd_in);
	close(fd_out);
}

/*
 * strip_ext
 *
 * inputs	- pointer to string
 * output	- pointer to string without extension
 * side effects	- NONE
 */
static char *
strip_ext(char *s, char *ext)
{
	char *t;
	char *p;

	t = make_strdup(s);
	if ((p = strstr(t, ext)) != NULL) {

		/* The observant will note this leaves 2 extra bytes allocated
		 * unnecessarily. *tough* memory is cheap.
		 */
		*p = '\0';
	}
	return (t);		
}

/*
 * first_token
 *
 * inputs	- pointer to string
 * output	- pointer to next token
 * side effects	- NONE
 */
static char*
first_token(char *s)
{
	char *t;

	if (s == NULL)
		return (NULL);
	if (*s == '\0')
		return (NULL);

	while (isspace(*s))
		s++;
	t = s;
	while (!isspace(*t))
		t++;
	*t = '\0';
	return (s);
}

/*
 * next_token
 *
 * inputs	- pointer to string
 * output	- pointer to next token
 * side effects	- NONE
 */
static char*
next_token(char *s)
{
	char *t;

	if (s == NULL)
		return (NULL);
	while (*s != '\0')
		s++;
	s++;
	if (*s == '\0')
		return (NULL);
	
	while (isspace(*s))
		s++;
	t = s;

	while (!isspace(*t))
		t++;
	*t = '\0';
	
	return (s);
}

/*
 * trim_name
 *
 * inputs	- pointer to name to trim
 * output	- pointer to static trimmed to first '_'
 *		  i.e 'foo_h' trimmed to 'foo'
 * side effects	- NONE
 */
static char *
trim_name(char *name)
{
	char *trimmed;
	char *p;

	trimmed = make_strdup(name);

	if ((p = strchr(trimmed, '_')) != NULL)
		*p = '\0';
	return (trimmed);
}

/*
 * make_malloc
 *
 * inputs	- number of byte to allocate
 * output	- pointer to allocated memory
 * side effects	- exits if unable to malloc
 */
static char*
make_malloc(int size)
{
	char *s;

	s = malloc(size);

	if (s == NULL)
		err(0, "Out of memory");
	return (s);	
}
