#!/usr/bin/awk -f
#
# Convert forth source files to a giant C string
#
# Joe Abley <jabley@patho.gen.nz>, 12 January 1999
#
# 02-oct-1999:  Cleaned up awk slightly; added some additional logic
#               suggested by dcs to compress the stored forth program.
#
# $FreeBSD$

BEGIN \
{
  printf "/***************************************************************\n";
  printf "** s o f t c o r e . c\n";
  printf "** Forth Inspired Command Language -\n";
  printf "** Words from CORE set written in FICL\n";
  printf "** Author: John Sadler (john_sadler@alum.mit.edu)\n";
  printf "** Created: 27 December 1997\n";
  printf "** Last update: %s\n", datestamp;
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
  gsub(/\t/, "    ");			# replace each tab with 4 spaces
  gsub(/\"/, "\\\"");			# escape quotes
  gsub(/\\[ 	]+$/, "");		# toss empty comments
}

# strip out empty lines
/^ *$/ \
{
  next;
}

# emit / ** lines as multi-line C comments
/^\\[ 	]\*\*/ \
{
  sub(/^\\[ 	]/, "");
  if (commenting == 0) printf "/*\n";
  printf "%s\n", $0;
  commenting = 1;
  next;
}

# strip blank lines
/^[ 	]*$/ \
{
  next;
}

# function to close a comment, used later
function end_comments()
{
  commenting = 0;
  printf "*/\n";
}

# pass commented preprocessor directives
/^\\[ 	]#/ \
{
  if (commenting) end_comments();
  sub(/^\\[ 	]/, "");
  printf "%s\n", $0;
  next;
}

# toss all other full-line \ comments
/^\\/ \
{
  if (commenting) end_comments();
  next;
}

# lop off trailing \ comments
/\\[ 	]+/ \
{
  sub(/\\[ 	]+.*$/, "");
}

# expunge ( ) comments
/[ 	]+\([ 	][^)]*\)/ \
{
  sub(/[ 	]+\([ 	][^)]*\)/, "");
}

# remove leading spaces
/^[ 	]+/ \
{
  sub(/^[ 	]+/, "");
}

# removing trailing spaces
/[ 	]+$/ \
{
  sub(/[ 	]+$/, "");
}

# strip out empty lines again (preceding rules may have generated some)
/^[ 	]*$/ \
{
  if (commenting) end_comments();
  next;
}

# emit all other lines as quoted string fragments
{
  if (commenting) end_comments();

  printf "    \"%s \"\n", $0;
  next;
}

END \
{
  if (commenting) end_comments();
  printf "    \"quit \";\n";
  printf "\n\nvoid ficlCompileSoftCore(FICL_SYSTEM *pSys)\n";
  printf "{\n";
  printf "    FICL_VM *pVM = pSys->vmList;\n";
  printf "    int ret = sizeof (softWords);\n";
  printf "	  assert(pVM);\n";
  printf "\n"
  printf "    ret = ficlExec(pVM, softWords);\n";
  printf "    if (ret == VM_ERREXIT)\n";
  printf "        assert(FALSE);\n";
  printf "    return;\n";
  printf "}\n";
}
