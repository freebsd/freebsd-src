// -*- C++ -*-
/* Copyright (C) 1989, 1990, 1991, 1992 Free Software Foundation, Inc.
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

#include "refer.h"
#include "refid.h"
#include "ref.h"
#include "token.h"
#include "search.h"
#include "command.h"

const char PRE_LABEL_MARKER = '\013';
const char POST_LABEL_MARKER = '\014';
const char LABEL_MARKER = '\015'; // label_type is added on

#define FORCE_LEFT_BRACKET 04
#define FORCE_RIGHT_BRACKET 010

static FILE *outfp = stdout;

string capitalize_fields;
string reverse_fields;
string abbreviate_fields;
string period_before_last_name = ". ";
string period_before_initial = ".";
string period_before_hyphen = "";
string period_before_other = ". ";
string sort_fields;
int annotation_field = -1;
string annotation_macro;
string discard_fields = "XYZ";
string pre_label = "\\*([.";
string post_label = "\\*(.]";
string sep_label = ", ";
int accumulate = 0;
int move_punctuation = 0;
int abbreviate_label_ranges = 0;
string label_range_indicator;
int label_in_text = 1;
int label_in_reference = 1;
int date_as_label = 0;
int sort_adjacent_labels = 0;
// Join exactly two authors with this.
string join_authors_exactly_two = " and ";
// When there are more than two authors join the last two with this.
string join_authors_last_two = ", and ";
// Otherwise join authors with this.
string join_authors_default = ", ";
string separate_label_second_parts = ", ";
// Use this string to represent that there are other authors.
string et_al = " et al";
// Use et al only if it can replace at least this many authors.
int et_al_min_elide = 2;
// Use et al only if the total number of authors is at least this.
int et_al_min_total = 3;


int compatible_flag = 0;

int short_label_flag = 0;

static int recognize_R1_R2 = 1;

search_list database_list;
int search_default = 1;
static int default_database_loaded = 0;

static reference **citation = 0;
static int ncitations = 0;
static int citation_max = 0;

static reference **reference_hash_table = 0;
static int hash_table_size;
static int nreferences = 0;

static int need_syncing = 0;
string pending_line;
string pending_lf_lines;

static void output_pending_line();
static unsigned immediately_handle_reference(const string &);
static void immediately_output_references();
static unsigned store_reference(const string &);
static void divert_to_temporary_file();
static reference *make_reference(const string &, unsigned *);
static void usage();
static void do_file(const char *);
static void split_punct(string &line, string &punct);
static void output_citation_group(reference **v, int n, label_type, FILE *fp);
static void possibly_load_default_database();

int main(int argc, char **argv)
{
  program_name = argv[0];
  static char stderr_buf[BUFSIZ];
  setbuf(stderr, stderr_buf);
  outfp = stdout;
  int finished_options = 0;
  int bib_flag = 0;
  int done_spec = 0;

  for (--argc, ++argv;
       !finished_options && argc > 0 && argv[0][0] == '-'
       && argv[0][1] != '\0';
       argv++, argc--) {
    const char *opt = argv[0] + 1; 
    while (opt != 0 && *opt != '\0') {
      switch (*opt) {
      case 'C':
	compatible_flag = 1;
	opt++;
	break;
      case 'B':
	bib_flag = 1;
	label_in_reference = 0;
	label_in_text = 0;
	++opt;
	if (*opt == '\0') {
	  annotation_field = 'X';
	  annotation_macro = "AP";
	}
	else if (csalnum(opt[0]) && opt[1] == '.' && opt[2] != '\0') {
	  annotation_field = opt[0];
	  annotation_macro = opt + 2;
	}
	opt = 0;
	break;
      case 'P':
	move_punctuation = 1;
	opt++;
	break;
      case 'R':
	recognize_R1_R2 = 0;
	opt++;
	break;
      case 'S':
	// Not a very useful spec.
	set_label_spec("(A.n|Q)', '(D.y|D)");
	done_spec = 1;
	pre_label = " (";
	post_label = ")";
	sep_label = "; ";
	opt++;
	break;
      case 'V':
	verify_flag = 1;
	opt++;
	break;
      case 'f':
	{
	  const char *num = 0;
	  if (*++opt == '\0') {
	    if (argc > 1) {
	      num = *++argv;
	      --argc;
	    }
	    else {
	      error("option `f' requires an argument");
	      usage();
	    }
	  }
	  else {
	    num = opt;
	    opt = 0;
	  }
	  const char *ptr;
	  for (ptr = num; *ptr; ptr++)
	    if (!csdigit(*ptr)) {
	      error("bad character `%1' in argument to -f option", *ptr);
	      break;
	    }
	  if (*ptr == '\0') {
	    string spec;
	    spec = '%';
	    spec += num;
	    spec += '\0';
	    set_label_spec(spec.contents());
	    done_spec = 1;
	  }
	  break;
	}
      case 'b':
	label_in_text = 0;
	label_in_reference = 0;
	opt++;
	break;
      case 'e':
	accumulate = 1;
	opt++;
	break;
      case 'c':
	capitalize_fields = ++opt;
	opt = 0;
	break;
      case 'k':
	{
	  char buf[5];
	  if (csalpha(*++opt))
	    buf[0] = *opt++;
	  else {
	    if (*opt != '\0')
	      error("bad field name `%1'", *opt++);
	    buf[0] = 'L';
	  }
	  buf[1] = '~';
	  buf[2] = '%';
	  buf[3] = 'a';
	  buf[4] = '\0';
	  set_label_spec(buf);
	  done_spec = 1;
	}
	break;
      case 'a':
	{
	  const char *ptr;
	  for (ptr = ++opt; *ptr; ptr++)
	    if (!csdigit(*ptr)) {
	      error("argument to `a' option not a number");
	      break;
	    }
	  if (*ptr == '\0') {
	    reverse_fields = 'A';
	    reverse_fields += opt;
	  }
	  opt = 0;
	}
	break;
      case 'i':
	linear_ignore_fields = ++opt;
	opt = 0;
	break;
      case 'l':
	{
	  char buf[INT_DIGITS*2 + 11]; // A.n+2D.y-3%a
	  strcpy(buf, "A.n");
	  if (*++opt != '\0' && *opt != ',') {
	    char *ptr;
	    long n = strtol(opt, &ptr, 10);
	    if (n == 0 && ptr == opt) {
	      error("bad integer `%1' in `l' option", opt);
	      opt = 0;
	      break;
	    }
	    if (n < 0)
	      n = 0;
	    opt = ptr;
	    sprintf(strchr(buf, '\0'), "+%ld", n);
	  }
	  strcat(buf, "D.y");
	  if (*opt == ',')
	    opt++;
	  if (*opt != '\0') {
	    char *ptr;
	    long n = strtol(opt, &ptr, 10);
	    if (n == 0 && ptr == opt) {
	      error("bad integer `%1' in `l' option", opt);
	      opt = 0;
	      break;
	    }
	    if (n < 0)
	      n = 0;
	    sprintf(strchr(buf, '\0'), "-%ld", n);
	    opt = ptr;
	    if (*opt != '\0')
	      error("argument to `l' option not of form `m,n'");
	  }
	  strcat(buf, "%a");
	  if (!set_label_spec(buf))
	    assert(0);
	  done_spec = 1;
	}
	break;
      case 'n':
	search_default = 0;
	opt++;
	break;
      case 'p':
	{
	  const char *filename = 0;
	  if (*++opt == '\0') {
	    if (argc > 1) {
	      filename = *++argv;
	      argc--;
	    }
	    else {
	      error("option `p' requires an argument");
	      usage();
	    }
	  }
	  else {
	    filename = opt;
	    opt = 0;
	  }
	  database_list.add_file(filename);
	}
	break;
      case 's':
	if (*++opt == '\0')
	  sort_fields = "AD";
	else {
	  sort_fields = opt;
	  opt = 0;
	}
	accumulate = 1;
	break;
      case 't':
	{
	  char *ptr;
	  long n = strtol(opt, &ptr, 10);
	  if (n == 0 && ptr == opt) {
	    error("bad integer `%1' in `t' option", opt);
	    opt = 0;
	    break;
	  }
	  if (n < 1)
	    n = 1;
	  linear_truncate_len = int(n);
	  opt = ptr;
	  break;
	}
      case 'v':
	{
	  extern const char *version_string;
	  fprintf(stderr, "GNU refer version %s\n", version_string);
	  fflush(stderr);
	  opt++;
	  break;
	}
      case '-':
	if (opt[1] == '\0') {
	  finished_options = 1;
	  opt++;
	  break;
	}
	// fall through
      default:
	error("unrecognized option `%1'", *opt);
	usage();
	break;
      }
    }
  }
  if (!done_spec)
    set_label_spec("%1");
  if (argc <= 0) {
    if (bib_flag)
      do_bib("-");
    else
      do_file("-");
  }
  else {
    for (int i = 0; i < argc; i++) {
      if (bib_flag)
	do_bib(argv[i]);
      else
	do_file(argv[i]);
    }
  }
  if (accumulate)
    output_references();
  if (fflush(stdout) < 0)
    fatal("output error");
  return 0;
}

static void usage()
{
  fprintf(stderr,
"usage: %s [-benvCPRS] [-aN] [-cXYZ] [-fN] [-iXYZ] [-kX] [-lM,N] [-p file]\n"
"       [-sXYZ] [-tN] [-BL.M] [files ...]\n",
	  program_name);
  exit(1);
}

static void possibly_load_default_database()
{
  if (search_default && !default_database_loaded) {
    char *filename = getenv("REFER");
    if (filename)
      database_list.add_file(filename);
    else
      database_list.add_file(DEFAULT_INDEX, 1);
    default_database_loaded = 1;
  }
}

static int is_list(const string &str)
{
  const char *start = str.contents();
  const char *end = start + str.length();
  while (end > start && csspace(end[-1]))
    end--;
  while (start < end && csspace(*start))
    start++;
  return end - start == 6 && memcmp(start, "$LIST$", 6) == 0;
}

static void do_file(const char *filename)
{
  FILE *fp;
  if (strcmp(filename, "-") == 0) {
    fp = stdin;
  }
  else {
    errno = 0;
    fp = fopen(filename, "r");
    if (fp == 0) {
      error("can't open `%1': %2", filename, strerror(errno));
      return;
    }
  }
  current_filename = filename;
  fprintf(outfp, ".lf 1 %s\n", filename);
  string line;
  current_lineno = 0;
  for (;;) {
    line.clear();
    for (;;) {
      int c = getc(fp);
      if (c == EOF) {
	if (line.length() > 0)
	  line += '\n';
	break;
      }
      if (illegal_input_char(c))
	error("illegal input character code %1", c);
      else {
	line += c;
	if (c == '\n')
	  break;
      }
    }
    int len = line.length();
    if (len == 0)
      break;
    current_lineno++;
    if (len >= 2 && line[0] == '.' && line[1] == '[') {
      int start_lineno = current_lineno;
      int start_of_line = 1;
      string str;
      string post;
      string pre(line.contents() + 2, line.length() - 3);
      for (;;) {
	int c = getc(fp);
	if (c == EOF) {
	  error_with_file_and_line(current_filename, start_lineno,
				   "missing `.]' line");
	  break;
	}
	if (start_of_line)
	  current_lineno++;
	if (start_of_line && c == '.') {
	  int d = getc(fp);
	  if (d == ']') {
	    while ((d = getc(fp)) != '\n' && d != EOF) {
	      if (illegal_input_char(d))
		error("illegal input character code %1", d);
	      else
		post += d;
	    }
	    break;
	  }
	  if (d != EOF)
	    ungetc(d, fp);
	}
	if (illegal_input_char(c))
	  error("illegal input character code %1", c);
	else
	  str += c;
	start_of_line = (c == '\n');
      }
      if (is_list(str)) {
	output_pending_line();
	if (accumulate)
	  output_references();
	else
	  error("found `$LIST$' but not accumulating references");
      }
      else {
	unsigned flags = (accumulate
			  ? store_reference(str)
			  : immediately_handle_reference(str));
	if (label_in_text) {
	  if (accumulate && outfp == stdout)
	    divert_to_temporary_file();
	  if (pending_line.length() == 0) {
	    warning("can't attach citation to previous line");
	  }
	  else
	    pending_line.set_length(pending_line.length() - 1);
	  string punct;
	  if (move_punctuation)
	    split_punct(pending_line, punct);
	  int have_text = pre.length() > 0 || post.length() > 0;
	  label_type lt = label_type(flags & ~(FORCE_LEFT_BRACKET
					       |FORCE_RIGHT_BRACKET));
	  if ((flags & FORCE_LEFT_BRACKET) || !have_text)
	    pending_line += PRE_LABEL_MARKER;
	  pending_line += pre;
	  pending_line += LABEL_MARKER + lt;
	  pending_line += post;
	  if ((flags & FORCE_RIGHT_BRACKET) || !have_text)
	    pending_line += POST_LABEL_MARKER;
	  pending_line += punct;
	  pending_line += '\n';
	}
      }
      need_syncing = 1;
    }
    else if (len >= 4
	     && line[0] == '.' && line[1] == 'l' && line[2] == 'f'
	     && (compatible_flag || line[3] == '\n' || line[3] == ' ')) {
      pending_lf_lines += line;
      line += '\0';
      if (interpret_lf_args(line.contents() + 3))
	current_lineno--;
    }
    else if (recognize_R1_R2
	     && len >= 4
	     && line[0] == '.' && line[1] == 'R' && line[2] == '1'
	     && (compatible_flag || line[3] == '\n' || line[3] == ' ')) {
      line.clear();
      int start_of_line = 1;
      int start_lineno = current_lineno;
      for (;;) {
	int c = getc(fp);
	if (c != EOF && start_of_line)
	  current_lineno++;
	if (start_of_line && c == '.') {
	  c = getc(fp);
	  if (c == 'R') {
	    c = getc(fp);
	    if (c == '2') {
	      c = getc(fp);
	      if (compatible_flag || c == ' ' || c == '\n' || c == EOF) {
		while (c != EOF && c != '\n')
		  c = getc(fp);
		break;
	      }
	      else {
		line += '.';
		line += 'R';
		line += '2';
	      }
	    }
	    else {
	      line += '.';
	      line += 'R';
	    }
	  }
	  else
	    line += '.';
	}
	if (c == EOF) {
	  error_with_file_and_line(current_filename, start_lineno,
				   "missing `.R2' line");
	  break;
	}
	if (illegal_input_char(c))
	  error("illegal input character code %1", int(c));
	else {
	  line += c;
	  start_of_line = c == '\n';
	}
      }
      output_pending_line();
      if (accumulate)
	output_references();
      else
	nreferences = 0;
      process_commands(line, current_filename, start_lineno + 1);
      need_syncing = 1;
    }
    else {
      output_pending_line();
      pending_line = line;
    }
  }
  need_syncing = 0;
  output_pending_line();
  if (fp != stdin)
    fclose(fp);
}

class label_processing_state {
  enum {
    NORMAL,
    PENDING_LABEL,
    PENDING_LABEL_POST,
    PENDING_LABEL_POST_PRE,
    PENDING_POST
    } state;
  label_type type;		// type of pending labels
  int count;			// number of pending labels
  reference **rptr;		// pointer to next reference
  int rcount;			// number of references left
  FILE *fp;
  int handle_pending(int c);
public:
  label_processing_state(reference **, int, FILE *);
  ~label_processing_state();
  void process(int c);
};

static void output_pending_line()
{
  if (label_in_text && !accumulate && ncitations > 0) {
    label_processing_state state(citation, ncitations, outfp);
    int len = pending_line.length();
    for (int i = 0; i < len; i++)
      state.process((unsigned char)(pending_line[i]));
  }
  else
    put_string(pending_line, outfp);
  pending_line.clear();
  if (pending_lf_lines.length() > 0) {
    put_string(pending_lf_lines, outfp);
    pending_lf_lines.clear();
  }
  if (!accumulate)
    immediately_output_references();
  if (need_syncing) {
    fprintf(outfp, ".lf %d %s\n", current_lineno, current_filename);
    need_syncing = 0;
  }
}

static void split_punct(string &line, string &punct)
{
  const char *start = line.contents();
  const char *end = start + line.length();
  const char *ptr = start;
  const char *last_token_start = 0;
  for (;;) {
    if (ptr >= end)
      break;
    last_token_start = ptr;
    if (*ptr == PRE_LABEL_MARKER || *ptr == POST_LABEL_MARKER
	|| (*ptr >= LABEL_MARKER && *ptr < LABEL_MARKER + N_LABEL_TYPES))
      ptr++;
    else if (!get_token(&ptr, end))
      break;
  }
  if (last_token_start) {
    const token_info *ti = lookup_token(last_token_start, end);
    if (ti->is_punct()) {
      punct.append(last_token_start, end - last_token_start);
      line.set_length(last_token_start - start);
    }
  }
}

static void divert_to_temporary_file()
{
  outfp = xtmpfile();
}

static void store_citation(reference *ref)
{
  if (ncitations >= citation_max) {
    if (citation == 0)
      citation = new reference*[citation_max = 100];
    else {
      reference **old_citation = citation;
      citation_max *= 2;
      citation = new reference *[citation_max];
      memcpy(citation, old_citation, ncitations*sizeof(reference *));
      a_delete old_citation;
    }
  }
  citation[ncitations++] = ref;
}

static unsigned store_reference(const string &str)
{
  if (reference_hash_table == 0) {
    reference_hash_table = new reference *[17];
    hash_table_size = 17;
    for (int i = 0; i < hash_table_size; i++)
      reference_hash_table[i] = 0;
  }
  unsigned flags;
  reference *ref = make_reference(str, &flags);
  ref->compute_hash_code();
  unsigned h = ref->hash();
  reference **ptr;
  for (ptr = reference_hash_table + (h % hash_table_size);
       *ptr != 0;
       ((ptr == reference_hash_table)
	? (ptr = reference_hash_table + hash_table_size - 1)
	: --ptr))
    if (same_reference(**ptr, *ref))
      break;
  if (*ptr != 0) {
    if (ref->is_merged())
      warning("fields ignored because reference already used");
    delete ref;
    ref = *ptr;
  }
  else {
    *ptr = ref;
    ref->set_number(nreferences);
    nreferences++;
    ref->pre_compute_label();
    ref->compute_sort_key();
    if (nreferences*2 >= hash_table_size) {
      // Rehash it.
      reference **old_table = reference_hash_table;
      int old_size = hash_table_size;
      hash_table_size = next_size(hash_table_size);
      reference_hash_table = new reference*[hash_table_size];
      int i;
      for (i = 0; i < hash_table_size; i++)
	reference_hash_table[i] = 0;
      for (i = 0; i < old_size; i++)
	if (old_table[i]) {
	  reference **p;
	  for (p = (reference_hash_table
				+ (old_table[i]->hash() % hash_table_size));
	       *p;
	       ((p == reference_hash_table)
		? (p = reference_hash_table + hash_table_size - 1)
		: --p))
	    ;
	  *p = old_table[i];
	}
      a_delete old_table;
    }
  }
  if (label_in_text)
    store_citation(ref);
  return flags;
}

unsigned immediately_handle_reference(const string &str)
{
  unsigned flags;
  reference *ref = make_reference(str, &flags);
  ref->set_number(nreferences);
  if (label_in_text || label_in_reference) {
    ref->pre_compute_label();
    ref->immediate_compute_label();
  }
  nreferences++;
  store_citation(ref);
  return flags;
}

static void immediately_output_references()
{
  for (int i = 0; i < ncitations; i++) {
    reference *ref = citation[i];
    if (label_in_reference) {
      fputs(".ds [F ", outfp);
      const string &label = ref->get_label(NORMAL_LABEL);
      if (label.length() > 0
	  && (label[0] == ' ' || label[0] == '\\' || label[0] == '"'))
	putc('"', outfp);
      put_string(label, outfp);
      putc('\n', outfp);
    }
    ref->output(outfp);
    delete ref;
  }
  ncitations = 0;
}

static void output_citation_group(reference **v, int n, label_type type,
				  FILE *fp)
{
  if (sort_adjacent_labels) {
    // Do an insertion sort.  Usually n will be very small.
    for (int i = 1; i < n; i++) {
      int num = v[i]->get_number();
      reference *temp = v[i];
      int j;
      for (j = i - 1; j >= 0 && v[j]->get_number() > num; j--)
	v[j + 1] = v[j];
      v[j + 1] = temp;
    }
  }
  // This messes up if !accumulate.
  if (accumulate && n > 1) {
    // remove duplicates
    int j = 1;
    for (int i = 1; i < n; i++)
      if (v[i]->get_label(type) != v[i - 1]->get_label(type))
	v[j++] = v[i];
    n = j;
  }
  string merged_label;
  for (int i = 0; i < n; i++) {
    int nmerged = v[i]->merge_labels(v + i + 1, n - i - 1, type, merged_label);
    if (nmerged > 0) {
      put_string(merged_label, fp);
      i += nmerged;
    }
    else
      put_string(v[i]->get_label(type), fp);
    if (i < n - 1)
      put_string(sep_label, fp);
  }
}


label_processing_state::label_processing_state(reference **p, int n, FILE *f)
: state(NORMAL), count(0), rptr(p), rcount(n), fp(f)
{
}

label_processing_state::~label_processing_state()
{
  int handled = handle_pending(EOF);
  assert(!handled);
  assert(rcount == 0);
}

int label_processing_state::handle_pending(int c)
{
  switch (state) {
  case NORMAL:
    break;
  case PENDING_LABEL:
    if (c == POST_LABEL_MARKER) {
      state = PENDING_LABEL_POST;
      return 1;
    }
    else {
      output_citation_group(rptr, count, type, fp);
      rptr += count ;
      rcount -= count;
      state = NORMAL;
    }
    break;
  case PENDING_LABEL_POST:
    if (c == PRE_LABEL_MARKER) {
      state = PENDING_LABEL_POST_PRE;
      return 1;
    }
    else {
      output_citation_group(rptr, count, type, fp);
      rptr += count;
      rcount -= count;
      put_string(post_label, fp);
      state = NORMAL;
    }
    break;
  case PENDING_LABEL_POST_PRE:
    if (c >= LABEL_MARKER
	&& c < LABEL_MARKER + N_LABEL_TYPES
	&& c - LABEL_MARKER == type) {
      count += 1;
      state = PENDING_LABEL;
      return 1;
    }
    else {
      output_citation_group(rptr, count, type, fp);
      rptr += count;
      rcount -= count;
      put_string(sep_label, fp);
      state = NORMAL;
    }
    break;
  case PENDING_POST:
    if (c == PRE_LABEL_MARKER) {
      put_string(sep_label, fp);
      state = NORMAL;
      return 1;
    }
    else {
      put_string(post_label, fp);
      state = NORMAL;
    }
    break;
  }
  return 0;
}

void label_processing_state::process(int c)
{
  if (handle_pending(c))
    return;
  assert(state == NORMAL);
  switch (c) {
  case PRE_LABEL_MARKER:
    put_string(pre_label, fp);
    state = NORMAL;
    break;
  case POST_LABEL_MARKER:
    state = PENDING_POST;
    break;
  case LABEL_MARKER:
  case LABEL_MARKER + 1:
    count = 1;
    state = PENDING_LABEL;
    type = label_type(c - LABEL_MARKER);
    break;
  default:
    state = NORMAL;
    putc(c, fp);
    break;
  }
}

extern "C" {

static int rcompare(const void *p1, const void *p2)
{
  return compare_reference(**(reference **)p1, **(reference **)p2);
}

}

void output_references()
{
  assert(accumulate);
  if (nreferences > 0) {
    int j = 0;
    int i;
    for (i = 0; i < hash_table_size; i++)
      if (reference_hash_table[i] != 0)
	reference_hash_table[j++] = reference_hash_table[i];
    assert(j == nreferences);
    for (; j < hash_table_size; j++)
      reference_hash_table[j] = 0;
    qsort(reference_hash_table, nreferences, sizeof(reference*), rcompare);
    for (i = 0; i < nreferences; i++)
      reference_hash_table[i]->set_number(i);
    compute_labels(reference_hash_table, nreferences);
  }
  if (outfp != stdout) {
    rewind(outfp);
    {
      label_processing_state state(citation, ncitations, stdout);
      int c;
      while ((c = getc(outfp)) != EOF)
	state.process(c);
    }
    ncitations = 0;
    fclose(outfp);
    outfp = stdout;
  }
  if (nreferences > 0) {
    fputs(".]<\n", outfp);
    for (int i = 0; i < nreferences; i++) {
      if (sort_fields.length() > 0)
	reference_hash_table[i]->print_sort_key_comment(outfp);
      if (label_in_reference) {
	fputs(".ds [F ", outfp);
	const string &label = reference_hash_table[i]->get_label(NORMAL_LABEL);
	if (label.length() > 0
	    && (label[0] == ' ' || label[0] == '\\' || label[0] == '"'))
	  putc('"', outfp);
	put_string(label, outfp);
	putc('\n', outfp);
      }
      reference_hash_table[i]->output(outfp);
      delete reference_hash_table[i];
      reference_hash_table[i] = 0;
    }
    fputs(".]>\n", outfp);
    nreferences = 0;
  }
  clear_labels();
}

static reference *find_reference(const char *query, int query_len)
{
  // This is so that error messages look better.
  while (query_len > 0 && csspace(query[query_len - 1]))
    query_len--;
  string str;
  for (int i = 0; i < query_len; i++)
    str += query[i] == '\n' ? ' ' : query[i];
  str += '\0';
  possibly_load_default_database();
  search_list_iterator iter(&database_list, str.contents());
  reference_id rid;
  const char *start;
  int len;
  if (!iter.next(&start, &len, &rid)) {
    error("no matches for `%1'", str.contents());
    return 0;
  }
  const char *end = start + len;
  while (start < end) {
    if (*start == '%')
      break;
    while (start < end && *start++ != '\n')
      ;
  }
  if (start >= end) {
    error("found a reference for `%1' but it didn't contain any fields",
	  str.contents());
    return 0;
  }
  reference *result = new reference(start, end - start, &rid);
  if (iter.next(&start, &len, &rid))
    warning("multiple matches for `%1'", str.contents());
  return result;
}

static reference *make_reference(const string &str, unsigned *flagsp)
{
  const char *start = str.contents();
  const char *end = start + str.length();
  const char *ptr = start;
  while (ptr < end) {
    if (*ptr == '%')
      break;
    while (ptr < end && *ptr++ != '\n')
      ;
  }
  *flagsp = 0;
  for (; start < ptr; start++) {
    if (*start == '#')
      *flagsp = (SHORT_LABEL | (*flagsp & (FORCE_RIGHT_BRACKET
					   | FORCE_LEFT_BRACKET)));
    else if (*start == '[')
      *flagsp |= FORCE_LEFT_BRACKET;
    else if (*start == ']')
      *flagsp |= FORCE_RIGHT_BRACKET;
    else if (!csspace(*start))
      break;
  }
  if (start >= end) {
    error("empty reference");
    return new reference;
  }
  reference *database_ref = 0;
  if (start < ptr)
    database_ref = find_reference(start, ptr - start);
  reference *inline_ref = 0;
  if (ptr < end)
    inline_ref = new reference(ptr, end - ptr);
  if (inline_ref) {
    if (database_ref) {
      database_ref->merge(*inline_ref);
      delete inline_ref;
      return database_ref;
    }
    else
      return inline_ref;
  }
  else if (database_ref)
    return database_ref;
  else
    return new reference;
}

static void do_ref(const string &str)
{
  if (accumulate)
    (void)store_reference(str);
  else {
    (void)immediately_handle_reference(str);
    immediately_output_references();
  }
}

static void trim_blanks(string &str)
{
  const char *start = str.contents();
  const char *end = start + str.length();
  while (end > start && end[-1] != '\n' && csspace(end[-1]))
    --end;
  str.set_length(end - start);
}

void do_bib(const char *filename)
{
  FILE *fp;
  if (strcmp(filename, "-") == 0)
    fp = stdin;
  else {
    errno = 0;
    fp = fopen(filename, "r");
    if (fp == 0) {
      error("can't open `%1': %2", filename, strerror(errno));
      return;
    }
    current_filename = filename;
  }
  enum {
    START, MIDDLE, BODY, BODY_START, BODY_BLANK, BODY_DOT
    } state = START;
  string body;
  for (;;) {
    int c = getc(fp);
    if (c == EOF)
      break;
    if (illegal_input_char(c)) {
      error("illegal input character code %1", c);
      continue;
    }
    switch (state) {
    case START:
      if (c == '%') {
	body = c;
	state = BODY;
      }
      else if (c != '\n')
	state = MIDDLE;
      break;
    case MIDDLE:
      if (c == '\n')
	state = START;
      break;
    case BODY:
      body += c;
      if (c == '\n')
	state = BODY_START;
      break;
    case BODY_START:
      if (c == '\n') {
	do_ref(body);
	state = START;
      }
      else if (c == '.')
	state = BODY_DOT;
      else if (csspace(c)) {
	state = BODY_BLANK;
	body += c;
      }
      else {
	body += c;
	state = BODY;
      }
      break;
    case BODY_BLANK:
      if (c == '\n') {
	trim_blanks(body);
	do_ref(body);
	state = START;
      }
      else if (csspace(c))
	body += c;
      else {
	body += c;
	state = BODY;
      }
      break;
    case BODY_DOT:
      if (c == ']') {
	do_ref(body);
	state = MIDDLE;
      }
      else {
	body += '.';
	body += c;
	state = c == '\n' ? BODY_START : BODY;
      }
      break;
    default:
      assert(0);
    }
    if (c == '\n')
      current_lineno++;
  }
  switch (state) {
  case START:
  case MIDDLE:
    break;
  case BODY:
    body += '\n';
    do_ref(body);
    break;
  case BODY_DOT:
  case BODY_START:
    do_ref(body);
    break;
  case BODY_BLANK:
    trim_blanks(body);
    do_ref(body);
    break;
  }
  fclose(fp);
}

// from the Dragon Book

unsigned hash_string(const char *s, int len)
{
  const char *end = s + len;
  unsigned h = 0, g;
  while (s < end) {
    h <<= 4;
    h += *s++;
    if ((g = h & 0xf0000000) != 0) {
      h ^= g >> 24;
      h ^= g;
    }
  }
  return h;
}

int next_size(int n)
{
  static const int table_sizes[] = { 
    101, 503, 1009, 2003, 3001, 4001, 5003, 10007, 20011, 40009,
    80021, 160001, 500009, 1000003, 2000003, 4000037, 8000009,
    16000057, 32000011, 64000031, 128000003, 0 
  };

  const int *p;
  for (p = table_sizes; *p <= n && *p != 0; p++)
    ;
  assert(*p != 0);
  return *p;
}

