#! /bin/sh
# ------------------------------------------------------------------------------
#
# Function: Format PDF Output from groff Markup
#
# Copyright (C) 2005, Free Software Foundation, Inc.
# Written by Keith Marshall (keith.d.marshall@ntlworld.com)
# 
# This file is part of groff.
# 
# groff is free software; you can redistribute it and/or modify it under
# the terms of the GNU General Public License as published by the Free
# Software Foundation; either version 2, or (at your option) any later
# version.
# 
# groff is distributed in the hope that it will be useful, but WITHOUT ANY
# WARRANTY; without even the implied warranty of MERCHANTABILITY or
# FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
# for more details.
# 
# You should have received a copy of the GNU General Public License along
# with groff; see the file COPYING.  If not, write to the Free Software
# Foundation, 51 Franklin St - Fifth Floor, Boston, MA 02110-1301, USA.
#
# ------------------------------------------------------------------------------
#
# Set up an identifier for the NULL device.
# In most cases "/dev/null" will be correct, but some shells on
# MS-DOS/MS-Windows systems may require us to use "NUL".
#
  NULLDEV="/dev/null"
  test -c $NULLDEV || NULLDEV="NUL"
#
# Set up the command name to use in diagnostic messages.
# (We can't assume we have 'basename', so use the full path if required.
#  Also use the 'exec 2>...' workaround for a bug in Cygwin's 'ash').
#
  CMD=`exec 2>$NULLDEV; basename $0` || CMD=$0
#
# To ensure that prerequisite helper programs are available, and are
# executable, a [fairly] portable method of detecting such programs is
# provided by function `searchpath'.
#
  searchpath(){
  #
  # Usage:  searchpath progname path
  #
    IFS="${PATH_SEPARATOR-":"}" prog=':'
    for dir in $2
    do
      for ext in '' '.exe'
      #
      # try `progname' with all well known extensions
      # (e.g. Win32 may require `progname.exe')
      #
      do
        try="$dir/$1$ext"
        test -f "$try" && test -x "$try" && prog="$try" && break
      done
      test "$prog" = ":" || break
    done
    echo "$prog"
  }
# @PATH_SEARCH_SETUP@
#
# We need both 'grep' and 'sed' programs, to parse script options,
# and we also need 'cat', to display help and some error messages,
# so ensure they are all installed, before we continue.
#
  CAT=`searchpath cat "$PATH"`
  GREP=`searchpath grep "$PATH"`
  SED=`searchpath sed "$PATH"`
#
# Another fundamental requirement is the 'groff' program itself;
# we MUST use a 'groff' program located in 'GROFF_BIN_DIR', if this
# is specified; if not, we will search 'GROFF_BIN_PATH', only falling
# back to a 'PATH' search, if neither of these is specified.
#
  if test -n "$GROFF_BIN_DIR"
  then
    GPATH=GROFF_BIN_DIR
    GROFF=`searchpath groff "$GROFF_BIN_DIR"`
#
  elif test -n "$GROFF_BIN_PATH"
  then
    GPATH=GROFF_BIN_PATH
    GROFF=`searchpath groff "$GROFF_BIN_PATH"`
#
  else
    GPATH=PATH
    GROFF=`searchpath groff "$PATH"`
  fi
#
# If one or more of these is missing, diagnose and bail out.
#
  NO='' NOPROG="$CMD: installation problem: cannot find program"
  test "$CAT" = ":" && echo >&2 "$NOPROG 'cat' in PATH" && NO="$NO 'cat'"
  test "$GREP" = ":" && echo >&2 "$NOPROG 'grep' in PATH" && NO="$NO 'grep'"
  test "$GROFF" = ":" && echo >&2 "$NOPROG 'groff' in $GPATH" && NO="$NO 'groff'"
  test "$SED" = ":" && echo >&2 "$NOPROG 'sed' in PATH" && NO="$NO 'sed'"
  if test -n "$NO"
  then
    set $NO
    test $# -gt 1 && NO="s" IS="are" || NO='' IS="is"
    while test $# -gt 0
    do
      test $# -gt 2 && NO="$NO $1,"
      test $# -eq 2 && NO="$NO $1 and" && shift
      test $# -lt 2 && NO="$NO $1"
      shift
    done
    $CAT >&2 <<-ETX

	*** FATAL INSTALLATION ERROR ***

	The program$NO $IS required by '$CMD',
	but cannot be found; '$CMD' is unable to continue.

	ETX
    exit 1
  fi
#
# Set up temporary/intermediate file locations.
#
  WRKFILE=${GROFF_TMPDIR=${TMPDIR-${TMP-${TEMP-"."}}}}/pdf$$.tmp
#
  REFCOPY=${GROFF_TMPDIR}/pdf$$.cmp
  REFFILE=${GROFF_TMPDIR}/pdf$$.ref
#
  CS_DATA=""
  TC_DATA=${GROFF_TMPDIR}/pdf$$.tc
  BD_DATA=${GROFF_TMPDIR}/pdf$$.ps
#
# Set a trap, to delete temporary files on exit.
# (FIXME: may want to include other signals, in released version).
#
  trap "rm -f ${GROFF_TMPDIR}/pdf$$.*" 0
#
# Initialise 'groff' format control settings,
# to discriminate table of contents and document body formatting passes.
#
  TOC_FORMAT="-rPHASE=1"
  BODY_FORMAT="-rPHASE=2"
#
  LONGOPTS="
    help	reference-dictionary	no-reference-dictionary
    stylesheet	pdf-output		no-pdf-output
    version	report-progress		no-toc-relocation
    "
# Parse the command line, to identify 'pdfroff' specific options.
# Collect all other parameters into new argument and file lists,
# to be passed on to 'groff', enforcing the '-Tps' option.
#
  DIFF="" STREAM="" INPUT_FILES=""
  SHOW_VERSION="" GROFF_STYLE="$GROFF -Tps"
  while test $# -gt 0
  do
    case "$1" in
#
#     Long options must be processed locally ...
#
      --*)
#
#          First identify, matching any abbreviation to its full form.
#
           MATCH="" OPTNAME=`IFS==; set dummy $1; echo $2`
           for OPT in $LONGOPTS
           do
             MATCH="$MATCH"`echo --$OPT | $GREP "^$OPTNAME"`
           done
#
#          For options in the form --option=value
#          capture any specified value into $OPTARG.
#
	   OPTARG=`echo $1 | $SED -n s?"^${OPTNAME}="??p`
#
#          Perform case specific processing for matched option ...
#
           case "$MATCH" in

             --help)
               $CAT >&2 <<-ETX
		Usage: $CMD [-option ...] [--long-option ...] [file ...]

		Options:
		  -h
		  --help
		 	Display this usage summary, and exit.

		  -v
		  --version
		 	Display a version identification message and exit.

		  --report-progress
		  	Enable console messages, indicating the progress of the
		 	PDF document formatting process.

		  --pdf-output=name
		  	Write the PDF output stream to file 'name'; if this option
		 	is unspecified, standard output is used for PDF output.

		  --no-pdf-output
		 	Suppress the generation of PDF output entirely; use this
		 	with the --reference-dictionary option, if processing a
		 	document stream to produce only a reference dictionary.

		  --no-reference-dictionary
		 	Suppress the generation of a '$CMD' reference dictionary
		 	for the PDF document.  Normally '$CMD' will create a
		 	reference dictionary, at the start of document processing;
		 	this option can accelerate processing, if it is known in
		 	advance, that no reference dictionary is required.

		  --reference-dictionary=name
		 	Save the document reference dictionary in file 'name'.
		 	If 'name' already exists, when processing commences, it
		 	will be used as the base case, from which the updated
		 	dictionary will be derived.  If this option is not used,
		 	then the reference dictionary, created during the normal
		 	execution of '$CMD', will be deleted on completion of
		 	document processing.

		  --stylesheet=name
		  	Use the file 'name' as a 'groff' style sheet, to control
		 	the appearance of the document's front cover section.  If
		 	this option is not specified, then no special formatting
		 	is applied, to create a front cover section.

		  --no-toc-relocation
		 	Suppress the multiple pass 'groff' processing, which is
		 	normally required to position the table of contents at the
		 	start of a PDF document.

		ETX
               exit 0
               ;;

             --version)
	       GROFF_STYLE="$GROFF_STYLE \"$1\""
               SHOW_VERSION="GNU pdfroff (groff) version @VERSION@"
               ;;

             --report-progress)
               SHOW_PROGRESS=echo
               ;;

             --pdf-output)
	       PDF_OUTPUT="$OPTARG"
	       ;;

	     --no-pdf-output)
	       PDF_OUTPUT="$NULLDEV"
	       ;;

             --reference-dictionary)
               REFFILE="$OPTARG"
               ;;

             --no-reference-dictionary)
               AWK=":" DIFF=":" REFFILE="$NULLDEV" REFCOPY="$NULLDEV"
               ;;

             --stylesheet)
               STYLESHEET="$OPTARG" CS_DATA=${GROFF_TMPDIR}/pdf$$.cs
               ;;

	     --no-toc-relocation)
	       TC_DATA="" TOC_FORMAT="" BODY_FORMAT=""
	       ;;
#
#          any other non-null match must have matched more than one defined case,
#          so report the ambiguity, and bail out.
#
             --*)
               echo >&2 "$CMD: ambiguous abbreviation in option '$1'"
	       exit 1
               ;;
#
#          while no match at all simply represents an undefined case.
#
             *)
               echo >&2 "$CMD: unknown option '$1'"
	       exit 1
               ;;
           esac
           ;;
#
#     A solitary hyphen, as an argument, means "stream STDIN through groff",
#     while the "-i" option means "append STDIN stream to specified input files",
#     so set up a mechanism to achieve this, for ALL 'groff' passes.
#
      - | -i*)
	   STREAM="$CAT ${GROFF_TMPDIR}/pdf$$.in |"
	   test "$1" = "-" && INPUT_FILES="$INPUT_FILES $1" \
	     || GROFF_STYLE="$GROFF_STYLE $1" 
	   ;;
#
#     Those standard options which expect an argument, but are specified with
#     an intervening space, between flag and argument, must be reparsed, so we
#     can trap illegal use of '-T dev', or missing input files.
#
      -[dfFILmMnoPrTwW])
           OPTNAME="$1"
	   shift; set reparse "$OPTNAME$@"
	   ;;
#
#     Among standard options, '-Tdev' is treated as a special case.
#     '-Tps' is automatically enforced, so if specified, is silently ignored.
#
      -Tps) ;;
#
#     No other '-Tdev' option is permitted.
#
      -T*) echo >&2 "$CMD: option '$1' is incompatible with PDF output"
           exit 1
	   ;;
#
#     '-h' and '-v' options redirect to their equivalent long forms ...
#
      -h*) set redirect --help
           ;;
#
      -v*) shift; set redirect --version "$@"
           ;;
#
#     All other standard options are simply passed through to 'groff',
#     with no validation beforehand.
#
      -*)  GROFF_STYLE="$GROFF_STYLE \"$1\""
           ;;
#
#     All non-option arguments are considered as possible input file names,
#     and are passed on to 'groff', unaltered.
#
      *)   INPUT_FILES="$INPUT_FILES \"$1\""
           ;;
    esac
    shift
  done
#
# If the '-v' or '--version' option was specified,
# then we simply emulate the behaviour of 'groff', with this option,
# and quit.
#
  if test -n "$SHOW_VERSION"
  then
    echo >&2 "$SHOW_VERSION"
    echo >&2; eval $GROFF_STYLE $INPUT_FILES
    exit $?
  fi
#
# Establish how to invoke 'echo', suppressing the terminating newline.
# (Adapted from 'autoconf' code, as found in 'configure' scripts).
#
  case `echo "testing\c"; echo 1,2,3`,`echo -n testing; echo 1,2,3` in
    *c*,*-n*)  n=''   c=''   ;;
    *c*)       n='-n' c=''   ;;
    *)         n=''   c='\c' ;;
  esac
#
# If STDIN is specified among the input files,
# or if no input files are specified, then we need to capture STDIN,
# so we can replay it into each 'groff' processing pass.
#
  test -z "$INPUT_FILES" && STREAM="$CAT ${GROFF_TMPDIR}/pdf$$.in |"
  test -n "$STREAM" && $CAT > ${GROFF_TMPDIR}/pdf$$.in
#
# Unless reference resolution is explicitly suppressed,
# we initiate it by touching the cross reference dictionary file,
# and initialise the comparator, to kickstart the reference resolver loop.
#
  SAY=":"
  if test -z "$DIFF"
  then
    >> $REFFILE
    echo kickstart > $REFCOPY
    test "${SHOW_PROGRESS+"set"}" = "set" && SAY=echo
#
#   In order to correctly resolve 'pdfmark' references,
#   we need to have both the 'awk' and 'diff' programs available.
#
    NO=''
    if test -n "$GROFF_AWK_INTERPRETER"
    then
      AWK="$GROFF_AWK_INTERPRETER"
      test -f "$AWK" && test -x "$AWK" || AWK=":"
    else
      for prog in @GROFF_AWK_INTERPRETERS@
      do
	AWK=`searchpath $prog "$PATH"`
	test "$AWK" = ":" || break
      done
    fi
    DIFF=`searchpath diff "$PATH"`
    test "$AWK" = ":" && echo >&2 "$NOPROG 'awk' in PATH" && NO="$NO 'awk'"
    test "$DIFF" = ":" && echo >&2 "$NOPROG 'diff' in PATH" && NO="$NO 'diff'"
    if test -n "$NO"
    then
      set $NO
      SAY=":" AWK=":" DIFF=":"
      test $# -gt 1 && NO="s $1 and $2 are" || NO=" $1 is"
      $CAT >&2 <<-ETX

	*** WARNING ***

	The program$NO required, but cannot be found;
	consequently, '$CMD' is unable to resolve 'pdfmark' references.

	Document processing will continue, but no 'pdfmark' reference dictionary
	will be compiled; if any 'pdfmark' reference appears in the resulting PDF
	document, the formatting may not be correct.

	ETX
    fi
  fi
#
# Run the multi-pass 'pdfmark' reference resolver loop ...
#
  $SAY >&2 $n Resolving references ..$c
  until $DIFF $REFCOPY $REFFILE 1>$NULLDEV 2>&1
  do
#
#   until all references are resolved, to yield consistent values
#   in each of two consecutive passes, or until it seems that no consistent
#   resolution is achievable.
#
    $SAY >&2 $n .$c
    PASS_INDICATOR="${PASS_INDICATOR}."
    if test "$PASS_INDICATOR" = "...."
    then
#
#     More than three passes required indicates a probable inconsistency
#     in the source document; diagnose, and bail out.
#
      $SAY >&2 " failed"
      $CAT >&2 <<-ETX
	$CMD: unable to resolve references consistently after three passes
	$CMD: the source document may exhibit instability about the reference(s) ...
	ETX
#
#     Report the unresolved references, as a diff between the two pass files,
#     preferring 'unified' or 'context' diffs, when available
#
      DIFFOPT=''
      $DIFF -c0 $NULLDEV $NULLDEV 1>$NULLDEV 2>&1 && DIFFOPT='-c0'
      $DIFF -u0 $NULLDEV $NULLDEV 1>$NULLDEV 2>&1 && DIFFOPT='-u0'
      $DIFF >&2 $DIFFOPT $REFCOPY $REFFILE
      exit 1
    fi
#
#   Replace the comparison file copy from any previous pass,
#   with the most recently updated copy of the reference dictionary.
#   (Some versions of 'mv' may not support overwriting of an existing file,
#    so remove the old comparison file first).
#
    rm -f $REFCOPY
    mv $REFFILE $REFCOPY
#
#   Run 'groff' and 'awk', to identify reference marks in the document source,
#   filtering them into the reference dictionary; discard incomplete 'groff' output
#   at this stage.
#
    eval $STREAM $GROFF_STYLE -Z 1>$NULLDEV 2>$WRKFILE $REFCOPY $INPUT_FILES
    $AWK '/^gropdf-info:href/ {$1 = ".pdfhref D -N"; print}' $WRKFILE > $REFFILE
  done
  $SAY >&2 " done"
#
# To get to here ...
# We MUST have resolved all 'pdfmark' references, such that the content of the
# updated reference dictionary file EXACTLY matches the last saved copy.
#
# If PDF output has been suppressed, then there is nothing more to do.
#
  test "$PDF_OUTPUT" = "$NULLDEV" && exit 0
#
# We are now ready to start preparing the intermediate PostScript files,
# from which the PDF output will be compiled -- but before proceding further ...
# let's make sure we have a GhostScript interpreter to convert them!
#
  if test -n "$GROFF_GHOSTSCRIPT_INTERPRETER"
  then
    GS="$GROFF_GHOSTSCRIPT_INTERPRETER"
    test -f "$GS" && test -x "$GS" || GS=":"
  else
    for prog in @GROFF_GHOSTSCRIPT_INTERPRETERS@
    do
      GS=`searchpath $prog "$PATH"`
      test "$GS" = ":" || break
    done
  fi
#
# If we could not find a GhostScript interpreter, then we can do no more.
#
  if test "$GS" = ":"
  then
    echo >&2 "$CMD: installation problem: cannot find GhostScript interpreter"
    $CAT >&2 <<-ETX

	*** FATAL INSTALLATION ERROR ***

	'$CMD' requires a GhostScript interpreter to convert PostScript to PDF.
	Since you do not appear to have one installed, '$CMD' connot continue.

	ETX
    exit 1
  fi
#
# We now extend the local copy of the reference dictionary file,
# to create a full 'pdfmark' reference map for the document ...
#
  $AWK '/^grohtml-info/ {print ".pdfhref Z", $2, $3, $4}' $WRKFILE >> $REFCOPY
#
# Re-enable progress reporting, if necessary ...
# (Missing 'awk' or 'diff' may have disabled it, to avoid display
#  of spurious messages associated with reference resolution).
#
  test "${SHOW_PROGRESS+"set"}" = "set" && SAY=echo
#
# If a document cover style sheet is specified ...
# then we run a special formatting pass, to create a cover section file.
#
  if test -n "$STYLESHEET"
  then
    DOT='^\.[ 	]*'
    CS_MACRO=${CS_MACRO-"CS"} CE_MACRO=${CE_MACRO-"CE"}
    $SAY >&2 $n "Formatting document ... front cover section ..$c"
    CS_FILTER="$STREAM $SED -n '/${DOT}${CS_MACRO}/,/${DOT}${CE_MACRO}/p'"
    eval $CS_FILTER $INPUT_FILES | eval $GROFF_STYLE $STYLESHEET - > $CS_DATA
    $SAY >&2 ". done"
  fi
#
# If table of contents relocation is to be performed (it is, by default),
# then we run an extra 'groff' pass, to format a TOC intermediate file.
#
  if test -n "$TC_DATA"
  then
    $SAY >&2 $n "Formatting document ... table of contents ..$c"
    eval $STREAM $GROFF_STYLE $TOC_FORMAT $REFCOPY $INPUT_FILES > $TC_DATA
    $SAY >&2 ". done"
  fi
#
# In all cases, a final 'groff' pass is required, to format the document body.
#
  $SAY >&2 $n "Formatting document ... body section ..$c"
  eval $STREAM $GROFF_STYLE $BODY_FORMAT $REFCOPY $INPUT_FILES > $BD_DATA
  $SAY >&2 ". done"
#
# Finally ...
# Invoke GhostScript as a PDF writer, to bind all of the generated
# PostScript intermediate files into a single PDF output file.
#
  $SAY >&2 $n "Writing PDF output ..$c"
  PDFWRITE="$GS -dQUIET -dBATCH -dNOPAUSE -sDEVICE=pdfwrite"
#
# (This 'sed' script is a hack, to eliminate redundant blank pages).
#
  $SED '
    :again
      /%%EndPageSetup/b finish
      /%%Page:/{
	N
	b again
      }
      b
    :finish
      N
      /^%%Page:.*0 *Cg *EP/d
    ' $TC_DATA $BD_DATA | $PDFWRITE -sOutputFile=${PDF_OUTPUT-"-"} $CS_DATA -
  $SAY >&2 ". done"
#
# ------------------------------------------------------------------------------
# $RCSfile: pdfroff.sh,v $ $Revision: 1.7 $: end of file
