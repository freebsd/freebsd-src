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
Foundation, 675 Mass Ave, Cambridge, MA 02139, USA. */

#include "driver.h"
#include "stringclass.h"
#include "cset.h"

#include "ps.h"

#define PROLOGUE "prologue"

static void print_ps_string(const string &s, FILE *outfp);

cset white_space("\n\r \t");
string an_empty_string;

const char *extension_table[] = {
  "DPS",
  "CMYK",
  "Composite",
  "FileSystem",
};

const int NEXTENSIONS = sizeof(extension_table)/sizeof(extension_table[0]);

const char *resource_table[] = {
  "font",
  "procset",
  "file",
  "encoding",
  "form",
  "pattern",
};

const int NRESOURCES = sizeof(resource_table)/sizeof(resource_table[0]);

struct resource {
  resource *next;
  resource_type type;
  string name;
  enum { NEEDED = 01, SUPPLIED = 02, FONT_NEEDED = 04, BUSY = 010 };
  unsigned flags;
  string version;
  unsigned revision;
  char *filename;
  int rank;
  resource(resource_type, string &, string & = an_empty_string, unsigned = 0);
  ~resource();
  void print_type_and_name(FILE *outfp);
};

resource::resource(resource_type t, string &n, string &v, unsigned r)
: type(t), revision(r), flags (0), filename(0), rank(-1), next(0)
{
  name.move(n);
  version.move(v);
  if (type == RESOURCE_FILE) {
    if (name.search('\0') >= 0)
      error("filename contains a character with code 0");
    filename = name.extract();
  }
}

resource::~resource()
{
  a_delete filename;
}

void resource::print_type_and_name(FILE *outfp)
{
  fputs(resource_table[type], outfp);
  putc(' ', outfp);
  print_ps_string(name, outfp);
  if (type == RESOURCE_PROCSET) {
    putc(' ', outfp);
    print_ps_string(version, outfp);
    fprintf(outfp, " %u", revision);
  }
}

resource_manager::resource_manager()
: resource_list(0), extensions(0), language_level(0)
{
  read_download_file();
  string procset_name("grops");
  extern const char *version_string;
  string procset_version(version_string);
  procset_resource = lookup_resource(RESOURCE_PROCSET, procset_name,
				     procset_version, 0);
  procset_resource->flags |= resource::SUPPLIED;
}

resource_manager::~resource_manager()
{
  while (resource_list) {
    resource *tem = resource_list;
    resource_list = resource_list->next;
    delete tem;
  }
}

resource *resource_manager::lookup_resource(resource_type type,
					    string &name,
					    string &version,
					    unsigned revision)
{
  for (resource *r = resource_list; r; r = r->next)
    if (r->type == type
	&& r->name == name
	&& r->version == version
	&& r->revision == revision)
      return r;
  r = new resource(type, name, version, revision);
  r->next = resource_list;
  resource_list = r;
  return r;
}

// Just a specialized version of lookup_resource().

resource *resource_manager::lookup_font(const char *name)
{
  for (resource *r = resource_list; r; r = r->next)
    if (r->type == RESOURCE_FONT
	&& strlen(name) == r->name.length()
	&& memcmp(name, r->name.contents(), r->name.length()) == 0)
      return r;
  string s(name);
  r = new resource(RESOURCE_FONT, s);
  r->next = resource_list;
  resource_list = r;
  return r;
}

void resource_manager::need_font(const char *name)
{
  lookup_font(name)->flags |= resource::FONT_NEEDED;
}

typedef resource *Presource;	// Work around g++ bug.

void resource_manager::document_setup(ps_output &out)
{
  int nranks = 0;
  for (resource *r = resource_list; r; r = r->next)
    if (r->rank >= nranks)
      nranks = r->rank + 1;
  if (nranks > 0) {
    // Sort resource_list in reverse order of rank.
    Presource *head = new Presource[nranks + 1];
    Presource **tail = new Presource *[nranks + 1];
    for (int i = 0; i < nranks + 1; i++) {
      head[i] = 0;
      tail[i] = &head[i];
    }
    for (r = resource_list; r; r = r->next) {
      i = r->rank < 0 ? 0 : r->rank + 1;
      *tail[i] = r;
      tail[i] = &(*tail[i])->next;
    }
    resource_list = 0;
    for (i = 0; i < nranks + 1; i++)
      if (head[i]) {
	*tail[i] = resource_list;
	resource_list = head[i];
      }
    a_delete head;
    a_delete tail;
    // check it
    for (r = resource_list; r; r = r->next)
      if (r->next)
	assert(r->rank >= r->next->rank);
    for (r = resource_list; r; r = r->next)
      if (r->type == RESOURCE_FONT && r->rank >= 0)
	supply_resource(r, -1, out.get_file());
  }
}

void resource_manager::print_resources_comment(unsigned flag, FILE *outfp)
{
  int continued = 0;
  for (resource *r = resource_list; r; r = r->next)
    if (r->flags & flag) {
      if (continued)
	fputs("%%+ ", outfp);
      else {
	fputs(flag == resource::NEEDED
	      ? "%%DocumentNeededResources: "
	      : "%%DocumentSuppliedResources: ",
	      outfp);
	continued = 1;
      }
      r->print_type_and_name(outfp);
      putc('\n', outfp);
    }
}

void resource_manager::print_header_comments(ps_output &out)
{
  for (resource *r = resource_list; r; r = r->next)
    if (r->type == RESOURCE_FONT && (r->flags & resource::FONT_NEEDED))
      supply_resource(r, 0, 0);
  print_resources_comment(resource::NEEDED, out.get_file());
  print_resources_comment(resource::SUPPLIED, out.get_file());
  print_language_level_comment(out.get_file());
  print_extensions_comment(out.get_file());
}

void resource_manager::output_prolog(ps_output &out)
{
  FILE *outfp = out.get_file();
  out.end_line();
  char *path;
  FILE *fp = font::open_file(PROLOGUE, &path);
  if (!fp)
    fatal("can't find `%1'", PROLOGUE);
  fputs("%%BeginResource: ", outfp);
  procset_resource->print_type_and_name(outfp);
  putc('\n', outfp);
  process_file(-1, fp, path, outfp);
  fclose(fp);
  a_delete path;
  fputs("%%EndResource\n", outfp);
}

void resource_manager::import_file(const char *filename, ps_output &out)
{
  out.end_line();
  string name(filename);
  resource *r = lookup_resource(RESOURCE_FILE, name);
  supply_resource(r, -1, out.get_file(), 1);
}

void resource_manager::supply_resource(resource *r, int rank, FILE *outfp,
				       int is_document)
{
  if (r->flags & resource::BUSY) {
    r->name += '\0';
    fatal("loop detected in dependency graph for %1 `%2'",
	  resource_table[r->type],
	  r->name.contents());
  }
  r->flags |= resource::BUSY;
  if (rank > r->rank)
    r->rank = rank;
  char *path;
  FILE *fp = 0;
  if (r->filename != 0) {
    if (r->type == RESOURCE_FONT) {
      fp = font::open_file(r->filename, &path);
      if (!fp) {
	error("can't find `%1'", r->filename);
	a_delete r->filename;
	r->filename = 0;
      }
    }
    else {
      errno = 0;
      fp = fopen(r->filename, "r");
      if (!fp) {
	error("can't open `%1': %2", r->filename, strerror(errno));
	a_delete r->filename;
	r->filename = 0;
      }
      else
	path = r->filename;
    }
  }
  if (fp) {
    if (outfp) {
      if (r->type == RESOURCE_FILE && is_document) {
	fputs("%%BeginDocument: ", outfp);
	print_ps_string(r->name, outfp);
	putc('\n', outfp);
      }
      else {
	fputs("%%BeginResource: ", outfp);
	r->print_type_and_name(outfp);
	putc('\n', outfp);
      }
    }
    process_file(rank, fp, path, outfp);
    fclose(fp);
    if (r->type == RESOURCE_FONT)
      a_delete path;
    if (outfp) {
      if (r->type == RESOURCE_FILE && is_document)
	fputs("%%EndDocument\n", outfp);
      else
	fputs("%%EndResource\n", outfp);
    }
    r->flags |= resource::SUPPLIED;
  }
  else {
    if (outfp) {
      if (r->type == RESOURCE_FILE && is_document) {
	fputs("%%IncludeDocument: ", outfp);
	print_ps_string(r->name, outfp);
	putc('\n', outfp);
      }
      else {
	fputs("%%IncludeResource: ", outfp);
	r->print_type_and_name(outfp);
	putc('\n', outfp);
      }
    }
    r->flags |= resource::NEEDED;
  }
  r->flags &= ~resource::BUSY;
}


#define PS_LINE_MAX 255
#define PS_MAGIC "%!PS-Adobe-"

static int ps_get_line(char *buf, FILE *fp)
{
  int c = getc(fp);
  if (c == EOF) {
    buf[0] = '\0';
    return 0;
  }
  current_lineno++;
  int i = 0;
  int err = 0;
  while (c != '\r' && c != '\n' && c != EOF) {
    if ((c < 0x1b && !white_space(c)) || c == 0x7f)
      error("illegal input character code %1", int(c));
    else if (i < PS_LINE_MAX)
      buf[i++] = c;
    else if (!err) {
      err = 1;
      error("PostScript file non-conforming "
	    "because length of line exceeds 255");
    }
    c = getc(fp);
  }
  buf[i++] = '\n';
  buf[i] = '\0';
  if (c == '\r') {
    c = getc(fp);
    if (c != EOF && c != '\n')
      ungetc(c, fp);
  }
  return 1;
}

static int read_text_arg(const char **pp, string &res)
{
  res.clear();
  while (white_space(**pp))
    *pp += 1;
  if (**pp == '\0') {
    error("missing argument");
    return 0;
  }
  if (**pp != '(') {
    for (; **pp != '\0' && !white_space(**pp); *pp += 1)
      res += **pp;
    return 1;
  }
  *pp += 1;
  res.clear();
  int level = 0;
  for (;;) {
    if (**pp == '\0' || **pp == '\r' || **pp == '\n') {
      error("missing ')'");
      return 0;
    }
    if (**pp == ')') {
      if (level == 0) {
	*pp += 1;
	break;
      }
      res += **pp;
      level--;
    }
    else if (**pp == '(') {
      level++;
      res += **pp;
    }
    else if (**pp == '\\') {
      *pp += 1;
      switch (**pp) {
      case 'n':
	res += '\n';
	break;
      case 'r':
	res += '\n';
	break;
      case 't':
	res += '\t';
	break;
      case 'b':
	res += '\b';
	break;
      case 'f':
	res += '\f';
	break;
      case '0':
      case '1':
      case '2':
      case '3':
      case '4':
      case '5':
      case '6':
      case '7':
	{
	  int val = **pp - '0';
	  if ((*pp)[1] >= '0' && (*pp)[1] <= '7') {
	    *pp += 1;
	    val = val*8 + (**pp - '0');
	    if ((*pp)[1] >= '0' && (*pp)[1] <= '7') {
	      *pp += 1;
	      val = val*8 + (**pp - '0');
	    }
	  }
	}
	break;
      default:
	res += **pp;
	break;
      }
    }
    else
      res += **pp;
    *pp += 1;
  }
  return 1;
}

static int read_uint_arg(const char **pp, unsigned *res)
{
  while (white_space(**pp))
    *pp += 1;
  if (**pp == '\0') {
    error("missing argument");
    return 0;
  }
  const char *start = *pp;
  // XXX use strtoul
  long n = strtol(start, (char **)pp, 10);
  if (n == 0 && *pp == start) {
    error("not an integer");
    return 0;
  }
  if (n < 0) {
    error("argument must not be negative");
    return 0;
  }
  *res = unsigned(n);
  return 1;
}

resource *resource_manager::read_file_arg(const char **ptr)
{
  string arg;
  if (!read_text_arg(ptr, arg))
    return 0;
  return lookup_resource(RESOURCE_FILE, arg);
}

resource *resource_manager::read_font_arg(const char **ptr)
{
  string arg;
  if (!read_text_arg(ptr, arg))
    return 0;
  return lookup_resource(RESOURCE_FONT, arg);
}

resource *resource_manager::read_procset_arg(const char **ptr)
{
  string arg;
  if (!read_text_arg(ptr, arg))
    return 0;
  string version;
  if (!read_text_arg(ptr, version))
      return 0;
  unsigned revision;
  if (!read_uint_arg(ptr, &revision))
      return 0;
  return lookup_resource(RESOURCE_PROCSET, arg, version, revision);
}

resource *resource_manager::read_resource_arg(const char **ptr)
{
  while (white_space(**ptr))
    *ptr += 1;
  const char *name = *ptr;
  while (**ptr != '\0' && !white_space(**ptr))
    *ptr += 1;
  if (name == *ptr) {
    error("missing resource type");
    return 0;
  }
  for (int ri = 0; ri < NRESOURCES; ri++)
    if (strlen(resource_table[ri]) == *ptr - name
	&& memcmp(resource_table[ri], name, *ptr - name) == 0)
      break;
  if (ri >= NRESOURCES) {
    error("unknown resource type");
    return 0;
  }
  if (ri == RESOURCE_PROCSET)
    return read_procset_arg(ptr);
  string arg;
  if (!read_text_arg(ptr, arg))
    return 0;
  return lookup_resource(resource_type(ri), arg);
}

static const char *matches_comment(const char *buf, const char *comment)
{
  if (buf[0] != '%' || buf[1] != '%')
    return 0;
  for (buf += 2; *comment; comment++, buf++)
    if (*buf != *comment)
      return 0;
  if (comment[-1] == ':')
    return buf;
  if (*buf == '\0' || white_space(*buf))
    return buf;
  return 0;
}

// Return 1 if the line should be copied out.

int resource_manager::do_begin_resource(const char *ptr, int, FILE *,
					FILE *)
{
  resource *r = read_resource_arg(&ptr);
  if (r)
    r->flags |= resource::SUPPLIED;
  return 1;
}

int resource_manager::do_include_resource(const char *ptr, int rank, FILE *,
					  FILE *outfp)
{
  resource *r = read_resource_arg(&ptr);
  if (r) {
    if (r->type == RESOURCE_FONT) {
      if (rank >= 0)
	supply_resource(r, rank + 1, outfp);
      else
	r->flags |= resource::FONT_NEEDED;
    }
    else
      supply_resource(r, rank, outfp);
  }
  return 0;
}

int resource_manager::do_begin_document(const char *ptr, int, FILE *,
					FILE *)
{
  resource *r = read_file_arg(&ptr);
  if (r)
    r->flags |= resource::SUPPLIED;
  return 1;
}

int resource_manager::do_include_document(const char *ptr, int rank, FILE *,
					  FILE *outfp)
{
  resource *r = read_file_arg(&ptr);
  if (r)
    supply_resource(r, rank, outfp, 1);
  return 0;
}

int resource_manager::do_begin_procset(const char *ptr, int, FILE *,
				       FILE *outfp)
{
  resource *r = read_procset_arg(&ptr);
  if (r) {
    r->flags |= resource::SUPPLIED;
    if (outfp) {
      fputs("%%BeginResource: ", outfp);
      r->print_type_and_name(outfp);
      putc('\n', outfp);
    }
  }
  return 0;
}

int resource_manager::do_include_procset(const char *ptr, int rank, FILE *,
					  FILE *outfp)
{
  resource *r = read_procset_arg(&ptr);
  if (r)
    supply_resource(r, rank, outfp);
  return 0;
}

int resource_manager::do_begin_file(const char *ptr, int, FILE *,
				    FILE *outfp)
{
  resource *r = read_file_arg(&ptr);
  if (r) {
    r->flags |= resource::SUPPLIED;
    if (outfp) {
      fputs("%%BeginResource: ", outfp);
      r->print_type_and_name(outfp);
      putc('\n', outfp);
    }
  }
  return 0;
}

int resource_manager::do_include_file(const char *ptr, int rank, FILE *,
				      FILE *outfp)
{
  resource *r = read_file_arg(&ptr);
  if (r)
    supply_resource(r, rank, outfp);
  return 0;
}

int resource_manager::do_begin_font(const char *ptr, int, FILE *,
				    FILE *outfp)
{
  resource *r = read_font_arg(&ptr);
  if (r) {
    r->flags |= resource::SUPPLIED;
    if (outfp) {
      fputs("%%BeginResource: ", outfp);
      r->print_type_and_name(outfp);
      putc('\n', outfp);
    }
  }
  return 0;
}

int resource_manager::do_include_font(const char *ptr, int rank, FILE *,
				      FILE *outfp)
{
  resource *r = read_font_arg(&ptr);
  if (r) {
    if (rank >= 0)
      supply_resource(r, rank + 1, outfp);
    else
      r->flags |= resource::FONT_NEEDED;
  }
  return 0;
}

int resource_manager::change_to_end_resource(const char *, int, FILE *,
					     FILE *outfp)
{
  if (outfp)
    fputs("%%EndResource\n", outfp);
  return 0;
}

int resource_manager::do_begin_preview(const char *, int, FILE *fp, FILE *)
{
  char buf[PS_LINE_MAX + 2];
  do {
    if (!ps_get_line(buf, fp)) {
      error("end of file in preview section");
      break;
    }
  } while (!matches_comment(buf, "EndPreview"));
  return 0;
}

int read_one_of(const char **ptr, const char **s, int n)
{
  while (white_space(**ptr))
    *ptr += 1;
  if (**ptr == '\0')
    return -1;
  const char *start = *ptr;
  do {
    ++ptr;
  } while (**ptr != '\0' && !white_space(**ptr));
  for (int i = 0; i < n; i++)
    if (strlen(s[i]) == *ptr - start
	&& memcmp(s[i], start, *ptr - start) == 0)
      return i;
  return -1;
}

int resource_manager::do_begin_data(const char *ptr, int, FILE *fp,
				    FILE *outfp)
{
  while (white_space(*ptr))
    ptr++;
  const char *start = ptr;
  unsigned numberof;
  if (!read_uint_arg(&ptr, &numberof))
    return 0;
  static const char *types[] = { "Binary", "Hex", "ASCII" };
  const int Binary = 0;
  int type = 0;
  static const char *units[] = { "Bytes", "Lines" };
  const int Bytes = 0;
  int unit = Bytes;
  while (white_space(*ptr))
    ptr++;
  if (*ptr != '\0') {
    type = read_one_of(&ptr, types, 3);
    if (type < 0) {
      error("bad data type");
      return 0;
    }
    while (white_space(*ptr))
      ptr++;
    if (*ptr != '\0') {
      unit = read_one_of(&ptr, units, 2);
      if (unit < 0) {
	error("expected `Bytes' or `Lines'");
	return 0;
      }
    }
  }
  if (type != Binary)
    return 1;
  if (outfp) {
    fputs("%%BeginData: ", outfp);
    fputs(start, outfp);
  }
  if (numberof > 0) {
    unsigned bytecount = 0;
    unsigned linecount = 0;
    do {
      int c = getc(fp);
      if (c == EOF) {
	error("end of file within data section");
	return 0;
      }
      if (outfp)
	putc(c, outfp);
      bytecount++;
      if (c == '\r') {
	int cc = getc(fp);
	if (cc != '\n') {
	  linecount++;
	  current_lineno++;
	}
	if (cc != EOF)
	  ungetc(c, fp);
      }
      else if (c == '\n') {
	linecount++;
	current_lineno++;
      }
    } while ((unit == Bytes ? bytecount : linecount) < numberof);
  }
  char buf[PS_LINE_MAX + 2];
  if (!ps_get_line(buf, fp)) {
    error("missing %%%%EndData line");
    return 0;
  }
  if (!matches_comment(buf, "EndData"))
    error("bad %%%%EndData line");
  if (outfp)
    fputs(buf, outfp);
  return 0;
}

int resource_manager::do_begin_binary(const char *ptr, int, FILE *fp,
				      FILE *outfp)
{
  if (!outfp)
    return 0;
  unsigned count;
  if (!read_uint_arg(&ptr, &count))
    return 0;
  if (outfp)
    fprintf(outfp, "%%%%BeginData: %u Binary Bytes\n", count);
  while (count != 0) {
    int c = getc(fp);
    if (c == EOF) {
      error("end of file within binary section");
      return 0;
    }
    if (outfp)
      putc(c, outfp);
    --count;
    if (c == '\r') {
      int cc = getc(fp);
      if (cc != '\n')
	current_lineno++;
      if (cc != EOF)
	ungetc(c, fp);
    }
    else if (c == '\n')
      current_lineno++;
  }
  char buf[PS_LINE_MAX + 2];
  if (!ps_get_line(buf, fp)) {
    error("missing %%%%EndBinary line");
    return 0;
  }
  if (!matches_comment(buf, "EndBinary")) {
    error("bad %%%%EndBinary line");
    if (outfp)
      fputs(buf, outfp);
  }
  else if (outfp)
    fputs("%%EndData\n", outfp);
  return 0;
}

static unsigned parse_extensions(const char *ptr)
{
  unsigned flags = 0;
  for (;;) {
    while (white_space(*ptr))
      ptr++;
    if (*ptr == '\0')
      break;
    const char *name = ptr;
    do {
      ++ptr;
    } while (*ptr != '\0' && !white_space(*ptr));
    for (int i = 0; i < NEXTENSIONS; i++)
      if (strlen(extension_table[i]) == ptr - name
	  && memcmp(extension_table[i], name, ptr - name) == 0) {
	flags |= (1 << i);
	break;
      }
    if (i >= NEXTENSIONS) {
      string s(name, ptr - name);
      s += '\0';
      error("unknown extension `%1'", s.contents());
    }
  }
  return flags;
}

// XXX if it has not been surrounded with {Begin,End}Document need to strip
// out Page: Trailer {Begin,End}Prolog {Begin,End}Setup sections.

// XXX Perhaps the decision whether to use BeginDocument or
// BeginResource: file should be postponed till we have seen
// the first line of the file.

void resource_manager::process_file(int rank, FILE *fp, const char *filename,
				    FILE *outfp)
{
  // If none of these comments appear in the header section, and we are
  // just analyzing the file (ie outfp is 0), then we can return immediately.
  static const char *header_comment_table[] = {
    "DocumentNeededResources:",
    "DocumentSuppliedResources:",
    "DocumentNeededFonts:",
    "DocumentSuppliedFonts:",
    "DocumentNeededProcSets:",
    "DocumentSuppliedProcSets:",
    "DocumentNeededFiles:",
    "DocumentSuppliedFiles:",
  };
  
  const int NHEADER_COMMENTS = (sizeof(header_comment_table)
				/ sizeof(header_comment_table[0]));
  typedef (resource_manager::*resource_manager_mfp)(const char *, int, FILE *, FILE *);
  struct comment_info {
    const char *name;
    resource_manager_mfp proc;
  };

  static comment_info comment_table[] = {
    "BeginResource:", &resource_manager::do_begin_resource,
    "IncludeResource:", &resource_manager::do_include_resource,
    "BeginDocument:", &resource_manager::do_begin_document,
    "IncludeDocument:", &resource_manager::do_include_document,
    "BeginProcSet:", &resource_manager::do_begin_procset,
    "IncludeProcSet:", &resource_manager::do_include_procset,
    "BeginFont:", &resource_manager::do_begin_font,
    "IncludeFont:", &resource_manager::do_include_font,
    "BeginFile:", &resource_manager::do_begin_file,
    "IncludeFile:", &resource_manager::do_include_file,
    "EndProcSet", &resource_manager::change_to_end_resource,
    "EndFont", &resource_manager::change_to_end_resource,
    "EndFile", &resource_manager::change_to_end_resource,
    "BeginPreview:", &resource_manager::do_begin_preview,
    "BeginData:", &resource_manager::do_begin_data,
    "BeginBinary:", &resource_manager::do_begin_binary,
  };
  
  const int NCOMMENTS = sizeof(comment_table)/sizeof(comment_table[0]);
  char buf[PS_LINE_MAX + 2];
  int saved_lineno = current_lineno;
  const char *saved_filename = current_filename;
  current_filename = filename;
  current_lineno = 0;
  if (!ps_get_line(buf, fp)) {
    current_filename = saved_filename;
    current_lineno = saved_lineno;
    return;
  }
  if (strlen(buf) < sizeof(PS_MAGIC) - 1
      || memcmp(buf, PS_MAGIC, sizeof(PS_MAGIC) - 1) != 0) {
    if (outfp) {
      do {
	if (!(broken_flags & STRIP_PERCENT_BANG)
	    || buf[0] != '%' || buf[1] != '!')
	  fputs(buf, outfp);
      } while (ps_get_line(buf, fp));
    }
  }
  else {
    if (!(broken_flags & STRIP_PERCENT_BANG) && outfp)
      fputs(buf, outfp);
    int in_header = 1;
    int interesting = 0;
    int had_extensions_comment = 0;
    int had_language_level_comment = 0;
    for (;;) {
      if (!ps_get_line(buf, fp))
	break;
      int copy_this_line = 1;
      if (buf[0] == '%') {
	if (buf[1] == '%') {
	  const char *ptr;
	  for (int i = 0; i < NCOMMENTS; i++)
	    if (ptr = matches_comment(buf, comment_table[i].name)) {
	      copy_this_line
		= (this->*(comment_table[i].proc))(ptr, rank, fp, outfp);
	      break;
	    }
	  if (i >= NCOMMENTS && in_header) {
	    if (ptr = matches_comment(buf, "EndComments"))
	      in_header = 0;
	    else if (!had_extensions_comment
		     && (ptr = matches_comment(buf, "Extensions:"))) {
	      extensions |= parse_extensions(ptr);
	      // XXX handle possibility that next line is %%+
	      had_extensions_comment = 1;
	    }
	    else if (!had_language_level_comment
		     && (ptr = matches_comment(buf, "LanguageLevel:"))) {
	      unsigned ll;
	      if (read_uint_arg(&ptr, &ll) && ll > language_level)
		language_level = ll;
	      had_language_level_comment = 1;
	    }
	    else {
	      for (int i = 0; i < NHEADER_COMMENTS; i++)
		if (matches_comment(buf, header_comment_table[i])) {
		  interesting = 1;
		  break;
		}
	    }
	  }
	  if ((broken_flags & STRIP_STRUCTURE_COMMENTS)
	      && (matches_comment(buf, "EndProlog")
		  || matches_comment(buf, "Page:")
		  || matches_comment(buf, "Trailer")))
	    copy_this_line = 0;
	}
	else if (buf[1] == '!') {
	  if (broken_flags & STRIP_PERCENT_BANG)
	    copy_this_line = 0;
	}
      }
      else
	in_header = 0;
      if (!outfp && !in_header && !interesting)
	break;
      if (copy_this_line && outfp)
	fputs(buf, outfp);
    }
  }
  current_filename = saved_filename;
  current_lineno = saved_lineno;
}

void resource_manager::read_download_file()
{
  char *path = 0;
  FILE *fp = font::open_file("download", &path);
  if (!fp)
    fatal("can't find `download'");
  char buf[512];
  int lineno = 0;
  while (fgets(buf, sizeof(buf), fp)) {
    lineno++;
    char *p = strtok(buf, " \t\r\n");
    if (p == 0 || *p == '#')
      continue;
    char *q = strtok(0, " \t\r\n");
    if (!q)
      fatal_with_file_and_line(path, lineno, "missing filename");
    lookup_font(p)->filename = strsave(q);
  }
  a_delete path;
  fclose(fp);
}

// XXX Can we share some code with ps_output::put_string()?

static void print_ps_string(const string &s, FILE *outfp)
{
  int len = s.length();
  const char *str = s.contents();
  int funny = 0;
  if (str[0] == '(')
    funny = 1;
  else {
    for (int i = 0; i < len; i++)
      if (str[i] <= 040 || str[i] > 0176) {
	funny = 1;
	break;
      }
  }
  if (!funny) {
    put_string(s, outfp);
    return;
  }
  int level = 0;
  for (int i = 0; i < len; i++)
    if (str[i] == '(')
      level++;
    else if (str[i] == ')' && --level < 0)
      break;
  putc('(', outfp);
  for (i = 0; i < len; i++)
    switch (str[i]) {
    case '(':
    case ')':
      if (level != 0)
	putc('\\', outfp);
      putc(str[i], outfp);
      break;
    case '\\':
      fputs("\\\\", outfp);
      break;
    case '\n':
      fputs("\\n", outfp);
      break;
    case '\r':
      fputs("\\r", outfp);
      break;
    case '\t':
      fputs("\\t", outfp);
      break;
    case '\b':
      fputs("\\b", outfp);
      break;
    case '\f':
      fputs("\\f", outfp);
      break;
    default:
      if (str[i] < 040 || str[i] > 0176)
	fprintf(outfp, "\\%03o", str[i] & 0377);
      else
	putc(str[i], outfp);
      break;
    }
  putc(')', outfp);
}

void resource_manager::print_extensions_comment(FILE *outfp)
{
  if (extensions) {
    fputs("%%Extensions:", outfp);
    for (int i = 0; i < NEXTENSIONS; i++)
      if (extensions & (1 << i)) {
	putc(' ', outfp);
	fputs(extension_table[i], outfp);
      }
    putc('\n', outfp);
  }
}

void resource_manager::print_language_level_comment(FILE *outfp)
{
  if (language_level)
    fprintf(outfp, "%%%%LanguageLevel: %u\n", language_level);
}

