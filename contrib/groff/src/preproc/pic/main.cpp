// -*- C++ -*-
/* Copyright (C) 1989-1992, 2000, 2001, 2002, 2003
   Free Software Foundation, Inc.
     Written by James Clark (jjc@jclark.com)

This file is part of groff.

groff is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free
Software Foundation; either version 2, or (at your option) any later
version.

groff is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License along
with groff; see the file COPYING.  If not, write to the Free Software
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA. */

#include "pic.h"

extern int yyparse();
extern "C" const char *Version_string;

output *out;
char *graphname;		// the picture box name in TeX mode

int flyback_flag;
int zero_length_line_flag = 0;
// Non-zero means we're using a groff driver.
int driver_extension_flag = 1;
int compatible_flag = 0;
int safer_flag = 1;
int command_char = '.';		// the character that introduces lines
				// that should be passed through tranparently
static int lf_flag = 1;		// non-zero if we should attempt to understand
				// lines beginning with `.lf'

// Non-zero means a parse error was encountered.
static int had_parse_error = 0;

void do_file(const char *filename);

class top_input : public input {
  FILE *fp;
  int bol;
  int eof;
  int push_back[3];
  int start_lineno;
public:
  top_input(FILE *);
  int get();
  int peek();
  int get_location(const char **, int *);
};

top_input::top_input(FILE *p) : fp(p), bol(1), eof(0)
{
  push_back[0] = push_back[1] = push_back[2] = EOF;
  start_lineno = current_lineno;
}

int top_input::get()
{
  if (eof)
    return EOF;
  if (push_back[2] != EOF) {
    int c = push_back[2];
    push_back[2] = EOF;
    return c;
  }
  else if (push_back[1] != EOF) {
    int c = push_back[1];
    push_back[1] = EOF;
    return c;
  }
  else if (push_back[0] != EOF) {
    int c = push_back[0];
    push_back[0] = EOF;
    return c;
  }
  int c = getc(fp);
  while (invalid_input_char(c)) {
    error("invalid input character code %1", int(c));
    c = getc(fp);
    bol = 0;
  }
  if (bol && c == '.') {
    c = getc(fp);
    if (c == 'P') {
      c = getc(fp);
      if (c == 'F' || c == 'E') {
	int d = getc(fp);
	if (d != EOF)
	  ungetc(d, fp);
	if (d == EOF || d == ' ' || d == '\n' || compatible_flag) {
	  eof = 1;
	  flyback_flag = c == 'F';
	  return EOF;
	}
	push_back[0] = c;
	push_back[1] = 'P';
	return '.';
      }
      if (c == 'S') {
	c = getc(fp);
	if (c != EOF)
	  ungetc(c, fp);
	if (c == EOF || c == ' ' || c == '\n' || compatible_flag) {
	  error("nested .PS");
	  eof = 1;
	  return EOF;
	}
	push_back[0] = 'S';
	push_back[1] = 'P';
	return '.';
      }
      if (c != EOF)
	ungetc(c, fp);
      push_back[0] = 'P';
      return '.';
    }
    else {
      if (c != EOF)
	ungetc(c, fp);
      return '.';
    }
  }
  if (c == '\n') {
    bol = 1;
    current_lineno++;
    return '\n';
  }
  bol = 0;
  if (c == EOF) {
    eof = 1;
    error("end of file before .PE or .PF");
    error_with_file_and_line(current_filename, start_lineno - 1,
			     ".PS was here");
  }
  return c;
}

int top_input::peek()
{
  if (eof)
    return EOF;
  if (push_back[2] != EOF)
    return push_back[2];
  if (push_back[1] != EOF)
    return push_back[1];
  if (push_back[0] != EOF)
    return push_back[0];
  int c = getc(fp);
  while (invalid_input_char(c)) {
    error("invalid input character code %1", int(c));
    c = getc(fp);
    bol = 0;
  }
  if (bol && c == '.') {
    c = getc(fp);
    if (c == 'P') {
      c = getc(fp);
      if (c == 'F' || c == 'E') {
	int d = getc(fp);
	if (d != EOF)
	  ungetc(d, fp);
	if (d == EOF || d == ' ' || d == '\n' || compatible_flag) {
	  eof = 1;
	  flyback_flag = c == 'F';
	  return EOF;
	}
	push_back[0] = c;
	push_back[1] = 'P';
	push_back[2] = '.';
	return '.';
      }
      if (c == 'S') {
	c = getc(fp);
	if (c != EOF)
	  ungetc(c, fp);
	if (c == EOF || c == ' ' || c == '\n' || compatible_flag) {
	  error("nested .PS");
	  eof = 1;
	  return EOF;
	}
	push_back[0] = 'S';
	push_back[1] = 'P';
	push_back[2] = '.';
	return '.';
      }
      if (c != EOF)
	ungetc(c, fp);
      push_back[0] = 'P';
      push_back[1] = '.';
      return '.';
    }
    else {
      if (c != EOF)
	ungetc(c, fp);
      push_back[0] = '.';
      return '.';
    }
  }
  if (c != EOF)
    ungetc(c, fp);
  if (c == '\n')
    return '\n';
  return c;
}

int top_input::get_location(const char **filenamep, int *linenop)
{
  *filenamep = current_filename;
  *linenop = current_lineno;
  return 1;
}

void do_picture(FILE *fp)
{
  flyback_flag = 0;
  int c;
  a_delete graphname;
  graphname = strsave("graph");		// default picture name in TeX mode
  while ((c = getc(fp)) == ' ')
    ;
  if (c == '<') {
    string filename;
    while ((c = getc(fp)) == ' ')
      ;
    while (c != EOF && c != ' ' && c != '\n') {
      filename += char(c);
      c = getc(fp);
    }
    if (c == ' ') {
      do {
	c = getc(fp);
      } while (c != EOF && c != '\n');
    }
    if (c == '\n') 
      current_lineno++;
    if (filename.length() == 0)
      error("missing filename after `<'");
    else {
      filename += '\0';
      const char *old_filename = current_filename;
      int old_lineno = current_lineno;
      // filenames must be permanent
      do_file(strsave(filename.contents()));
      current_filename = old_filename;
      current_lineno = old_lineno;
    }
    out->set_location(current_filename, current_lineno);
  }
  else {
    out->set_location(current_filename, current_lineno);
    string start_line;
    while (c != EOF) {
      if (c == '\n') {
	current_lineno++;
	break;
      }
      start_line += c;
      c = getc(fp);
    }
    if (c == EOF)
      return;
    start_line += '\0';
    double wid, ht;
    switch (sscanf(&start_line[0], "%lf %lf", &wid, &ht)) {
    case 1:
      ht = 0.0;
      break;
    case 2:
      break;
    default:
      ht = wid = 0.0;
      break;
    }
    out->set_desired_width_height(wid, ht);
    out->set_args(start_line.contents());
    lex_init(new top_input(fp));
    if (yyparse()) {
      had_parse_error = 1;
      lex_error("giving up on this picture");
    }
    parse_cleanup();
    lex_cleanup();

    // skip the rest of the .PF/.PE line
    while ((c = getc(fp)) != EOF && c != '\n')
      ;
    if (c == '\n')
      current_lineno++;
    out->set_location(current_filename, current_lineno);
  }
}

void do_file(const char *filename)
{
  FILE *fp;
  if (strcmp(filename, "-") == 0)
    fp = stdin;
  else {
    errno = 0;
    fp = fopen(filename, "r");
    if (fp == 0) {
      delete out;
      fatal("can't open `%1': %2", filename, strerror(errno));
    }
  }
  out->set_location(filename, 1);
  current_filename = filename;
  current_lineno = 1;
  enum { START, MIDDLE, HAD_DOT, HAD_P, HAD_PS, HAD_l, HAD_lf } state = START;
  for (;;) {
    int c = getc(fp);
    if (c == EOF)
      break;
    switch (state) {
    case START:
      if (c == '.')
	state = HAD_DOT;
      else {
	putchar(c);
	if (c == '\n') {
	  current_lineno++;
	  state = START;
	}
	else
	  state = MIDDLE;
      }
      break;
    case MIDDLE:
      putchar(c);
      if (c == '\n') {
	current_lineno++;
	state = START;
      }
      break;
    case HAD_DOT:
      if (c == 'P')
	state = HAD_P;
      else if (lf_flag && c == 'l')
	state = HAD_l;
      else {
	putchar('.');
	putchar(c);
	if (c == '\n') {
	  current_lineno++;
	  state = START;
	}
	else
	  state = MIDDLE;
      }
      break;
    case HAD_P:
      if (c == 'S')
	state = HAD_PS;
      else  {
	putchar('.');
	putchar('P');
	putchar(c);
	if (c == '\n') {
	  current_lineno++;
	  state = START;
	}
	else
	  state = MIDDLE;
      }
      break;
    case HAD_PS:
      if (c == ' ' || c == '\n' || compatible_flag) {
	ungetc(c, fp);
	do_picture(fp);
	state = START;
      }
      else {
	fputs(".PS", stdout);
	putchar(c);
	state = MIDDLE;
      }
      break;
    case HAD_l:
      if (c == 'f')
	state = HAD_lf;
      else {
	putchar('.');
	putchar('l');
	putchar(c);
	if (c == '\n') {
	  current_lineno++;
	  state = START;
	}
	else
	  state = MIDDLE;
      }
      break;
    case HAD_lf:
      if (c == ' ' || c == '\n' || compatible_flag) {
	string line;
	while (c != EOF) {
	  line += c;
	  if (c == '\n') {
	    current_lineno++;
	    break;
	  }
	  c = getc(fp);
	}
	line += '\0';
	interpret_lf_args(line.contents());
	printf(".lf%s", line.contents());
	state = START;
      }
      else {
	fputs(".lf", stdout);
	putchar(c);
	state = MIDDLE;
      }
      break;
    default:
      assert(0);
    }
  }
  switch (state) {
  case START:
    break;
  case MIDDLE:
    putchar('\n');
    break;
  case HAD_DOT:
    fputs(".\n", stdout);
    break;
  case HAD_P:
    fputs(".P\n", stdout);
    break;
  case HAD_PS:
    fputs(".PS\n", stdout);
    break;
  case HAD_l:
    fputs(".l\n", stdout);
    break;
  case HAD_lf:
    fputs(".lf\n", stdout);
    break;
  }
  if (fp != stdin)
    fclose(fp);
}

#ifdef FIG_SUPPORT
void do_whole_file(const char *filename)
{
  // Do not set current_filename.
  FILE *fp;
  if (strcmp(filename, "-") == 0)
    fp = stdin;
  else {
    errno = 0;
    fp = fopen(filename, "r");
    if (fp == 0)
      fatal("can't open `%1': %2", filename, strerror(errno));
  }
  lex_init(new file_input(fp, filename));
  if (yyparse())
    had_parse_error = 1;
  parse_cleanup();
  lex_cleanup();
}
#endif

void usage(FILE *stream)
{
  fprintf(stream, "usage: %s [ -nvC ] [ filename ... ]\n", program_name);
#ifdef TEX_SUPPORT
  fprintf(stream, "       %s -t [ -cvzC ] [ filename ... ]\n", program_name);
#endif
#ifdef FIG_SUPPORT
  fprintf(stream, "       %s -f [ -v ] [ filename ]\n", program_name);
#endif
}

#if defined(__MSDOS__) || defined(__EMX__)
static char *fix_program_name(char *arg, char *dflt)
{
  if (!arg)
    return dflt;
  char *prog = strchr(arg, '\0');
  for (;;) {
    if (prog == arg)
      break;
    --prog;
    if (strchr("\\/:", *prog)) {
      prog++;
      break;
    }
  }	
  char *ext = strchr(prog, '.');
  if (ext)
    *ext = '\0';
  for (char *p = prog; *p; p++)
    if ('A' <= *p && *p <= 'Z')
      *p = 'a' + (*p - 'A');
  return prog;
}
#endif /* __MSDOS__ || __EMX__ */

int main(int argc, char **argv)
{
  setlocale(LC_NUMERIC, "C");
#if defined(__MSDOS__) || defined(__EMX__)
  argv[0] = fix_program_name(argv[0], "pic");
#endif /* __MSDOS__ || __EMX__ */
  program_name = argv[0];
  static char stderr_buf[BUFSIZ];
  setbuf(stderr, stderr_buf);
  int opt;
#ifdef TEX_SUPPORT
  int tex_flag = 0;
  int tpic_flag = 0;
#endif
#ifdef FIG_SUPPORT
  int whole_file_flag = 0;
  int fig_flag = 0;
#endif
  static const struct option long_options[] = {
    { "help", no_argument, 0, CHAR_MAX + 1 },
    { "version", no_argument, 0, 'v' },
    { NULL, 0, 0, 0 }
  };
  while ((opt = getopt_long(argc, argv, "T:CDSUtcvnxzpf", long_options, NULL))
	 != EOF)
    switch (opt) {
    case 'C':
      compatible_flag = 1;
      break;
    case 'D':
    case 'T':
      break;
    case 'S':
      safer_flag = 1;
      break;
    case 'U':
      safer_flag = 0;
      break;
    case 'f':
#ifdef FIG_SUPPORT
      whole_file_flag++;
      fig_flag++;
#else
      fatal("fig support not included");
#endif
      break;
    case 'n':
      driver_extension_flag = 0;
      break;
    case 'p':
    case 'x':
      warning("-%1 option is obsolete", char(opt));
      break;
    case 't':
#ifdef TEX_SUPPORT
      tex_flag++;
#else
      fatal("TeX support not included");
#endif
      break;
    case 'c':
#ifdef TEX_SUPPORT
      tpic_flag++;
#else
      fatal("TeX support not included");
#endif
      break;
    case 'v':
      {
	printf("GNU pic (groff) version %s\n", Version_string);
	exit(0);
	break;
      }
    case 'z':
      // zero length lines will be printed as dots
      zero_length_line_flag++;
      break;
    case CHAR_MAX + 1: // --help
      usage(stdout);
      exit(0);
      break;
    case '?':
      usage(stderr);
      exit(1);
      break;
    default:
      assert(0);
    }
  parse_init();
#ifdef TEX_SUPPORT
  if (tpic_flag) {
    out = make_tpic_output();
    lf_flag = 0;
  }
  else if (tex_flag) {
    out = make_tex_output();
    command_char = '\\';
    lf_flag = 0;
  }
  else
#endif
#ifdef FIG_SUPPORT
  if (fig_flag)
    out = make_fig_output();
  else
#endif
    out = make_troff_output();
#ifdef FIG_SUPPORT
  if (whole_file_flag) {
    if (optind >= argc)
      do_whole_file("-");
    else if (argc - optind > 1) {
      usage(stderr);
      exit(1);
    } else
      do_whole_file(argv[optind]);
  }
  else {
#endif
    if (optind >= argc)
      do_file("-");
    else
      for (int i = optind; i < argc; i++)
	do_file(argv[i]);
#ifdef FIG_SUPPORT
  }
#endif
  delete out;
  if (ferror(stdout) || fflush(stdout) < 0)
    fatal("output error");
  return had_parse_error;
}

