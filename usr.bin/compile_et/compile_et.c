/*
 *
 * Copyright 1986, 1987 by MIT Student Information Processing Board
 * For copyright info, see "Copyright.SIPB".
 *
 *	$Id: compile_et.c,v 1.2 1994/07/19 19:21:24 g89r4222 Exp $
 */

#include <stdio.h>
#include <sys/file.h>
#include <strings.h>
#include <sys/param.h>

static char copyright[] = "Copyright 1987 by MIT Student Information Processing Board";

extern char *gensym();
extern char *current_token;
extern int table_number, current;
char buffer[BUFSIZ];
char *table_name = (char *)NULL;
FILE *hfile, *cfile;
     
/* C library */
extern char *malloc();
extern int errno;
     
/* lex stuff */
extern FILE *yyin;
extern int yylineno;
     
/* pathnames */
char c_file[MAXPATHLEN];	/* temporary file */
char h_file[MAXPATHLEN];	/* output */
char o_file[MAXPATHLEN];	/* output */
char et_file[MAXPATHLEN];	/* input */

main(argc, argv)
     int argc;
     char **argv;
{
     register char *p;
     int n_flag = 0, debug = 0;
     
     while (argc > 2) {
	  register char *arg, ch;
	  arg = argv[--argc];
	  if (strlen(arg) != 2 || arg[0] != '-')
	       goto usage;
	  ch = arg[1];
	  if (ch == 'n')
	       n_flag++;
	  else if (ch == 'd')
	       debug++;
	  else
	       goto usage;
     }

     if (argc != 2) {
     usage:
	  fprintf(stderr, "Usage:  %s et_file [-n]\n", argv[0]);
	  exit(1);
     }
     
     strcpy(et_file, argv[1]);
     p = rindex(et_file, '/');
     if (p == (char *)NULL)
	  p = et_file;
     else
	  p++;
     p = rindex(p, '.');
     if (!strcmp(p, ".et"))
	  *++p = '\0';
     else {
	  if (!p)
	       p = et_file;
	  while (*p)
	       p++;
	  *p++ = '.';
	  *p = '\0';
     }
     /* p points at null where suffix should be */
     strcpy(p, "et.c");
     strcpy(c_file, et_file);
     p[0] = 'h';
     p[1] = '\0';
     strcpy(h_file, et_file);
     p[0] = 'o';
     strcpy(o_file, et_file);
     p[0] = 'e';
     p[1] = 't';
     p[2] = '\0';

     yyin = fopen(et_file, "r");
     if (!yyin) {
	  perror(et_file);
	  exit(1);
     }
     
     hfile = fopen(h_file, "w");
     if (hfile == (FILE *)NULL) {
	  perror(h_file);
	  exit(1);
     }
     
     cfile = fopen(c_file, "w");
     if (cfile == (FILE *)NULL) {
	  perror("Can't open temp file");
	  exit(1);
     }
     
     /* parse it */
     fputs("#define NULL 0\n", cfile);
     fputs("static char *_et[] = {\n", cfile);
     
     yyparse();
     fclose(yyin);		/* bye bye input file */
     
     fputs("\t(char *)0\n};\n", cfile);
     fputs("extern int init_error_table();\n\n", cfile);
     fprintf(cfile, "int %s_err_base = %d;\n\n", table_name, table_number);
     fprintf(cfile, "int\ninit_%s_err_tbl()\n", table_name);
     fprintf(cfile, "{\n\treturn(init_error_table(_et, %d, %d));\n}\n",
	     table_number, current);
     fclose(cfile);
     
     fputs("extern int init_", hfile);
     fputs(table_name, hfile);
     fputs("_err_tbl();\nextern int ", hfile);
     fputs(table_name, hfile);
     fputs("_err_base;\n", hfile);
     fclose(hfile);		/* bye bye hfile */
     
     if (n_flag)
	  exit(0);

     if (!fork()) {
	  p = rindex(c_file, '/');
	  if (p) {
	       *p++ = '\0';
	       chdir(c_file);
	  }
	  else
	       p = c_file;
	  execlp("cc", "cc", "-c", "-R", "-O", p, 0);
	  perror("cc");
	  exit(1);
     }
     else wait(0);

     if (!debug)
	  (void) unlink(c_file);
     /* make it .o file name */
     c_file[strlen(c_file)-1] = 'o';
     if (!fork()) {
	  execlp("cp", "cp", c_file, o_file, 0);
	  perror("cp");
	  exit(1);
     }
     else wait(0);
     if (!debug)
	  (void) unlink(c_file);

     exit(0);
}

yyerror(s)
     char *s;
{
     fputs(s, stderr);
     fprintf(stderr, "\nLine number %d; last token was '%s'\n",
	     yylineno, current_token);
}
