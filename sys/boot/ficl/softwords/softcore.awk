#!/usr/bin/awk -f
# Convert forth source files to a giant C string
# Joe Abley <jabley@patho.gen.nz>, 12 January 1999
# $FreeBSD$

BEGIN \
{
  printf "/***************************************************************\n";
  printf "** s o f t c o r e . c\n";
  printf "** Forth Inspired Command Language -\n";
  printf "** Words from CORE set written in FICL\n";
  printf "** Author: John Sadler (john_sadler@alum.mit.edu)\n";
  printf "** Created: 27 December 1997\n";
  printf "** Last update: %s\n", strftime();
  printf "***************************************************************/\n";
  printf "\n/*\n";
  printf "** This file contains definitions that are compiled into the\n";
  printf "** system dictionary by the first virtual machine to be created.\n";
  printf "** Created automagically by ficl/softwords/softcore.awk\n";
  printf "*/\n";
  printf "\n#include \"ficl.h\"\n";
  printf "\nstatic char softWords[] =\n";

  commenting = 0;
}

# some general early substitutions
{
  gsub("\t", "    ");			# replace each tab with 4 spaces
  gsub("\"", "\\\"");			# escape quotes
  gsub("\\\\[[:space:]]+$", "");	# toss empty comments
}

# strip out empty lines
/^ *$/ \
{
  next;
}

# emit / ** lines as multi-line C comments
/^\\[[:space:]]\*\*/ && (commenting == 0) \
{
  sub("^\\\\[[:space:]]", "");
  printf "/*\n%s\n", $0;
  commenting = 1;
  next;
}

/^\\[[:space:]]\*\*/ \
{
  sub("^\\\\[[:space:]]", "");
  printf "%s\n", $0;
  next;
}

# function to close a comment, used later
function end_comments()
{
  commenting = 0;
  printf "*/\n";
}

# pass commented preprocessor directives
/^\\[[:space:]]#/ \
{
  if (commenting) end_comments();
  sub("^\\\\[[:space:]]", "");
  printf "%s\n", $0;
  next;
}

# toss all other full-line comments
/^\\/ \
{
  if (commenting) end_comments();
  next;
}

# emit all other lines as quoted string fragments
{
  if (commenting) end_comments();

  sub("\\\\[[:space:]]+.*$", "");	# lop off trailing \ comments
  sub("[[:space:]]+$", "");		# remove trailing spaces
  printf "    \"%s \\n\"\n", $0;
  next;
}

END \
{
  if (commenting) end_comments();
  printf "    \"quit \";\n";
  printf "\n\nvoid ficlCompileSoftCore(FICL_VM *pVM)\n";
  printf "{\n";
  printf "    assert(ficlExec(pVM, softWords) != VM_ERREXIT);\n";
  printf "}\n";
}
