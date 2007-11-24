#! /bin/sh

# groffer - display groff files

# Source file position: <groff-source>/contrib/groffer/groffer2.sh
# Installed position: <prefix>/lib/groff/groffer/groffer2.sh

# This file should not be run independently.  It is called by
# `groffer.sh' in the source or by the installed `groffer' program.

# Copyright (C) 2001,2002,2003,2004,2005
# Free Software Foundation, Inc.
# Written by Bernd Warken

# Last update: 22 August 2005

# This file is part of `groffer', which is part of `groff'.

# `groff' is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or (at your option)
# any later version.

# `groff' is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.

# You should have received a copy of the GNU General Public License
# along with `groff'; see the files COPYING and LICENSE in the top
# directory of the `groff' source.  If not, write to the Free Software
# Foundation, 51 Franklin St - Fifth Floor, Boston, MA 02110-1301,
# USA.


########################################################################
#             Test of rudimentary shell functionality
########################################################################


########################################################################
# Test of `unset'
#
export _UNSET;
export _foo;
_foo=bar;
_res="$(unset _foo 2>&1)";
if unset _foo >${_NULL_DEV} 2>&1 && \
   test _"${_res}"_ = __ && test _"${_foo}"_ = __
then
  _UNSET='unset';
  eval "${_UNSET}" _foo;
  eval "${_UNSET}" _res;
else
  _UNSET=':';
fi;


########################################################################
# Test of `test'.
#
if test a = a && test a != b && test -f "${_GROFFER_SH}"
then
  :;
else
  echo '"test" did not work.' >&2;
  exit "${_ERROR}";
fi;


########################################################################
# Test of `echo' and the `$()' construct.
#
if echo '' >${_NULL_DEV}
then
  :;
else
  echo '"echo" did not work.' >&2;
  exit "${_ERROR}";
fi;
if test _"$(t1="$(echo te)" &&
            t2="$(echo '')" &&
            t3="$(echo 'st')" &&
            echo "${t1}${t2}${t3}")"_ \
     != _test_
then
  echo 'The "$()" construct did not work' >&2;
  exit "${_ERROR}";
fi;


########################################################################
# Test of sed program; test in groffer.sh is not valid here.
#
if test _"$(echo red | sed -e 's/r/s/')"_ != _sed_
then
  echo 'The sed program did not work.' >&2;
  exit "${_ERROR}";
fi;


########################################################################
# Test of function definitions.
#
_t_e_s_t_f_u_n_c_()
{
  return 0;
}

if _t_e_s_t_f_u_n_c_ 2>${_NULL_DEV}
then
  :;
else
  echo 'Shell '"${_SHELL}"' does not support function definitions.' >&2;
  exit "${_ERROR}";
fi;


########################################################################
#                    debug - diagnostic messages
########################################################################

export _DEBUG_STACKS;
_DEBUG_STACKS='no';		# disable stack output in each function
#_DEBUG_STACKS='yes';		# enable stack output in each function

export _DEBUG_LM;
_DEBUG_LM='no';			# disable landmark messages
#_DEBUG_LM='yes';		# enable landmark messages

export _DEBUG_KEEP_FILES;
_DEBUG_KEEP_FILES='no'		# disable file keeping in temporary dir
#_DEBUG_KEEP_FILES='yes'	# enable file keeping in temporary dir

export _DEBUG_PRINT_PARAMS;
_DEBUG_PRINT_PARAMS='no';	# disable printing of all parameters
#_DEBUG_PRINT_PARAMS='yes';	# enable printing of all parameters

export _DEBUG_PRINT_SHELL;
_DEBUG_PRINT_SHELL='no';	# disable printing of the shell name
#_DEBUG_PRINT_SHELL='yes';	# enable printing of the shell name

export _DEBUG_PRINT_TMPDIR;
_DEBUG_PRINT_TMPDIR='no';	# disable printing of the temporary dir
#_DEBUG_PRINT_TMPDIR='yes';	# enable printing of the temporary dir

export _DEBUG_USER_WITH_STACK;
_DEBUG_USER_WITH_STACK='no';	# disable stack dump in error_user()
#_DEBUG_USER_WITH_STACK='yes';	# enable stack dump in error_user()

# determine all --debug* options
case " $*" in
*\ --debug*)
  case " $* " in
  *' --debug '*)
    # _DEBUG_STACKS='yes';
    # _DEBUG_LM='yes';
    _DEBUG_KEEP_FILES='yes';
    _DEBUG_PRINT_PARAMS='yes';
    _DEBUG_PRINT_SHELL='yes';
    _DEBUG_PRINT_TMPDIR='yes';
    _DEBUG_USER_WITH_STACK='yes';
    ;;
  esac;
  d=' --debug-all --debug-keep --debug-lm --debug-params --debug-shell '\
'--debug-stacks --debug-tmpdir --debug-user ';
  for i
  do
    case "$i" in
    --debug-s)
      echo 'The abbreviation --debug-s has multiple options: '\
'--debug-shell and --debug-stacks.' >&2
      exit "${_ERROR}";
      ;;
    esac;
    case "$d" in
    *\ ${i}*)
      # extract whole word of abbreviation $i
      s="$(cat <<EOF | sed -n -e 's/^.* \('"$i"'[^ ]*\) .*/\1/p'
$d
EOF
)"
      case "$s" in
      '') continue; ;;
      --debug-all)
        _DEBUG_STACKS='yes';
        _DEBUG_LM='yes';
        _DEBUG_KEEP_FILES='yes';
        _DEBUG_PRINT_PARAMS='yes';
        _DEBUG_PRINT_SHELL='yes';
        _DEBUG_PRINT_TMPDIR='yes';
        _DEBUG_USER_WITH_STACK='yes';
        ;;
      --debug-keep)
        _DEBUG_PRINT_TMPDIR='yes';
        _DEBUG_KEEP_FILES='yes';
        ;;
      --debug-lm)
        _DEBUG_LM='yes';
        ;;
      --debug-params)
        _DEBUG_PRINT_PARAMS='yes';
        ;;
      --debug-shell)
        _DEBUG_PRINT_SHELL='yes';
        ;;
      --debug-stacks)
        _DEBUG_STACKS='yes';
        ;;
      --debug-tmpdir)
        _DEBUG_PRINT_TMPDIR='yes';
        ;;
      --debug-user)
        _DEBUG_USER_WITH_STACK='yes';
        ;;
      esac;
      ;;
    esac;
  done
  ;;
esac;

if test _"${_DEBUG_PRINT_PARAMS}"_ = _yes_
then
  echo "parameters: $@" >&2;
fi;

if test _"${_DEBUG_PRINT_SHELL}"_ = _yes_
then
  if test _"${_SHELL}"_ = __
  then
    if test _"${POSIXLY_CORRECT}"_ = _y_
    then
      echo 'shell: bash as /bin/sh (none specified)' >&2;
    else
      echo 'shell: /bin/sh (none specified)' >&2;
    fi;
  else
    echo "shell: ${_SHELL}" >&2;
  fi;
fi;


########################################################################
#                       Environment Variables
########################################################################

# Environment variables that exist only for this file start with an
# underscore letter.  Global variables to this file are written in
# upper case letters, e.g. $_GLOBAL_VARIABLE; temporary variables
# start with an underline and use only lower case letters and
# underlines, e.g.  $_local_variable .

#   [A-Z]*     system variables,      e.g. $MANPATH
#   _[A-Z_]*   global file variables, e.g. $_MAN_PATH
#   _[a-z_]*   temporary variables,   e.g. $_manpath

# Due to incompatibilities of the `ash' shell, the name of loop
# variables in `for' must be single character
#   [a-z]      local loop variables,   e.g. $i


########################################################################
# read-only variables (global to this file)
########################################################################

# function return values; `0' means ok; other values are error codes
export _ALL_EXIT;
export _BAD;
export _GOOD;
export _NO;
export _OK;
export _YES;

_GOOD='0';			# return ok
_BAD='1';			# return negatively, error code `1'
# $_ERROR was already defined as `7' in groffer.sh.

_NO="${_BAD}";
_YES="${_GOOD}";
_OK="${_GOOD}";

# quasi-functions, call with `eval', e.g `eval "${return_ok}"'
export return_ok;
export return_good;
export return_bad;
export return_yes;
export return_no;
export return_error;
export return_var;
return_ok="func_pop; return ${_OK}";
return_good="func_pop; return ${_GOOD}";
return_bad="func_pop; return ${_BAD}";
return_yes="func_pop; return ${_YES}";
return_no="func_pop; return ${_NO}";
return_error="func_pop; return ${_ERROR}";
return_var="func_pop; return";	# add number, e.g. `eval "${return_var} $n'


export _DEFAULT_MODES;
_DEFAULT_MODES='x,ps,tty';
export _DEFAULT_RESOLUTION;
_DEFAULT_RESOLUTION='75';

export _DEFAULT_TTY_DEVICE;
_DEFAULT_TTY_DEVICE='latin1';

# _VIEWER_* viewer programs for different modes (only X is necessary)
# _VIEWER_* a comma-separated list of viewer programs (with options)
export _VIEWER_DVI;		# viewer program for dvi mode
export _VIEWER_HTML_TTY;	# viewer program for html mode in tty
export _VIEWER_HTML_X;		# viewer program for html mode in X
export _VIEWER_PDF;		# viewer program for pdf mode
export _VIEWER_PS;		# viewer program for ps mode
export _VIEWER_X;		# viewer program for X mode
_VIEWER_DVI='kdvi,xdvi,dvilx';
_VIEWER_HTML_TTY='lynx';
_VIEWER_HTML_X='konqueror,mozilla,netscape,galeon,opera,amaya,arena';
_VIEWER_PDF='kghostview --scale 1.45,ggv,xpdf,acroread,kpdf';
_VIEWER_PS='kghostview --scale 1.45,ggv,gv,ghostview,gs_x11,gs';
_VIEWER_X='gxditview,xditview';

# Search automatically in standard sections `1' to `8', and in the
# traditional sections `9', `n', and `o'.  On many systems, there
# exist even more sections, mostly containing a set of man pages
# special to a specific program package.  These aren't searched for
# automatically, but must be specified on the command line.
export _MAN_AUTO_SEC_LIST;
_MAN_AUTO_SEC_LIST="'1' '2' '3' '4' '5' '6' '7' '8' '9' 'n' 'o'";
export _MAN_AUTO_SEC_CHARS;
_MAN_AUTO_SEC_CHARS='[123456789no]';

export _SPACE_SED;
_SPACE_SED='['"${_SP}${_TAB}"']';

export _SPACE_CASE;
_SPACE_CASE='[\'"${_SP}"'\'"${_TAB}"']';

export _PROCESS_ID;		# for shutting down the program
_PROCESS_ID="$$";


############ the command line options of the involved programs
#
# The naming scheme for the options environment names is
# $_OPTS_<prog>_<length>[_<argspec>]
#
# <prog>:    program name GROFFER, GROFF, or CMDLINE (for all
#            command line options)
# <length>:  LONG (long options) or SHORT (single character options)
# <argspec>: ARG for options with argument, NA for no argument;
#            without _<argspec> both the ones with and without arg.
#
# Each option that takes an argument must be specified with a
# trailing : (colon).

# exports
export _OPTS_GROFFER_SHORT_NA;
export _OPTS_GROFFER_SHORT_ARG;
export _OPTS_GROFFER_LONG_NA;
export _OPTS_GROFFER_LONG_ARG;
export _OPTS_GROFF_SHORT_NA;
export _OPTS_GROFF_SHORT_ARG;
export _OPTS_GROFF_LONG_NA;
export _OPTS_GROFF_LONG_ARG;
export _OPTS_X_SHORT_ARG;
export _OPTS_X_SHORT_NA;
export _OPTS_X_LONG_ARG;
export _OPTS_X_LONG_NA;
export _OPTS_MAN_SHORT_ARG;
export _OPTS_MAN_SHORT_NA;
export _OPTS_MAN_LONG_ARG;
export _OPTS_MAN_LONG_NA;
export _OPTS_MANOPT_SHORT_ARG;
export _OPTS_MANOPT_SHORT_NA;
export _OPTS_MANOPT_LONG_ARG;
export _OPTS_MANOPT_LONG_NA;
export _OPTS_CMDLINE_SHORT_NA;
export _OPTS_CMDLINE_SHORT_ARG;
export _OPTS_CMDLINE_LONG_NA;
export _OPTS_CMDLINE_LONG_ARG;

###### groffer native options

_OPTS_GROFFER_SHORT_NA="'h' 'Q' 'v' 'V' 'X' 'Z'";
_OPTS_GROFFER_SHORT_ARG="'T'";

_OPTS_GROFFER_LONG_NA="'auto' \
'apropos' 'apropos-data' 'apropos-devel' 'apropos-progs' \
'debug' 'debug-all' 'debug-keep' 'debug-lm' 'debug-params' 'debug-shell' \
'debug-stacks' 'debug-tmpdir' 'debug-user' 'default' 'do-nothing' 'dvi' \
'groff' 'help' 'intermediate-output' 'html' 'man' \
'no-location' 'no-man' 'no-special' 'pdf' 'ps' 'rv' 'source' \
'text' 'text-device' \
'tty' 'tty-device' 'version' 'whatis' 'where' 'www' 'x' 'X'";

_OPTS_GROFFER_LONG_ARG="\
'default-modes' 'device' 'dvi-viewer' 'dvi-viewer-tty' 'extension' 'fg' \
'fn' 'font' 'foreground' 'html-viewer' 'html-viewer-tty' 'mode' \
'pdf-viewer' 'pdf-viewer-tty' 'print' 'ps-viewer' 'ps-viewer-tty' 'shell' \
'title' 'tty-viewer' 'tty-viewer-tty' 'www-viewer' 'www-viewer-tty' \
'x-viewer' 'x-viewer-tty' 'X-viewer' 'X-viewer-tty'";

##### groffer options inhereted from groff

_OPTS_GROFF_SHORT_NA="'a' 'b' 'c' 'C' 'e' 'E' 'g' 'G' 'i' 'l' 'N' 'p' \
'R' 's' 'S' 't' 'U' 'z'";
_OPTS_GROFF_SHORT_ARG="'d' 'f' 'F' 'I' 'L' 'm' 'M' 'n' 'o' 'P' 'r' \
'w' 'W'";
_OPTS_GROFF_LONG_NA="";
_OPTS_GROFF_LONG_ARG="";

##### groffer options inhereted from the X Window toolkit

_OPTS_X_SHORT_NA="";
_OPTS_X_SHORT_ARG="";

_OPTS_X_LONG_NA="'iconic' 'rv'";

_OPTS_X_LONG_ARG="'background' 'bd' 'bg' 'bordercolor' 'borderwidth' \
'bw' 'display' 'fg' 'fn' 'font' 'foreground' 'ft' 'geometry' \
'resolution' 'title' 'xrm'";

###### groffer options inherited from man

_OPTS_MAN_SHORT_NA="";
_OPTS_MAN_SHORT_ARG="";

_OPTS_MAN_LONG_NA="'all' 'ascii' 'catman' 'ditroff' \
'local-file' 'location' 'troff' 'update'";

_OPTS_MAN_LONG_ARG="'locale' 'manpath' \
'pager' 'preprocessor' 'prompt' 'sections' 'systems' 'troff-device'";

###### additional options for parsing $MANOPT only

_OPTS_MANOPT_SHORT_NA="'7' 'a' 'c' 'd' 'D' 'f' 'h' 'k' 'l' 't' 'u' \
'V' 'w' 'Z'";
_OPTS_MANOPT_SHORT_ARG="'e' 'L' 'm' 'M' 'p' 'P' 'r' 'S' 'T'";

_OPTS_MANOPT_LONG_NA="${_OPTS_MAN_LONG_NA} \
'apropos' 'debug' 'default' 'help' 'html' 'ignore-case' 'location-cat' \
'match-case' 'troff' 'update' 'version' 'whatis' 'where' 'where-cat'";

_OPTS_MANOPT_LONG_ARG="${_OPTS_MAN_LONG_NA} \
'config_file' 'encoding' 'extension' 'locale'";

###### collections of command line options

_OPTS_CMDLINE_SHORT_NA="${_OPTS_GROFFER_SHORT_NA} \
${_OPTS_GROFF_SHORT_NA} ${_OPTS_X_SHORT_NA} ${_OPTS_MAN_SHORT_NA}";
_OPTS_CMDLINE_SHORT_ARG="${_OPTS_GROFFER_SHORT_ARG} \
${_OPTS_GROFF_SHORT_ARG} ${_OPTS_X_SHORT_ARG} ${_OPTS_MAN_SHORT_ARG}";

_OPTS_CMDLINE_LONG_NA="${_OPTS_GROFFER_LONG_NA} \
${_OPTS_GROFF_LONG_NA} ${_OPTS_X_LONG_NA} ${_OPTS_MAN_LONG_NA}";
_OPTS_CMDLINE_LONG_ARG="${_OPTS_GROFFER_LONG_ARG} \
${_OPTS_GROFF_LONG_ARG} ${_OPTS_MAN_LONG_ARG} ${_OPTS_X_LONG_ARG}";


########################################################################
# read-write variables (global to this file)
########################################################################

export _ALL_PARAMS;		# All options and file name parameters
export _ADDOPTS_GROFF;		# Transp. options for groff (`eval').
export _ADDOPTS_POST;		# Transp. options postproc (`eval').
export _ADDOPTS_X;		# Transp. options X postproc (`eval').
export _APROPOS_PROG;		# Program to run apropos.
export _APROPOS_SECTIONS;	# Sections for different --apropos-*.
export _DEFAULT_MODES;		# Set default modes.
export _DISPLAY_MODE;		# Display mode.
export _DISPLAY_PROG;		# Viewer program to be used for display.
export _DISPLAY_ARGS;		# X resources for the viewer program.
export _FILEARGS;		# Stores filespec parameters.
export _FILESPEC_ARG;		# Stores the actual filespec parameter.
export _FUNC_STACK;		# Store debugging information.
export _REGISTERED_TITLE;	# Processed file names.
# _HAS_* from availability tests
export _HAS_COMPRESSION;	# `yes' if gzip compression is available
export _HAS_BZIP;		# `yes' if bzip2 compression is available
# _MAN_* finally used configuration of man searching
export _MAN_ALL;		# search all man pages per filespec
export _MAN_ENABLE;		# enable search for man pages
export _MAN_EXT;		# extension for man pages
export _MAN_FORCE;		# force file parameter to be man pages
export _MAN_IS_SETUP;		# setup man variables only once
export _MAN_LANG;		# language for man pages
export _MAN_LANG2;		# language for man pages
export _MAN_LANG_DONE;		# language dirs added to man path
export _MAN_PATH;		# search path for man pages
export _MAN_SEC;		# sections for man pages; sep. `:'
export _MAN_SEC_DONE;		# sections added to man path
export _MAN_SYS;		# system names for man pages; sep. `,'
export _MAN_SYS;		# system names added to man path
# _MANOPT_* as parsed from $MANOPT
export _MANOPT_ALL;		# $MANOPT --all
export _MANOPT_EXTENSION;	# $MANOPT --extension
export _MANOPT_LANG;		# $MANOPT --locale
export _MANOPT_PATH;		# $MANOPT --manpath
export _MANOPT_PAGER;		# $MANOPT --pager
export _MANOPT_SEC;		# $MANOPT --sections
export _MANOPT_SYS;		# $MANOPT --systems
# _OPT_* as parsed from groffer command line
export _OPT_ALL;		# display all suitable man pages.
export _OPT_APROPOS;		# call `apropos' program.
export _OPT_BD;			# set border color in some modes.
export _OPT_BG;			# set background color in some modes.
export _OPT_BW;			# set border width in some modes.
export _OPT_DEFAULT_MODES;	# `,'-list of modes when no mode given.
export _OPT_DEVICE;		# device option.
export _OPT_DO_NOTHING;		# do nothing in main_display().
export _OPT_DISPLAY;		# set X display.
export _OPT_FG;			# set foreground color in some modes.
export _OPT_FN;			# set font in some modes.
export _OPT_GEOMETRY;		# set size and position of viewer in X.
export _OPT_ICONIC;		# -iconic option for X viewers.
export _OPT_LANG;		# set language for man pages
export _OPT_LOCATION;		# print processed file names to stderr
export _OPT_MODE;		# values: X, tty, Q, Z, ""
export _OPT_MANPATH;		# manual setting of path for man-pages
export _OPT_PAGER;		# specify paging program for tty mode
export _OPT_RESOLUTION;		# set X resolution in dpi
export _OPT_RV;			# reverse fore- and background colors.
export _OPT_SECTIONS;		# sections for man page search
export _OPT_SYSTEMS;		# man pages of different OS's
export _OPT_TITLE;		# title for gxditview window
export _OPT_TEXT_DEVICE;	# set device for tty mode.
export _OPT_V;			# groff option -V.
export _OPT_VIEWER_DVI;		# viewer program for dvi mode
export _OPT_VIEWER_PDF;		# viewer program for pdf mode
export _OPT_VIEWER_PS;		# viewer program for ps mode
export _OPT_VIEWER_HTML;	# viewer program for html mode
export _OPT_VIEWER_X;		# viewer program for x mode
export _OPT_WHATIS;		# print the man description
export _OPT_XRM;		# specify X resource.
export _OPT_Z;			# groff option -Z.
export _OUTPUT_FILE_NAME;	# output generated, see main_set_res..()
export _VIEWER_TERMINAL;	# viewer options for terminal (--*-viewer-tty)
# _TMP_* temporary directory and files
export _TMP_DIR;		# groffer directory for temporary files
export _TMP_CAT;		# stores concatenation of everything
export _TMP_STDIN;		# stores stdin, if any

# these variables are preset in section `Preset' after the rudim. test


########################################################################
# Preset and reset of read-write global variables
########################################################################


export _START_DIR;		# directory at start time of the script
_START_DIR="$(pwd)";

# For variables that can be reset by option `--default', see reset().

_FILEARGS='';

# _HAS_* from availability tests
_HAS_COMPRESSION='';
_HAS_BZIP='';

# _TMP_* temporary files
_TMP_DIR='';
_TMP_CAT='';
_TMP_CONF='';
_TMP_STDIN='';


########################################################################
# reset ()
#
# Reset the variables that can be affected by options to their default.
#
reset()
{
  if test "$#" -ne 0
  then
    error "reset() does not have arguments.";
  fi;

  _ADDOPTS_GROFF='';
  _ADDOPTS_POST='';
  _ADDOPTS_X='';
  _APROPOS_PROG='';
  _APROPOS_SECTIONS='';
  _DISPLAY_ARGS='';
  _DISPLAY_MODE='';
  _DISPLAY_PROG='';
  _REGISTERED_TITLE='';

  # _MAN_* finally used configuration of man searching
  _MAN_ALL='no';
  _MAN_ENABLE='yes';		# do search for man-pages
  _MAN_EXT='';
  _MAN_FORCE='no';		# first local file, then search man page
  _MAN_IS_SETUP='no';
  _MAN_LANG='';
  _MAN_LANG2='';
  _MAN_PATH='';
  _MAN_SEC='';
  _MAN_SEC_DONE='no';
  _MAN_SYS='';
  _MAN_SYS_DONE='no';

  # _MANOPT_* as parsed from $MANOPT
  _MANOPT_ALL='no';
  _MANOPT_EXTENSION='';
  _MANOPT_LANG='';
  _MANOPT_PATH='';
  _MANOPT_PAGER='';
  _MANOPT_SEC='';
  _MANOPT_SYS='';

  # _OPT_* as parsed from groffer command line
  _OPT_ALL='no';
  _OPT_APROPOS='no';
  _OPT_BD='';
  _OPT_BG='';
  _OPT_BW='';
  _OPT_DEFAULT_MODES='';
  _OPT_DEVICE='';
  _OPT_DISPLAY='';
  _OPT_DO_NOTHING='no';
  _OPT_FG='';
  _OPT_FN='';
  _OPT_GEOMETRY='';
  _OPT_ICONIC='no';
  _OPT_LANG='';
  _OPT_LOCATION='no';
  _OPT_MODE='';
  _OPT_MANPATH='';
  _OPT_PAGER='';
  _OPT_RESOLUTION='';
  _OPT_RV='no';
  _OPT_SECTIONS='';
  _OPT_SYSTEMS='';
  _OPT_TITLE='';
  _OPT_TEXT_DEVICE='';
  _OPT_V='no';
  _OPT_VIEWER_DVI='';
  _OPT_VIEWER_PDF='';
  _OPT_VIEWER_PS='';
  _OPT_VIEWER_HTML='';
  _OPT_VIEWER_X='';
  _OPT_WHATIS='no';
  _OPT_XRM='';
  _OPT_Z='no';
  _VIEWER_TERMINAL='no';
}

reset;


########################################################################
#          Functions for error handling and debugging
########################################################################


##############
# echo1 (<text>*)
#
# Output to stdout.
#
# Arguments : arbitrary text including `-'.
#
echo1()
{
  cat <<EOF
$@
EOF
}


##############
# echo2 (<text>*)
#
# Output to stderr.
#
# Arguments : arbitrary text.
#
echo2()
{
  cat >&2 <<EOF
$@
EOF
}


##############
# landmark (<text>)
#
# Print <text> to standard error as a debugging aid.
#
# Globals: $_DEBUG_LM
#
landmark()
{
  if test _"${_DEBUG_LM}"_ = _yes_
  then
    echo2 "LM: $*";
  fi;
}

landmark "1: debugging functions";


##############
# clean_up ()
#
# Clean up at exit.
#
clean_up()
{
  cd "${_START_DIR}" >"${_NULL_DEV}" 2>&1;
  if test _${_DEBUG_KEEP_FILES}_ = _yes_
  then
    echo2 "Kept temporary directory ${_TMP_DIR}."
  else
    if test _"${_TMP_DIR}"_ != __
    then
      if test -d "${_TMP_DIR}" || test -f "${_TMP_DIR}"
      then
        rm -f -r "${_TMP_DIR}" >${_NULL_DEV} 2>&1;
      fi; 
    fi;
  fi;
}


#############
# diag (text>*)
#
# Output a diagnostic message to stderr
#
diag()
{
  echo2 '>>>>>'"$*";
}


#############
# error (<text>*)
#
# Print an error message to standard error, print the function stack,
# exit with an error condition.  The argument should contain the name
# of the function from which it was called.  This is for system errors.
#
error()
{
  case "$#" in
    1) echo2 'groffer error: '"$1"; ;;
    *) echo2 'groffer error: wrong number of arguments in error().'; ;;
  esac;
  func_stack_dump;
  if test _"${_TMP_DIR}"_ != __ && test -d "${_TMP_DIR}"
  then
    : >"${_TMP_DIR}"/,error;
  fi;
  exit "${_ERROR}";
}


#############
# error_user (<text>*)
#
# Print an error message to standard error; exit with an error condition.
# The error is supposed to be produce by the user.  So the funtion stack
# is omitted.
#
error_user()
{
  case "$#" in
    1)
      echo2 'groffer error: '"$1";
       ;;
    *)
      echo2 'groffer error: wrong number of arguments in error_user().';
      ;;
  esac;
  if test _"${_DEBUG_USER_WITH_STACK}"_ = _yes_
  then
    func_stack_dump;
  fi;
  if test _"${_TMP_DIR}"_ != __ && test -d "${_TMP_DIR}"
  then
    : >"${_TMP_DIR}"/,error;
  fi;
  exit "${_ERROR}";
}


#############
# exit_test ()
#
# Test whether the former command ended with error().  Exit again.
#
# Globals: $_ERROR
#
exit_test()
{
  if test "$?" = "${_ERROR}"
  then
    exit ${_ERROR};
  fi;
  if test _"${_TMP_DIR}"_ != __ && test -f "${_TMP_DIR}"/,error
  then
    exit ${_ERROR};
  fi;
}


#############
# func_check (<func_name> <rel_op> <nr_args> "$@")
#
# Check number of arguments and register to _FUNC_STACK.
#
# Arguments: >=3
#   <func_name>: name of the calling function.
#   <rel_op>:    a relational operator: = != < > <= >=
#   <nr_args>:   number of arguments to be checked against <operator>
#   "$@":        the arguments of the calling function.
#
# Variable prefix: fc
#
func_check()
{
  if test "$#" -lt 3
  then
    error 'func_check() needs at least 3 arguments.';
  fi;
  fc_fname="$1";
  case "$3" in
    1)
      fc_nargs="$3";
      fc_s='';
      ;;
    0|[2-9])
      fc_nargs="$3";
      fc_s='s';
      ;;
    *)
      error "func_check(): third argument must be a digit.";
      ;;
  esac;
  case "$2" in
    '='|'-eq')
      fc_op='-eq';
      fc_comp='exactly';
      ;;
    '>='|'-ge')
      fc_op='-ge';
      fc_comp='at least';
      ;;
    '<='|'-le')
      fc_op='-le';
      fc_comp='at most';
      ;;
    '<'|'-lt')
      fc_op='-lt';
      fc_comp='less than';
      ;;
    '>'|'-gt')
      fc_op='-gt';
      fc_comp='more than';
      ;;
    '!='|'-ne')
      fc_op='-ne';
      fc_comp='not';
      ;;
    *)
      error \
        'func_check(): second argument is not a relational operator.';
      ;;
  esac;
  shift;
  shift;
  shift;
  if test "$#" "${fc_op}" "${fc_nargs}"
  then
    do_nothing;
  else
    error "func_check(): \
${fc_fname}"'() needs '"${fc_comp} ${fc_nargs}"' argument'"${fc_s}"'.';
  fi;
  func_push "${fc_fname}";
  if test _"${_DEBUG_STACKS}"_ = _yes_
  then
    echo2 '+++ '"${fc_fname} $@";
    echo2 '>>> '"${_FUNC_STACK}";
  fi;
  eval ${_UNSET} fc_comp;
  eval ${_UNSET} fc_fname;
  eval ${_UNSET} fc_nargs;
  eval ${_UNSET} fc_op;
  eval ${_UNSET} fc_s;
}


#############
# func_pop ()
#
# Retrieve the top element from the stack.
#
# The stack elements are separated by `!'; the popped element is
# identical to the original element, except that all `!' characters
# were removed.
#
# Arguments: 1
#
func_pop()
{
  if test "$#" -ne 0
  then
    error 'func_pop() does not have arguments.';
  fi;
  case "${_FUNC_STACK}" in
  '')
    if test _"${_DEBUG_STACKS}"_ = _yes_
    then
      error 'func_pop(): stack is empty.';
    fi;
    ;;
  *!*)
    # split at first bang `!'.
    _FUNC_STACK="$(echo1 "${_FUNC_STACK}" | sed -e 's/^[^!]*!//')";
    exit_test;
    ;;
  *)
    _FUNC_STACK='';
    ;;
  esac;
  if test _"${_DEBUG_STACKS}"_ = _yes_
  then
    echo2 '<<< '"${_FUNC_STACK}";
  fi;
}


#############
# func_push (<element>)
#
# Store another element to stack.
#
# The stack elements are separated by `!'; if <element> contains a `!'
# it is removed first.
#
# Arguments: 1
#
# Variable prefix: fp
#
func_push()
{
  if test "$#" -ne 1
  then
    error 'func_push() needs 1 argument.';
  fi;
  case "$1" in
  *'!'*)
    # remove all bangs `!'.
    fp_element="$(echo1 "$1" | sed -e 's/!//g')";
    exit_test;
    ;;
  *)
    fp_element="$1";
    ;;
  esac;
  if test _"${_FUNC_STACK}"_ = __
  then
    _FUNC_STACK="${fp_element}";
  else
    _FUNC_STACK="${fp_element}!${_FUNC_STACK}";
  fi;
  eval ${_UNSET} fp_element;
}


#############
# func_stack_dump ()
#
# Print the content of the stack.  Ignore the arguments.
#
func_stack_dump()
{
  diag 'call stack: '"${_FUNC_STACK}";
}


########################################################################
#                        System Test
########################################################################

landmark "2: system test";

# Test the availability of the system utilities used in this script.


########################################################################
# Test of function `sed'.
#

if test _"$(echo xTesTx \
           | sed -e 's/^.\([Tt]e*x*sTT*\).*$/\1/' \
           | sed -e 's|T|t|g')"_ != _test_
then
  error 'Test of "sed" command failed.';
fi;


########################################################################
# Test of function `cat'.
#
if test _"$(echo test | cat)"_ != _test_
then
  error 'Test of "cat" command failed.';
fi;


########################################################################
# Test for compression.
#
if test _"$(echo 'test' | gzip -c -d -f - 2>${_NULL_DEV})"_ = _test_
then
  _HAS_COMPRESSION='yes';
  if echo1 'test' | bzip2 -c 2>${_NULL_DEV} | bzip2 -t 2>${_NULL_DEV} \
     && test _"$(echo 'test' | bzip2 -c 2>${_NULL_DEV} \
                             | bzip2 -d -c 2>${_NULL_DEV})"_ \
             = _test_
  then
    _HAS_BZIP='yes';
  else
    _HAS_BZIP='no';
  fi;
else
  _HAS_COMPRESSION='no';
  _HAS_BZIP='no';
fi;


########################################################################
#       Definition of normal Functions in alphabetical order
########################################################################
landmark "3: functions";

########################################################################
# apropos_filespec ()
#
# Setup for the --apropos* options
#
apropos_filespec()
{

  func_check apropos_filespec '=' 0 "$@";
  if obj _OPT_APROPOS is_yes
  then
    eval to_tmp_line \
      "'.SH $(echo1 "${_FILESPEC_ARG}" | sed 's/[^\\]-/\\-/g')'";
    exit_test;
    if obj _APROPOS_PROG is_empty
    then
      error 'apropos_filespec: apropos_setup() must be run first.';
    fi;
    if obj _APROPOS_SECTIONS is_empty
    then
      if obj _OPT_SECTIONS is_empty
      then
        s='^.*(.*).*$';
      else
        s='^.*(['"$(echo1 "${_OPT_SECTIONS}" | sed -e 's/://g')"']';
      fi;
    else
      s='^.*(['"${_APROPOS_SECTIONS}"']';
    fi;
    eval "${_APROPOS_PROG}" "'${_FILESPEC_ARG}'" | \
      sed -n -e '
/^'"${_FILESPEC_ARG}"': /p
/'"$s"'/p
' | \
      sort |\
      sed -e '
s/^\(.* (..*)\)  *-  *\(.*\)$/\.br\n\.TP 15\n\.BR \1\n\2/
' >>"${_TMP_CAT}";
  fi;
  eval "${return_ok}";
}


########################################################################
# apropos_setup ()
#
# Setup for the --apropos* options
#
apropos_setup()
{
  func_check apropos_setup '=' 0 "$@";
  if obj _OPT_APROPOS is_yes
  then
    if is_prog apropos
    then
      _APROPOS_PROG='apropos';
    elif is_prog man
    then
      if man --apropos man >${_NULL_DEV} 2>${_NULL_DEV}
      then
        _APROPOS_PROG='man --apropos';
      elif man -k man >${_NULL_DEV} 2>${_NULL_DEV}
      then
        _APROPOS_PROG='man -k';
      fi;
    fi;
    if obj _APROPOS_PROG is_empty
    then
      error 'apropos_setup: no apropos program available.';
    fi;
    to_tmp_line '.TH GROFFER APROPOS';
  fi;
  eval "${return_ok}";
}


########################################################################
# base_name (<path>)
#
# Get the file name part of <path>, i.e. delete everything up to last
# `/' from the beginning of <path>.  Remove final slashes, too, to get a
# non-empty output.
#
# Arguments : 1
# Output    : the file name part (without slashes)
#
# Variable prefix: bn
#
base_name()
{
  func_check base_name = 1 "$@";
  bn_name="$1";
  case "${bn_name}" in
    */)
      # delete all final slashes
      bn_name="$(echo1 "${bn_name}" | sed -e 's|//*$||')";
      exit_test;
      ;;
  esac;
  case "${bn_name}" in
    /|'')
      eval ${_UNSET} bn_name;
      eval "${return_bad}";
      ;;
    */*)
      # delete everything before and including the last slash `/'.
      echo1 "${bn_name}" | sed -e 's|^.*//*\([^/]*\)$|\1|';
      ;;
    *)
      obj bn_name echo1;
      ;;
  esac;
  eval ${_UNSET} bn_name;
  eval "${return_ok}";
}


########################################################################
# cat_z (<file>)
#
# Decompress if possible or just print <file> to standard output.
#
# gzip, bzip2, and .Z decompression is supported.
#
# Arguments: 1, a file name.
# Output: the content of <file>, possibly decompressed.
#
if test _"${_HAS_COMPRESSION}"_ = _yes_
then
  cat_z()
  {
    func_check cat_z = 1 "$@";
    case "$1" in
      '')
        error 'cat_z(): empty file name';
        ;;
      '-')
        error 'cat_z(): for standard input use save_stdin()';
        ;;
    esac;
    if obj _HAS_BZIP is_yes
    then
      if bzip2 -t "$1" 2>${_NULL_DEV}
      then
        bzip2 -c -d "$1" 2>${_NULL_DEV};
        eval "${return_ok}";
      fi;
    fi;
    gzip -c -d -f "$1" 2>${_NULL_DEV};
    eval "${return_ok}";
  }
else
  cat_z()
  {
    func_check cat_z = 1 "$@";
    cat "$1";
    eval "${return_ok}";
  }
fi;


########################################################################
# clean_up ()
#
# Do the final cleaning up before exiting; used by the trap calls.
#
# defined above


########################################################################
# diag (<text>*)
#
# Print marked message to standard error; useful for debugging.
#
# defined above


########################################################################
landmark '4: dirname()*';
########################################################################

#######################################################################
# dirname_append (<dir> <name>)
#
# Append `name' to `dir' with clean handling of `/'.
#
# Arguments : 2
# Output    : the generated new directory name <dir>/<name>
#
dirname_append()
{
  func_check dirname_append = 2 "$@";
  if is_empty "$1"
  then
    error "dir_append(): first argument is empty.";
  fi;
  if is_empty "$2"
  then
    echo1 "$1";
  else
    dirname_chop "$1"/"$2";
  fi;
  eval "${return_ok}";
}


########################################################################
# dirname_chop (<name>)
#
# Remove unnecessary slashes from directory name.
#
# Argument: 1, a directory name.
# Output:   path without double, or trailing slashes.
#
# Variable prefix: dc
#
dirname_chop()
{
  func_check dirname_chop = 1 "$@";
  # replace all multiple slashes by a single slash `/'.
  dc_res="$(echo1 "$1" | sed -e 's|///*|/|g')";
  exit_test;
  case "${dc_res}" in
  ?*/)
    # remove trailing slash '/';
    echo1 "${dc_res}" | sed -e 's|/$||';
    ;;
  *)
    obj dc_res echo1
    ;;
  esac;
  eval ${_UNSET} dc_res;
  eval "${return_ok}";
}


########################################################################
# do_filearg (<filearg>)
#
# Append the file, man-page, or standard input corresponding to the
# argument to the temporary file.  If this is compressed in the gzip
# or Z format it is decompressed.  A title element is generated.
#
# Argument either:
#   - name of an existing file.
#   - `-' to represent standard input (several times allowed).
#   - `man:name.(section)' the man-page for `name' in `section'.
#   - `man:name.section' the man-page for `name' in `section'.
#   - `man:name' the man-page for `name' in the lowest `section'.
#   - `name.section' the man-page for `name' in `section'.
#   - `name' the man-page for `name' in the lowest `section'.
# Globals :
#   $_TMP_STDIN, $_MAN_ENABLE, $_MAN_IS_SETUP, $_OPT_MAN
#
# Output  : none
# Return  : $_GOOD if found, ${_BAD} otherwise.
#
# Variable prefix: df
#
do_filearg()
{
  func_check do_filearg = 1 "$@";
  df_filespec="$1";
  # store sequence into positional parameters
  case "${df_filespec}" in
  '')
    eval ${_UNSET} df_filespec;
    eval "${return_good}";
    ;;
  '-')
    register_file '-';
    eval ${_UNSET} df_filespec;
    eval "${return_good}";
    ;;
  */*)			       # with directory part; so no man search
    set 'File';
    ;;
  *)
    if obj _MAN_ENABLE is_yes
    then
      if obj _MAN_FORCE is_yes
      then
        set 'Manpage' 'File';
      else
        set 'File' 'Manpage';
      fi;
      else
      set 'File';
    fi;
    ;;
  esac;
  for i
  do
    case "$i" in
    File)
      if test -f "${df_filespec}"
      then
        if test -r "${df_filespec}"
        then
          register_file "${df_filespec}";
          eval ${_UNSET} df_filespec;
          eval ${_UNSET} df_no_man;
          eval "${return_good}";
        else
          echo2 "could not read \`${df_filespec}'";
          eval ${_UNSET} df_filespec;
          eval ${_UNSET} df_no_man;
          eval "${return_bad}";
        fi;
      else
        if obj df_no_man is_not_empty
        then
          if obj _OPT_WHATIS is_yes
          then
            to_tmp_line "This is neither a file nor a man page."
          else
            echo2 "\`${df_filespec}' is neither a file nor a man page."
          fi;
        fi;
        df_no_file=yes;
        continue;
      fi;
      ;;
    Manpage)			# parse filespec as man page
      if obj _MAN_IS_SETUP is_not_yes
      then
        man_setup;
      fi;
      if man_do_filespec "${df_filespec}"
      then
        eval ${_UNSET} df_filespec;
        eval ${_UNSET} df_no_file;
        eval "${return_good}";
      else
        if obj df_no_file is_not_empty
        then
          if obj _OPT_WHATIS is_yes
          then
            to_tmp_line "This is neither a file nor a man page."
          else
            echo2 "\`${df_filespec}' is neither a file nor a man page."
          fi;
        fi;
        df_no_man=yes;
        continue;
      fi;
      ;;
    esac;
  done;
  eval ${_UNSET} df_filespec;
  eval ${_UNSET} df_no_file;
  eval ${_UNSET} df_no_man;
  eval "${return_bad}";
} # do_filearg()


########################################################################
# do_nothing ()
#
# Dummy function.
#
do_nothing()
{
  eval return "${_OK}";
}


########################################################################
# echo2 (<text>*)
#
# Print to standard error with final line break.
#
# defined above


########################################################################
# error (<text>*)
#
# Print error message and exit with error code.
#
# defined above


########################################################################
# exit_test ()
#
# Test whether the former command ended with error().  Exit again.
#
# defined above


########################################################################
# func_check (<func_name> <rel_op> <nr_args> "$@")
#
# Check number of arguments and register to _FUNC_STACK.
#
# Arguments: >=3
#   <func_name>: name of the calling function.
#   <rel_op>:    a relational operator: = != < > <= >=
#   <nr_args>:   number of arguments to be checked against <operator>
#   "$@":        the arguments of the calling function.
#
# defined above

#########################################################################
# func_pop ()
#
# Delete the top element from the function call stack.
#
# defined above


########################################################################
# func_push (<element>)
#
# Store another element to function call stack.
#
# defined above


########################################################################
# func_stack_dump ()
#
# Print the content of the stack.
#
# defined above


########################################################################
# get_first_essential (<arg>*)
#
# Retrieve first non-empty argument.
#
# Return  : `1' if all arguments are empty, `0' if found.
# Output  : the retrieved non-empty argument.
#
# Variable prefix: gfe
#
get_first_essential()
{
  func_check get_first_essential '>=' 0 "$@";
  if is_equal "$#" 0
  then
    eval "${return_ok}";
  fi;
  for i
  do
    gfe_var="$i";
    if obj gfe_var is_not_empty
    then
      obj gfe_var echo1;
      eval ${_UNSET} gfe_var;
      eval "${return_ok}";
    fi;
  done;
  eval ${_UNSET} gfe_var;
  eval "${return_bad}";
}


########################################################################
landmark '5: is_*()';
########################################################################

########################################################################
# is_dir (<name>)
#
# Test whether `name' is a directory.
#
# Arguments : 1
# Return    : `0' if arg1 is a directory, `1' otherwise.
#
is_dir()
{
  func_check is_dir '=' 1 "$@";
  if test _"$1"_ != __ && test -d "$1" && test -r "$1"
  then
    eval "${return_yes}";
  fi;
  eval "${return_no}";
}


########################################################################
# is_empty (<string>)
#
# Test whether `string' is empty.
#
# Arguments : <=1
# Return    : `0' if arg1 is empty or does not exist, `1' otherwise.
#
is_empty()
{
  func_check is_empty '=' 1 "$@";
  if test _"$1"_ = __
  then
    eval "${return_yes}";
  fi;
  eval "${return_no}";
}


########################################################################
# is_equal (<string1> <string2>)
#
# Test whether `string1' is equal to <string2>.
#
# Arguments : 2
# Return    : `0' both arguments are equal strings, `1' otherwise.
#
is_equal()
{
  func_check is_equal '=' 2 "$@";
  if test _"$1"_ = _"$2"_
  then
    eval "${return_yes}";
  fi;
  eval "${return_no}";
}


########################################################################
# is_existing (<name>)
#
# Test whether `name' is an existing file or directory.  Solaris 2.5 does
# not have `test -e'.
#
# Arguments : 1
# Return    : `0' if arg1 exists, `1' otherwise.
#
is_existing()
{
  func_check is_existing '=' 1 "$@";
  if test _"$1"_ = __
  then
    eval "${return_no}";
  fi;
  if test -f "$1" || test -d "$1" || test -c "$1"
  then
    eval "${return_yes}";
  fi;
  eval "${return_no}";
}


########################################################################
# is_file (<name>)
#
# Test whether `name' is a readable file.
#
# Arguments : 1
# Return    : `0' if arg1 is a readable file, `1' otherwise.
#
is_file()
{
  func_check is_file '=' 1 "$@";
  if is_not_empty "$1" && test -f "$1" && test -r "$1"
  then
    eval "${return_yes}";
  fi;
  eval "${return_no}";
}


########################################################################
# is_non_empty_file (<file_name>)
#
# Test whether `file_name' is a non-empty existing file.
#
# Arguments : <=1
# Return    :
#   `0' if arg1 is a non-empty existing file
#   `1' otherwise
#
is_non_empty_file()
{
  func_check is_non_empty_file '=' 1 "$@";
  if is_file "$1" && test -s "$1"
  then
    eval "${return_yes}";
  fi;
  eval "${return_no}";
}


########################################################################
# is_not_dir (<name>)
#
# Test whether `name' is not a readable directory.
#
# Arguments : 1
# Return    : `0' if arg1 is a directory, `1' otherwise.
#
is_not_dir()
{
  func_check is_not_dir '=' 1 "$@";
  if is_dir "$1"
  then
    eval "${return_no}";
  fi;
  eval "${return_yes}";
}


########################################################################
# is_not_empty (<string>)
#
# Test whether `string' is not empty.
#
# Arguments : <=1
# Return    : `0' if arg1 exists and is not empty, `1' otherwise.
#
is_not_empty()
{
  func_check is_not_empty '=' 1 "$@";
  if is_empty "$1"
  then
    eval "${return_no}";
  fi;
  eval "${return_yes}";
}


########################################################################
# is_not_equal (<string1> <string2>)
#
# Test whether `string1' differs from `string2'.
#
# Arguments : 2
#
is_not_equal()
{
  func_check is_not_equal '=' 2 "$@";
  if is_equal "$1" "$2"
  then
    eval "${return_no}";
  fi
  eval "${return_yes}";
}


########################################################################
# is_not_file (<filename>)
#
# Test whether `name' is a not readable file.
#
# Arguments : 1 (empty allowed)
#
is_not_file()
{
  func_check is_not_file '=' 1 "$@";
  if is_file "$1"
  then
    eval "${return_no}";
  fi;
  eval "${return_yes}";
}


########################################################################
# is_not_prog ([<name> [<arg>*]])
#
# Verify that arg is a not program in $PATH.
#
# Arguments : >=0 (empty allowed)
#   more args are ignored, this allows to specify progs with arguments
#
is_not_prog()
{
  func_check is_not_prog '>=' 0 "$@";
  case "$#" in
  0)
    eval "${return_yes}";
    ;;
  *)
    if where_is "$1" >${_NULL_DEV}
    then
      eval "${return_no}";
    fi;
    ;;
  esac
  eval "${return_yes}";
}


########################################################################
# is_not_writable (<name>)
#
# Test whether `name' is a not a writable file or directory.
#
# Arguments : >=1 (empty allowed), more args are ignored
#
is_not_writable()
{
  func_check is_not_writable '>=' 1 "$@";
  if is_writable "$1"
  then
    eval "${return_no}";
  fi;
  eval "${return_yes}";
}


########################################################################
# is_not_X ()
#
# Test whether not running in X Window by checking $DISPLAY
#
is_not_X()
{
  func_check is_X '=' 0 "$@";
  if obj DISPLAY is_empty
  then
    eval "${return_yes}";
  fi;
  eval "${return_no}";
}


########################################################################
# is_not_yes (<string>)
#
# Test whether `string' is not "yes".
#
# Arguments : 1
#
is_not_yes()
{
  func_check is_not_yes = 1 "$@";
  if is_yes "$1"
  then
    eval "${return_no}";
  fi;
  eval "${return_yes}";
}


########################################################################
# is_prog ([<name> [<arg>*]])
#
# Determine whether <name> is a program in $PATH
#
# Arguments : >=0 (empty allowed)
#   <arg>* are ignored, this allows to specify progs with arguments.
#
is_prog()
{
  func_check is_prog '>=' 0 "$@";
  case "$#" in
  0)
    eval "${return_no}";
    ;;
  *)
    if where_is "$1" >${_NULL_DEV}
    then
      eval "${return_yes}";
    fi;
    ;;
  esac
  eval "${return_no}";
}


########################################################################
# is_writable (<name>)
#
# Test whether `name' is a writable file or directory.
#
# Arguments : >=1 (empty allowed), more args are ignored
#
is_writable()
{
  func_check is_writable '>=' 1 "$@";
  if test _"$1"_ = __
  then
    eval "${return_no}";
  fi;
  if test -r "$1"
  then
    if test -w "$1"
    then
      eval "${return_yes}";
    fi;
  fi;
  eval "${return_no}";
}


########################################################################
# is_X ()
#
# Test whether running in X Window by checking $DISPLAY
#
is_X()
{
  func_check is_X '=' 0 "$@";
  if obj DISPLAY is_not_empty
  then
    eval "${return_yes}";
  fi;
  eval "${return_no}";
}


########################################################################
# is_yes (<string>)
#
# Test whether `string' has value "yes".
#
# Return    : `0' if arg1 is `yes', `1' otherwise.
#
is_yes()
{
  func_check is_yes '=' 1 "$@";
  if is_equal "$1" 'yes'
  then
    eval "${return_yes}";
  fi;
  eval "${return_no}";
}


########################################################################
# landmark ()
#
# Print debugging information on standard error if $_DEBUG_LM is `yes'.
#
# Globals: $_DEBUG_LM
#
# Defined in section `Debugging functions'.


########################################################################
# leave ([<code>])
#
# Clean exit without an error or with <code>.
#
leave()
{
  clean_up;
  if test $# = 0
  then
    exit "${_OK}";
  else
    exit "$1";
  fi;
}


########################################################################
landmark '6: list_*()';
########################################################################
#
# `list' is an object class that represents an array or list.  Its
# data consists of space-separated single-quoted elements.  So a list
# has the form "'first' 'second' '...' 'last'".  See list_append() for
# more details on the list structure.  The array elements of `list'
# can be get by `eval set x "$list"; shift`.


########################################################################
# list_append (<list> <element>...)
#
# Arguments: >=2
#   <list>: a variable name for a list of single-quoted elements
#   <element>:  some sequence of characters.
# Output: none, but $<list> is set to
#   if <list> is empty:  "'<element>' '...'"
#   otherwise:           "$list '<element>' ..."
#
# Variable prefix: la
#
list_append()
{
  func_check list_append '>=' 2 "$@";
  la_name="$1";
  eval la_list='"${'$1'}"';
  shift;
  for s
  do
    la_s="$s";
    case "${la_s}" in
    *\'*)
      # escape each single quote by replacing each
      # "'" (squote) by "'\''" (squote bslash squote squote);
      # note that the backslash must be doubled in the following `sed'
      la_element="$(echo1 "${la_s}" | sed -e 's/'"${_SQ}"'/&\\&&/g')";
      exit_test;
      ;;
    '')
      la_element="";
      ;;
    *)
      la_element="${la_s}";
      ;;
    esac;
    if obj la_list is_empty
    then
      la_list="'${la_element}'";
    else
      la_list="${la_list} '${la_element}'";
    fi;
  done;
  eval "${la_name}"='"${la_list}"';
  eval ${_UNSET} la_element;
  eval ${_UNSET} la_list;
  eval ${_UNSET} la_name;
  eval ${_UNSET} la_s;
  eval "${return_ok}";
}


########################################################################
# list_from_cmdline (<pre_name_of_opt_lists> [<cmdline_arg>...])
#
# Transform command line arguments into a normalized form.
#
# Options, option arguments, and file parameters are identified and
# output each as a single-quoted argument of its own.  Options and
# file parameters are separated by a '--' argument.
#
# Arguments: >=1
#   <pre_name>: common part of a set of 4 environment variable names:
#     $<pre_name>_SHORT_NA:  list of short options without an arg.
#     $<pre_name>_SHORT_ARG: list of short options that have an arg.
#     $<pre_name>_LONG_NA:   list of long options without an arg.
#     $<pre_name>_LONG_ARG:  list of long options that have an arg.
#   <cmdline_arg>...: the arguments from a command line, such as "$@",
#                     the content of a variable, or direct arguments.
#
# Output: ['-[-]opt' ['optarg']]... '--' ['filename']...
#
# Example:
#   list_from_cmdline PRE 'a b' 'c' '' 'long' -a f1 -bcarg --long=larg f2
# If $PRE_SHORT_NA, $PRE_SHORT_ARG, $PRE_LONG_NA, and $PRE_LONG_ARG are
# none-empty option lists, this will result in printing:
#     '-a' '-b' '-c' 'arg' '--long' 'larg' '--' 'f1' 'f2'
#
#   Use this function in the following way:
#     eval set x "$(args_norm PRE_NAME "$@")";
#     shift;
#     while test "$1" != '--'; do
#       case "$1" in
#       ...
#       esac;
#       shift;
#     done;
#     shift;         #skip '--'
#     # all positional parameters ("$@") left are file name parameters.
#
# Variable prefix: lfc
#
list_from_cmdline()
{
  func_check list_from_cmdline '>=' 1 "$@";
  lfc_short_n="$(obj_data "$1"_SHORT_NA)";  # short options, no argument
  lfc_short_a="$(obj_data "$1"_SHORT_ARG)"; # short options, with argument
  lfc_long_n="$(obj_data "$1"_LONG_NA)";    # long options, no argument
  lfc_long_a="$(obj_data "$1"_LONG_ARG)";   # long options, with argument
  exit_test;
  if obj lfc_short_n is_empty
  then
    error 'list_from_cmdline(): no $'"$1"'_SHORT_NA options.';
  fi;
  if obj lfc_short_a is_empty
  then
    error 'list_from_cmdline(): no $'"$1"'_SHORT_ARG options.';
  fi;
  if obj lfc_long_n is_empty
  then
    error 'list_from_cmdline(): no $'"$1"'_LONG_NA options.';
  fi;
  if obj lfc_long_a is_empty
  then
    error 'list_from_cmdline(): no $'"$1"'_LONG_ARG options.';
  fi;

  shift;
  if is_equal "$#" 0
  then
    echo1 --
    eval ${_UNSET} lfc_fparams;
    eval ${_UNSET} lfc_short_a;
    eval ${_UNSET} lfc_short_n;
    eval ${_UNSET} lfc_long_a;
    eval ${_UNSET} lfc_long_n;
    eval ${_UNSET} lfc_result;
    eval "${return_ok}";
  fi;

  lfc_fparams='';
  lfc_result='';
  while test "$#" -ge 1
  do
    lfc_arg="$1";
    shift;
    case "${lfc_arg}" in
    --) break; ;;
    --*=*)
      # delete leading '--';
      lfc_abbrev="$(echo1 "${lfc_arg}" | sed -e 's/^--//')";
      lfc_with_equal="${lfc_abbrev}";
      # extract option by deleting from the first '=' to the end
      lfc_abbrev="$(echo1 "${lfc_with_equal}" | \
                    sed -e 's/^\([^=]*\)=.*$/\1/')";
      lfc_opt="$(list_single_from_abbrev lfc_long_a "${lfc_abbrev}")";
      exit_test;
      if obj lfc_opt is_empty
      then
        error_user "--${lfc_abbrev} is not an option.";
      else
        # get the option argument by deleting up to first `='
        lfc_optarg="$(echo1 "${lfc_with_equal}" | sed -e 's/^[^=]*=//')";
        exit_test;
        list_append lfc_result "--${lfc_opt}" "${lfc_optarg}";
        continue;
      fi;
      ;;
    --*)
      # delete leading '--';
      lfc_abbrev="$(echo1 "${lfc_arg}" | sed -e 's/^--//')";
      if list_has lfc_long_n "${lfc_abbrev}"
      then
        lfc_opt="${lfc_abbrev}";
      else
        exit_test;
        lfc_opt="$(list_single_from_abbrev lfc_long_n "${lfc_abbrev}")";
        exit_test;
        if obj lfc_opt is_not_empty && is_not_equal "$#" 0
        then
          a="$(list_single_from_abbrev lfc_long_a "${lfc_abbrev}")";
          exit_test;
          if obj a is_not_empty
          then
            error_user "The abbreviation ${lfc_arg} \
has multiple options: --${lfc_opt} and --${a}.";
          fi;
        fi;
      fi;
      if obj lfc_opt is_not_empty
      then
        # long option, no argument
        list_append lfc_result "--${lfc_opt}";
        continue;
      fi;
      lfc_opt="$(list_single_from_abbrev lfc_long_a "${lfc_abbrev}")";
      exit_test;
      if obj lfc_opt is_not_empty
      then
        # long option with argument
        if test "$#" -le 0
        then
          error_user "no argument for option --${lfc_opt}."
        fi;
        list_append lfc_result "--${lfc_opt}" "$1";
        shift;
        continue;
      fi;
      error_user "${lfc_arg} is not an option.";
      ;;
    -?*)			# short option (cluster)
      # delete leading `-';
      lfc_rest="$(echo1 "${lfc_arg}" | sed -e 's/^-//')";
      exit_test;
      while obj lfc_rest is_not_empty
      do
        # get next short option from cluster (first char of $lfc_rest)
        lfc_optchar="$(echo1 "${lfc_rest}" | sed -e 's/^\(.\).*$/\1/')";
        # remove first character from ${lfc_rest};
        lfc_rest="$(echo1 "${lfc_rest}" | sed -e 's/^.//')";
        exit_test;
        if list_has lfc_short_n "${lfc_optchar}"
        then
          list_append lfc_result "-${lfc_optchar}";
          continue;
        elif list_has lfc_short_a "${lfc_optchar}"
        then
          if obj lfc_rest is_empty
          then
            if test "$#" -ge 1
            then
              list_append lfc_result "-${lfc_optchar}" "$1";
              shift;
              continue;
            else
              error_user "no argument for option -${lfc_optchar}.";
            fi;
          else			# rest is the argument
            list_append lfc_result "-${lfc_optchar}" "${lfc_rest}";
            lfc_rest='';
            continue;
          fi;
        else
          error_user "unknown option -${lfc_optchar}.";
        fi;
      done;
      ;;
    *)
      # Here, $lfc_arg is not an option, so a file parameter.
      list_append lfc_fparams "${lfc_arg}";

      # Ignore the strange POSIX option handling to end option
      # parsing after the first file name argument.  To reuse it, do
      # a `break' here if $POSIXLY_CORRECT of `bash' is not empty.
      # When `bash' is called as `sh' $POSIXLY_CORRECT is set
      # automatically to `y'.
      ;;
    esac;
  done;
  list_append lfc_result '--';
  if obj lfc_fparams is_not_empty
  then
    lfc_result="${lfc_result} ${lfc_fparams}";
  fi;
  if test "$#" -gt 0
  then
    list_append lfc_result "$@";
  fi;
  obj lfc_result echo1;
  eval ${_UNSET} lfc_abbrev;
  eval ${_UNSET} lfc_fparams;
  eval ${_UNSET} lfc_short_a;
  eval ${_UNSET} lfc_short_n;
  eval ${_UNSET} lfc_long_a;
  eval ${_UNSET} lfc_long_n;
  eval ${_UNSET} lfc_result;
  eval ${_UNSET} lfc_arg;
  eval ${_UNSET} lfc_opt;
  eval ${_UNSET} lfc_opt_arg;
  eval ${_UNSET} lfc_opt_char;
  eval ${_UNSET} lfc_with_equal;
  eval ${_UNSET} lfc_rest;
  eval "${return_ok}";
} # list_from_cmdline()


########################################################################
# list_from_split (<string> <separator>)
#
# In <string>, escape all white space characters and replace each
# <separator> by space.
#
# Arguments: 2: a <string> that is to be split into parts divided by
#               <separator>
# Output:    the resulting list string
#
# Variable prefix: lfs
#
list_from_split()
{
  func_check list_from_split = 2 "$@";

  # precede each space or tab by a backslash `\' (doubled for `sed')
  lfs_s="$(echo1 "$1" | sed -e 's/\('"${_SPACE_SED}"'\)/\\\1/g')";
  exit_test;

  # replace split character of string by the list separator ` ' (space).
  case "$2" in
    /)				# cannot use normal `sed' separator
      echo1 "${lfs_s}" | sed -e 's|'"$2"'| |g';
      ;;
    ?)				# use normal `sed' separator
      echo1 "${lfs_s}" | sed -e 's/'"$2"'/ /g';
      ;;
    ??*)
      error 'list_from_split(): separator must be a single character.';
      ;;
  esac;
  eval ${_UNSET} lfs_s;
  eval "${return_ok}";
}


########################################################################
# list_get (<list>)
#
# Check whether <list> is a space-separated list of '-quoted elements.
#
# If the test fails an error is raised.
# If the test succeeds the argument is echoed.
#
# Testing criteria:
#   A list has the form "'first' 'second' '...' 'last'".  So it has a
#   leading and a final quote and the elements are separated by "' '"
#   constructs.  If these are all removed there should not be any
#   unescaped single-quotes left.  Watch out for escaped single
#   quotes; they have the form '\'' (sq bs sq sq).

# Arguments: 1
# Output: the argument <list> unchanged, if the check succeeded.
#
# Variable prefix: lg
#
list_get()
{
  func_check list_get = 1 "$@";
  eval lg_list='"${'$1'}"';
  # remove leading and final space characters
  lg_list="$(echo1 "${lg_list}" | sed -e '
s/^'"${_SPACE_SED}"'*//
s/'"${_SPACE_SED}"'*$//
')";
  exit_test;
  case "${lg_list}" in
  '')
    eval ${_UNSET} lg_list;
    eval "${return_ok}";
    ;;
  \'*\')
    obj lg_list echo1;
    eval ${_UNSET} lg_list;
    eval "${return_ok}";
    ;;
  *)
    error "list_get(): bad list: $1"
    ;;
  esac;
  eval ${_UNSET} lg_list;
  eval "${return_ok}";
}


########################################################################
# list_has (<var_name> <element>)
#
# Test whether the list <var_name> has the element <element>.
#
# Arguments: 2
#   <var_name>: a variable name for a list of single-quoted elements
#   <element>:  some sequence of characters.
#
# Variable prefix: lh
#
list_has()
{
  func_check list_has = 2 "$@";
  eval lh_list='"${'$1'}"';
  if obj lh_list is_empty
  then
    eval "${_UNSET}" lh_list;
    eval "${return_no}";
  fi;
  case "$2" in
    \'*\')  lh_element=" $2 "; ;;
    *)      lh_element=" '$2' "; ;;
  esac;
  if string_contains " ${lh_list} " "${lh_element}"
  then
    eval "${_UNSET}" lh_list;
    eval "${_UNSET}" lh_element;
    eval "${return_yes}";
  else
    eval "${_UNSET}" lh_list;
    eval "${_UNSET}" lh_element;
    eval "${return_no}";
  fi;
}


########################################################################
# list_has_abbrev (<var_name> <abbrev>)
#
# Test whether the list <var_name> has an element starting with <abbrev>.
#
# Arguments: 2
#   <var_name>: a variable name for a list of single-quoted elements
#   <abbrev>:   some sequence of characters.
#
# Variable prefix: lha
#
list_has_abbrev()
{
  func_check list_has_abbrev = 2 "$@";
  eval lha_list='"${'$1'}"';
  if obj lha_list is_empty
  then
    eval "${_UNSET}" lha_list;
    eval "${return_no}";
  fi;
  case "$2" in
    \'*)
      lha_element="$(echo1 "$2" | sed -e 's/'"${_SQ}"'$//')";
      exit_test;
      ;;
    *) lha_element="'$2"; ;;
  esac;
  if string_contains " ${lha_list}" " ${lha_element}"
  then
    eval "${_UNSET}" lha_list;
    eval "${_UNSET}" lha_element;
    eval "${return_yes}";
  else
    eval "${_UNSET}" lha_list;
    eval "${_UNSET}" lha_element;
    eval "${return_no}";
  fi;
  eval "${return_ok}";
}


########################################################################
# list_has_not (<list> <element>)
#
# Test whether <list> has no <element>.
#
# Arguments: 2
#   <list>:    a space-separated list of single-quoted elements.
#   <element>: some sequence of characters.
#
# Variable prefix: lhn
#
list_has_not()
{
  func_check list_has_not = 2 "$@";
  eval lhn_list='"${'$1'}"';
  if obj lhn_list is_empty
  then
    eval "${_UNSET}" lhn_list;
    eval "${return_yes}";
  fi;
  case "$2" in
    \'*\') lhn_element=" $2 "; ;;
    *)     lhn_element=" '$2' "; ;;
  esac;
  if string_contains " ${lhn_list} " "${lhn_element}"
  then
    eval "${_UNSET}" lhn_list;
    eval "${_UNSET}" lhn_element;
    eval "${return_no}";
  else
    eval "${_UNSET}" lhn_list;
    eval "${_UNSET}" lhn_element;
    eval "${return_yes}";
  fi;
}


########################################################################
# list_single_from_abbrev (<list> <abbrev>)
#
# Check whether the list has an element starting with <abbrev>.  If
# there are more than a single element an error is created.
#
# Arguments: 2
#   <list>:   a variable name for a list of single-quoted elements
#   <abbrev>: some sequence of characters.
#
# Output: the found element.
#
# Variable prefix: lsfa
#
list_single_from_abbrev()
{
  func_check list_single_from_abbrev = 2 "$@";
  eval lsfa_list='"${'$1'}"';
  if obj lsfa_list is_empty
  then
    eval "${_UNSET}" lsfa_list;
    eval "${return_no}";
  fi;
  lsfa_abbrev="$2";
  if list_has lsfa_list "${lsfa_abbrev}"
  then
    obj lsfa_abbrev echo1;
    eval "${_UNSET}" lsfa_abbrev;
    eval "${_UNSET}" lsfa_list;
    eval "${return_yes}";
  fi;
  if list_has_abbrev lsfa_list "${lsfa_abbrev}"
  then
    lsfa_element='';
    eval set x "${lsfa_list}";
    shift;
    for i
    do
      case "$i" in
      ${lsfa_abbrev}*)
        if obj lsfa_element is_not_empty
        then
          error_user "The abbreviation --${lsfa_abbrev} \
has multiple options: --${lsfa_element} and --${i}.";
        fi;
        lsfa_element="$i";
        ;;
      esac;
    done;
    obj lsfa_element echo1;
    eval "${_UNSET}" lsfa_abbrev;
    eval "${_UNSET}" lsfa_element;
    eval "${_UNSET}" lsfa_list;
    eval "${return_yes}";
  else
    eval "${_UNSET}" lsfa_abbrev;
    eval "${_UNSET}" lsfa_element;
    eval "${_UNSET}" lsfa_list;
    eval "${return_no}";
  fi;
}


########################################################################
landmark '7: man_*()';
########################################################################

########################################################################
# man_do_filespec (<filespec>)
#
# Print suitable man page(s) for filespec to $_TMP_CAT.
#
# Arguments : 2
#   <filespec>: argument of the form `man:name.section', `man:name',
#               `man:name(section)', `name.section', `name'.
#
# Globals   : $_OPT_ALL
#
# Output    : none.
# Return    : `0' if man page was found, `1' else.
#
# Only called from do_fileargs(), checks on $MANPATH and $_MAN_ENABLE
# are assumed (see man_setup()).
#
# Variable prefix: mdf
#
man_do_filespec()
{
  func_check man_do_filespec = 1 "$@";
  if obj _MAN_PATH is_empty
  then
    eval "${return_bad}";
  fi;
  if is_empty "$1"
  then
    eval "${return_bad}";
  fi;
  mdf_spec="$1";
  mdf_name='';
  mdf_section='';
  case "${mdf_spec}" in
  */*)				# not a man spec with containing '/'
    eval ${_UNSET} mdf_got_one;
    eval ${_UNSET} mdf_name;
    eval ${_UNSET} mdf_section;
    eval ${_UNSET} mdf_spec;
    eval "${return_bad}";
    ;;
  man:?*\(?*\))			# man:name(section)
    mdf_name="$(echo1 "${mdf_spec}" \
                | sed -e 's/^man:\(..*\)(\(..*\))$/\1/')";
    mdf_section="$(echo1 "${mdf_spec}" \
                   | sed -e 's/^man:\(..*\)(\(..*\))$/\2/')";
    exit_test;
    ;;
  man:?*.${_MAN_AUTO_SEC_CHARS}) # man:name.section
    mdf_name="$(echo1 "${mdf_spec}" \
                | sed -e 's/^man:\(..*\)\..$/\1/')";
    mdf_section="$(echo1 "${mdf_spec}" \
                   | sed -e 's/^.*\(.\)$/\1/')";
    exit_test;
    ;;
  man:?*)			# man:name
    mdf_name="$(echo1 "${mdf_spec}" | sed -e 's/^man://')";
    exit_test;
    ;;
  ?*\(?*\))			# name(section)
    mdf_name="$(echo1 "${mdf_spec}" \
                | sed -e 's/^\(..*\)(\(..*\))$/\1/')";
    mdf_section="$(echo1 "${mdf_spec}" \
                   | sed -e 's/^\(..*\)(\(..*\))$/\2/')";
    exit_test;
    ;;
  ?*.${_MAN_AUTO_SEC_CHARS})	# name.section
    mdf_name="$(echo1 "${mdf_spec}" \
                | sed -e 's/^\(..*\)\..$/\1/')";
    mdf_section="$(echo1 "${mdf_spec}" \
                   | sed -e 's/^.*\(.\)$/\1/')";
    exit_test;
    ;;
  ?*)
    mdf_name="${mdf_spec}";
    ;;
  esac;
  if obj mdf_name is_empty
  then
    eval ${_UNSET} mdf_got_one;
    eval ${_UNSET} mdf_name;
    eval ${_UNSET} mdf_section;
    eval ${_UNSET} mdf_spec;
    eval "${return_bad}";
  fi;
  mdf_got_one='no';
  if obj mdf_section is_empty
  then
    if obj _OPT_SECTIONS is_empty
    then
      eval set x "${_MAN_AUTO_SEC_LIST}";
    else
      # use --sections when no section is given to filespec
      eval set x "$(echo1 "${_OPT_SECTIONS}" | sed -e 's/:/ /g')";
    fi;
    shift;
    for s
    do
      mdf_s="$s";
      if man_search_section "${mdf_name}" "${mdf_s}"
      then			# found
        if obj _MAN_ALL is_yes
        then
          mdf_got_one='yes';
        else
          eval ${_UNSET} mdf_got_one;
          eval ${_UNSET} mdf_name;
          eval ${_UNSET} mdf_s;
          eval ${_UNSET} mdf_section;
          eval ${_UNSET} mdf_spec;
          eval "${return_good}";
        fi;
      fi;
    done;
  else
    if man_search_section "${mdf_name}" "${mdf_section}"
    then
      eval ${_UNSET} mdf_got_one;
      eval ${_UNSET} mdf_name;
      eval ${_UNSET} mdf_s;
      eval ${_UNSET} mdf_section;
      eval ${_UNSET} mdf_spec;
      eval "${return_good}";
    else
      eval ${_UNSET} mdf_got_one;
      eval ${_UNSET} mdf_name;
      eval ${_UNSET} mdf_section;
      eval ${_UNSET} mdf_spec;
      eval "${return_bad}";
    fi;
  fi;
  if obj _MAN_ALL is_yes && obj mdf_got_one is_yes
  then
    eval ${_UNSET} mdf_got_one;
    eval ${_UNSET} mdf_name;
    eval ${_UNSET} mdf_s;
    eval ${_UNSET} mdf_section;
    eval ${_UNSET} mdf_spec;
    eval "${return_good}";
  fi;
  eval ${_UNSET} mdf_got_one;
  eval ${_UNSET} mdf_name;
  eval ${_UNSET} mdf_s;
  eval ${_UNSET} mdf_section;
  eval ${_UNSET} mdf_spec;
  eval "${return_bad}";
} # man_do_filespec()


########################################################################
# man_register_file (<file> <name> [<section>])
#
# Write a found man page file and register the title element.
#
# Arguments: 1, 2, or 3; maybe empty
# Output: none
#
man_register_file()
{
  func_check man_register_file '>=' 2 "$@";
  case "$#" in
    2|3) do_nothing; ;;
    *)
      error "man_register_file() expects 2 or 3 arguments.";
      ;;
  esac;
  if is_empty "$1"
  then
    error 'man_register_file(): file name is empty';
  fi;
  to_tmp "$1";
  case "$#" in
    2)
       register_title "man:$2";
       eval "${return_ok}";
       ;;
    3)
       register_title "$2.$3";
       eval "${return_ok}";
       ;;
  esac;
  eval "${return_ok}";
}


########################################################################
# man_search_section (<name> <section>)
#
# Retrieve man pages.
#
# Arguments : 2
# Globals   : $_MAN_PATH, $_MAN_EXT
# Return    : 0 if found, 1 otherwise
#
# Variable prefix: mss
#
man_search_section()
{
  func_check man_search_section = 2 "$@";
  if obj _MAN_PATH is_empty
  then
    eval "${return_bad}";
  fi;
  if is_empty "$1"
  then
    eval "${return_bad}";
  fi;
  if is_empty "$2"
  then
    eval "${return_bad}";
  fi;
  mss_name="$1";
  mss_section="$2";
  eval set x "$(path_split "${_MAN_PATH}")";
  exit_test;
  shift;
  mss_got_one='no';
  if obj _MAN_EXT is_empty
  then
    for d
    do
      mss_dir="$(dirname_append "$d" "man${mss_section}")";
      exit_test;
      if obj mss_dir is_dir
      then
        mss_prefix="$(\
          dirname_append "${mss_dir}" "${mss_name}.${mss_section}")";
        if obj _OPT_WHATIS is_yes
        then
          mss_files="$(eval ls "${mss_prefix}"'*' 2>${_NULL_DEV} |
                       sed -e '\| found|s|.*||'
                       )";
        else
          mss_files="$(eval ls "'${mss_prefix}'"'*' 2>${_NULL_DEV} |
                       sed -e '\| found|s|.*||'
                       )";
        fi;
        exit_test;
        if obj mss_files is_not_empty
        then
          # for f in $mss_files
          for f in $(eval set x ${mss_files}; shift; echo1 "$@")
          do
            exit_test;
            mss_f="$f";
            if obj mss_f is_file
            then
              if is_yes "${mss_got_one}"
              then
                register_file "${mss_f}";
              elif obj _MAN_ALL is_yes
              then
                man_register_file "${mss_f}" "${mss_name}";
              else
                man_register_file "${mss_f}" "${mss_name}" "${mss_section}";
                eval ${_UNSET} mss_dir;
                eval ${_UNSET} mss_ext;
                eval ${_UNSET} mss_f;
                eval ${_UNSET} mss_files;
                eval ${_UNSET} mss_got_one;
                eval ${_UNSET} mss_name;
                eval ${_UNSET} mss_prefix;
                eval ${_UNSET} mss_section;
                eval "${return_good}";
              fi;
              mss_got_one='yes';
            fi;
          done;
        fi;
      fi;
    done;
  else
    mss_ext="${_MAN_EXT}";
    # check for directory name having trailing extension
    for d
    do
      mss_dir="$(dirname_append $d man${mss_section}${mss_ext})";
      exit_test;
      if obj mss_dir is_dir
      then
        mss_prefix=\
          "$(dirname_append "${mss_dir}" "${mss_name}.${mss_section}")";
        mss_files="$( eval ls "${mss_prefix}"'*' 2>${_NULL_DEV} |
                     sed -e '\|not found|s|.*||'
                     )";
        exit_test;
        if obj mss_files is_not_empty
        then
          # for f in $mss_files
          for f in $(eval set x ${mss_files}; shift; echo1 "$@")
          do
            mss_f="$f";
            if obj mss_f is_file
            then
              if is_yes "${mss_got_one}"
              then
                register_file "${mss_f}";
              elif obj _MAN_ALL is_yes
              then
                man_register_file "${mss_f}" "${mss_name}";
              else
                man_register_file "${mss_f}" "${mss_name}" "${mss_section}";
                eval ${_UNSET} mss_dir;
                eval ${_UNSET} mss_ext;
                eval ${_UNSET} mss_f;
                eval ${_UNSET} mss_files;
                eval ${_UNSET} mss_got_one;
                eval ${_UNSET} mss_name;
                eval ${_UNSET} mss_prefix;
                eval ${_UNSET} mss_section;
                eval "${return_good}";
              fi;
              mss_got_one='yes';
            fi;
          done;
        fi;
      fi;
    done;
    # check for files with extension in directories without extension
    for d
    do
      mss_dir="$(dirname_append "$d" "man${mss_section}")";
      exit_test;
      if obj mss_dir is_dir
      then
        mss_prefix="$(dirname_append "${mss_dir}" \
                        "${mss_name}.${mss_section}${mss_ext}")";
        mss_files="$(eval ls "${mss_prefix}"'*' 2>${_NULL_DEV} |
                     sed -e '\|not found|s|.*||'
                     )";
        exit_test;
        if obj mss_files is_not_empty
        then
          # for f in $mss_files
          for f in $(eval set x ${mss_files}; shift; echo1 "$@")
          do
            mss_f="$f";
            if obj mss_f is_file
            then
              if is_yes "${mss_got_one}"
              then
                register_file "${mss_f}";
              elif obj _MAN_ALL is_yes
              then
                man_register_file "${mss_f}" "${mss_name}";
              else
                man_register_file "${mss_f}" "${mss_name}" "${mss_section}";
                eval ${_UNSET} mss_dir;
                eval ${_UNSET} mss_ext;
                eval ${_UNSET} mss_f;
                eval ${_UNSET} mss_files;
                eval ${_UNSET} mss_got_one;
                eval ${_UNSET} mss_name;
                eval ${_UNSET} mss_prefix;
                eval ${_UNSET} mss_section;
                eval "${return_good}";
              fi;
              mss_got_one='yes';
            fi;
          done;
        fi;
      fi;
    done;
  fi;
  if obj _MAN_ALL is_yes && is_yes "${mss_got_one}"
  then
    eval ${_UNSET} mss_dir;
    eval ${_UNSET} mss_ext;
    eval ${_UNSET} mss_f;
    eval ${_UNSET} mss_files;
    eval ${_UNSET} mss_got_one;
    eval ${_UNSET} mss_name;
    eval ${_UNSET} mss_prefix;
    eval ${_UNSET} mss_section;
    eval "${return_good}";
  fi;
  eval ${_UNSET} mss_dir;
  eval ${_UNSET} mss_ext;
  eval ${_UNSET} mss_f;
  eval ${_UNSET} mss_files;
  eval ${_UNSET} mss_got_one;
  eval ${_UNSET} mss_name;
  eval ${_UNSET} mss_prefix;
  eval ${_UNSET} mss_section;
  eval "${return_bad}";
} # man_search_section()


########################################################################
# man_setup ()
#
# Setup the variables $_MAN_* needed for man page searching.
#
# Globals:
#   in:     $_OPT_*, $_MANOPT_*, $LANG, $LC_MESSAGES, $LC_ALL,
#           $MANPATH, $MANROFFSEQ, $MANSEC, $PAGER, $SYSTEM, $MANOPT.
#   out:    $_MAN_PATH, $_MAN_LANG, $_MAN_SYS, $_MAN_LANG, $_MAN_LANG2,
#           $_MAN_SEC, $_MAN_ALL
#   in/out: $_MAN_ENABLE
#
# The precedence for the variables related to `man' is that of GNU
# `man', i.e.
#
# $LANG; overridden by
# $LC_MESSAGES; overridden by
# $LC_ALL; this has the same precedence as
# $MANPATH, $MANROFFSEQ, $MANSEC, $PAGER, $SYSTEM; overridden by
# $MANOPT; overridden by
# the groffer command line options.
#
# Variable prefix: ms
#
man_setup()
{
  func_check main_man_setup = 0 "$@";

  if obj _MAN_IS_SETUP is_yes
  then
    eval "${return_ok}";
  fi;
  _MAN_IS_SETUP='yes';

  if obj _MAN_ENABLE is_not_yes
  then
    eval "${return_ok}";
  fi;

  # determine basic path for man pages
  _MAN_PATH="$(get_first_essential \
               "${_OPT_MANPATH}" "${_MANOPT_PATH}" "${MANPATH}")";
  exit_test;
  if obj _MAN_PATH is_empty
  then
    manpath_set_from_path;
  else
    _MAN_PATH="$(path_clean "${_MAN_PATH}")";
    exit_test;
  fi;
  if obj _MAN_PATH is_empty
  then
    if is_prog 'manpath'
    then
      _MAN_PATH="$(manpath 2>${_NULL_DEV})"; # not always available
      exit_test;
    fi;
  fi;
  if obj _MAN_PATH is_empty
  then
    _MAN_ENABLE="no";
    eval "${return_ok}";
  fi;

  _MAN_ALL="$(get_first_essential "${_OPT_ALL}" "${_MANOPT_ALL}")";
  exit_test;
  if obj _MAN_ALL is_empty
  then
    _MAN_ALL='no';
  fi;

  _MAN_SYS="$(get_first_essential \
              "${_OPT_SYSTEMS}" "${_MANOPT_SYS}" "${SYSTEM}")";
  ms_lang="$(get_first_essential \
           "${_OPT_LANG}" "${LC_ALL}" "${LC_MESSAGES}" "${LANG}")";
  exit_test;
  case "${ms_lang}" in
    C|POSIX)
      _MAN_LANG="";
      _MAN_LANG2="";
      ;;
    ?)
      _MAN_LANG="${ms_lang}";
      _MAN_LANG2="";
      ;;
    *)
      _MAN_LANG="${ms_lang}";
      # get first two characters of $ms_lang
      _MAN_LANG2="$(echo1 "${ms_lang}" | sed -e 's/^\(..\).*$/\1/')";
      exit_test;
      ;;
  esac;
  # from now on, use only $_LANG, forget about $_OPT_LANG, $LC_*.

  manpath_add_lang_sys;		# this is very slow

  _MAN_SEC="$(get_first_essential \
              "${_OPT_SECT}" "${_MANOPT_SEC}" "${MANSEC}")";
  exit_test;
  if obj _MAN_PATH is_empty
  then
    _MAN_ENABLE="no";
    eval ${_UNSET} ms_lang;
    eval "${return_ok}";
  fi;

  _MAN_EXT="$(get_first_essential \
              "${_OPT_EXTENSION}" "${_MANOPT_EXTENSION}")";
  exit_test;
  eval ${_UNSET} ms_lang;
  eval "${return_ok}";
} # man_setup()


########################################################################
landmark '8: manpath_*()';
########################################################################

########################################################################
# manpath_add_lang_sys ()
#
# Add language and operating system specific directories to man path.
#
# Arguments : 0
# Output    : none
# Globals:
#   in:     $_MAN_SYS: has the form `os1,os2,...', a comma separated
#             list of names of operating systems.
#           $_MAN_LANG and $_MAN_LANG2: each a single name
#   in/out: $_MAN_PATH: has the form `dir1:dir2:...', a colon
#             separated list of directories.
#
# Variable prefix: mals
#
manpath_add_lang_sys()
{
  func_check manpath_add_lang_sys = 0 "$@";
  if obj _MAN_PATH is_empty
  then
    eval "${return_ok}";
  fi;
  # twice test both sys and lang
  eval set x "$(path_split "${_MAN_PATH}")";
  shift;
  exit_test;
  mals_mp='';
  for p
  do				# loop on man path directories
    mals_mp="$(_manpath_add_lang_sys_single "${mals_mp}" "$p")";
    exit_test;
  done;
  eval set x "$(path_split "${mals_mp}")";
  shift;
  exit_test;
  for p
  do				# loop on man path directories
    mals_mp="$(_manpath_add_lang_sys_single "${mals_mp}" "$p")";
    exit_test;
  done;
  _MAN_PATH="$(path_chop "${mals_mp}")";
  exit_test;
  eval ${_UNSET} mals_mp;
  eval "${return_ok}";
}


# To the directory in $1 append existing sys/lang subdirectories
# Function is necessary to split the OS list.
#
# globals: in: $_MAN_SYS, $_MAN_LANG, $_MAN_LANG2
# argument: 2: `man_path' and `dir'
# output: colon-separated path of the retrieved subdirectories
#
# Variable prefix: _mals
#
_manpath_add_lang_sys_single()
{
  func_check _manpath_add_lang_sys_single = 2 "$@";
  _mals_res="$1";
  _mals_parent="$2";
  eval set x "$(list_from_split "${_MAN_SYS}" ',')";
  shift;
  exit_test;
  for d in "$@" "${_MAN_LANG}" "${_MAN_LANG2}"
  do
    _mals_dir="$(dirname_append "${_mals_parent}" "$d")";
    exit_test;
    if obj _mals_res path_not_contains "${_mals_dir}" && \
       obj _mals_dir is_dir
    then
      _mals_res="${_mals_res}:${_mals_dir}";
    fi;
  done;
  if path_not_contains "${_mals_res}" "${_mals_parent}"
  then
    _mals_res="${_mals_res}:${_mals_parent}";
  fi;
  path_chop "${_mals_res}";
  eval ${_UNSET} _mals_dir;
  eval ${_UNSET} _mals_parent;
  eval ${_UNSET} _mals_res;
  eval "${return_ok}";
}

# end manpath_add_lang_sys ()


########################################################################
# manpath_set_from_path ()
#
# Determine basic search path for man pages from $PATH.
#
# Return:    `0' if a valid man path was retrieved.
# Output:    none
# Globals:
#   in:  $PATH
#   out: $_MAN_PATH
#
# Variable prefix: msfp
#
manpath_set_from_path()
{
  func_check manpath_set_from_path = 0 "$@";

  msfp_manpath='';

  # get a basic man path from $PATH
  if obj PATH is_not_empty
  then
    eval set x "$(path_split "${PATH}")";
    shift;
    exit_test;
    for d
    do
      # delete the final `/bin' part
      msfp_base="$(echo1 "$d" | sed -e 's|//*bin/*$||')";
      exit_test;
      for e in /share/man /man
      do
        msfp_mandir="${msfp_base}$e";
        if test -d "${msfp_mandir}" && test -r "${msfp_mandir}"
        then
          msfp_manpath="${msfp_manpath}:${msfp_mandir}";
        fi;
      done;
    done;
  fi;

  # append some default directories
  for d in /usr/local/share/man /usr/local/man \
           /usr/share/man /usr/man \
           /usr/X11R6/man /usr/openwin/man \
           /opt/share/man /opt/man \
           /opt/gnome/man /opt/kde/man
  do
    msfp_d="$d";
    if obj msfp_manpath path_not_contains "${msfp_d}" && obj mfsp_d is_dir
    then
      msfp_manpath="${msfp_manpath}:${mfsp_d}";
    fi;
  done;

  _MAN_PATH="${msfp_manpath}";
  eval ${_UNSET} msfp_base;
  eval ${_UNSET} msfp_d;
  eval ${_UNSET} msfp_mandir;
  eval ${_UNSET} msfp_manpath;
  eval "${return_ok}";
} # manpath_set_from_path()


########################################################################
landmark '9: obj_*()';
########################################################################

########################################################################
# obj (<object> <call_name> <arg>...)
#
# This works like a method (object function) call for an object.
# Run "<call_name> $<object> <arg> ...".
#
# The first argument represents an object whose data is given as first
# argument to <call_name>().
#
# Argument: >=2
#           <object>: variable name
#           <call_name>: a program or function name
#
# Variable prefix: o
#
obj()
{
  func_check obj '>=' 2 "$@";
  eval o_arg1='"${'$1'}"';
  if is_empty "$2"
  then
    error "obj(): function name is empty."
  else
    o_func="$2";
  fi;
  shift;
  shift;
  eval "${o_func}"' "${o_arg1}" "$@"';
  n="$?";
  eval ${_UNSET} o_arg1;
  eval ${_UNSET} o_func;
  eval "${return_var} $n";
} # obj()


########################################################################
# obj_data (<object>)
#
# Print the data of <object>, i.e. the content of $<object>.
# For possible later extensions.
#
# Arguments: 1
#            <object>: a variable name
# Output:    the data of <object>
#
# Variable prefix: od
#
obj_data()
{
  func_check obj '=' 1 "$@";
  if is_empty "$1"
  then
    error "obj_data(): object name is empty."
  fi;
  eval od_res='"${'$1'}"';
  obj od_res echo1;
  eval ${_UNSET} od_res;
  eval "${return_ok}";
}


########################################################################
# obj_from_output (<object> <call_name> <arg>...)
#
# Run '$<object>="$(<call_name> <arg>...)"' to set the result of a
# function call to a global variable.
#
# Arguments: >=2
#            <object>: a variable name
#            <call_name>: the name of a function or program
#            <arg>: optional argument to <call_name>
# Output:    none
#
# Variable prefix: ofo
#
obj_from_output()
{
  func_check obj_from_output '>=' 2 "$@";
  if is_empty "$1"
  then
    error "res(): variable name is empty.";
  elif is_empty "$2"
  then
    error "res(): function name is empty."
  else
    ofo_result_name="$1";
  fi;
  shift;
  eval "${ofo_result_name}"'="$('"$@"')"';
  exit_test;
  eval "${return_ok}";
}


########################################################################
# obj_set (<object> <data>)
#
# Set the data of <object>, i.e. call "$<object>=<data>".
#
# Arguments: 2
#            <object>: a variable name
#            <data>: a string
# Output::   none
#
obj_set()
{
  func_check obj_set '=' 2 "$@";
  if is_empty "$1"
  then
    error "obj_set(): object name is empty."
  fi;
  eval "$1"='"$2"';
  eval "${return_ok}";
}


########################################################################
# path_chop (<path>)
#
# Remove unnecessary colons from path.
#
# Argument: 1, a colon separated path.
# Output:   path without leading, double, or trailing colons.
#
path_chop()
{
  func_check path_chop = 1 "$@";

  # replace multiple colons by a single colon `:'
  # remove leading and trailing colons
  echo1 "$1" | sed -e '
s/^:*//
s/:::*/:/g
s/:*$//
';
  eval "${return_ok}";
}


########################################################################
# path_clean (<path>)
#
# Remove non-existing directories from a colon-separated list.
#
# Argument: 1, a colon separated path.
# Output:   colon-separated list of existing directories.
#
# Variable prefix: pc
#
path_clean()
{
  func_check path_clean = 1 "$@";
  if is_not_equal "$#" 1
  then
    error 'path_clean() needs 1 argument.';
  fi;
  pc_arg="$1";
  eval set x "$(path_split "${pc_arg}")";
  exit_test;
  shift;
  pc_res="";
  for i
  do
    pc_i="$i";
    if obj pc_i is_not_empty \
       && obj pc_res path_not_contains "${pc_i}" \
       && obj pc_i is_dir
    then
      case "${pc_i}" in
      ?*/)
        pc_res="${pc_res}$(dirname_chop "${pc_i}")";
        exit_test;
        ;;
      *)
        pc_res="${pc_res}:${pc_i}";
        exit_test;
        ;;
      esac;
    fi;
  done;
  eval ${_UNSET} pc_arg;
  eval ${_UNSET} pc_i;
  eval ${_UNSET} pc_res;
  if path_chop "${pc_res}"
  then
    eval "${return_ok}";
  else
    eval "${return_bad}";
  fi;
}


########################################################################
# path_contains (<path> <dir>)
#-
# Test whether `dir' is contained in `path', a list separated by `:'.
#
# Arguments : 2 arguments.
# Return    : `0' if arg2 is substring of arg1, `1' otherwise.
#
path_contains()
{
  func_check path_contains = 2 "$@";
  case ":$1:" in
    *":$2:"*)
      eval "${return_yes}";
      ;;
    *)
      eval "${return_no}";
      ;;
  esac;
  eval "${return_ok}";
}


########################################################################
# path_not_contains (<path> <dir>)
#
# Test whether `dir' is not contained in colon separated `path'.
#
# Arguments : 2 arguments.
#
path_not_contains()
{
  func_check path_not_contains = 2 "$@";
  if path_contains "$1" "$2"
  then
    eval "${return_no}";
  else
    eval "${return_yes}";
  fi;
  eval "${return_ok}";
}


########################################################################
# path_split (<path>)
#
# In `path' escape white space and replace each colon by a space.
#
# Arguments: 1: a colon-separated path
# Output:    the resulting list, process with `eval set'
#
path_split()
{
  func_check path_split = 1 "$@";
  list_from_split "$1" ':';
  eval "${return_ok}";
}


########################################################################
landmark '10: register_*()';
########################################################################

########################################################################
# register_file (<filename>)
#
# Write a found file and register the title element.
#
# Arguments: 1: a file name
# Output: none
#
register_file()
{
  func_check register_file = 1 "$@";
  if is_empty "$1"
  then
    error 'register_file(): file name is empty';
  fi;
  if is_equal "$1" '-'
  then
    to_tmp "${_TMP_STDIN}";
    register_title 'stdin';
  else
    to_tmp "$1";
    register_title "$(base_name "$1")";
    exit_test;
  fi;
  eval "${return_ok}";
} # register_file()


########################################################################
# register_title (<filespec>)
#
# Create title element from <filespec> and append to $_REGISTERED_TITLE
#
# Globals: $_REGISTERED_TITLE (rw)
#
# Variable prefix: rt
#
register_title()
{
  func_check register_title '=' 1 "$@";
  if is_empty "$1"
  then
    eval "${return_ok}";
  fi;

  case "${_REGISTERED_TITLE}" in
  *\ *\ *\ *)
    eval "${return_ok}";
    ;;
  esac;

  # remove directory part
  rt_title="$(base_name "$1")";
  # replace space characters by `_'
  rt_title="$(echo1 "${rt_title}" | sed -e 's/[ 	]/_/g')";
  # remove extension `.bz2'
  rt_title="$(echo1 "${rt_title}" | sed -e 's/\.bz2$//')";
  # remove extension `.gz'
  rt_title="$(echo1 "${rt_title}" | sed -e 's/\.gz$//')";
  # remove extension `.Z'
  rt_title="$(echo1 "${rt_title}" | sed -e 's/\.Z$//')";
  exit_test;

  if obj rt_title is_empty
  then
    eval ${_UNSET} rt_title;
    eval "${return_ok}";
  fi;
  if obj _REGISTERED_TITLE is_empty
  then
    _REGISTERED_TITLE="${rt_title}";
  else
    _REGISTERED_TITLE="${_REGISTERED_TITLE} ${rt_title}";
  fi;
  eval ${_UNSET} rt_title;
  eval "${return_ok}";
} # register_title()


########################################################################
# reset ()
#
# Reset the variables that can be affected by options to their default.
#
#
# Defined in section `Preset' after the rudimentary shell tests.


########################################################################
# rm_file (<file_name>)
#
# Remove file if $_DEBUG_KEEP_FILES allows it.
#
# Globals: $_DEBUG_KEEP_FILES
#
rm_file()
{
  func_check rm_file '=' 1 "$@";
  if is_file "$1"
  then
    rm -f "$1" >${_NULL_DEV} 2>&1;
  fi;
  if is_existing "$1"
  then
    eval "${return_bad}";
  else
    eval "${return_good}";
  fi;
}


########################################################################
# rm_file_with_debug (<file_name>)
#
# Remove file if $_DEBUG_KEEP_FILES allows it.
#
# Globals: $_DEBUG_KEEP_FILES
#
rm_file_with_debug()
{
  func_check rm_file_with_debug '=' 1 "$@";
  if obj _DEBUG_KEEP_FILES is_not_yes
  then
    if is_file "$1"
    then
      rm -f "$1" >${_NULL_DEV} 2>&1;
    fi;
  fi;
  if is_existing "$1"
  then
    eval "${return_bad}";
  else
    eval "${return_good}";
  fi;
}


########################################################################
# rm_tree (<dir_name>)
#
# Remove file if $_DEBUG_KEEP_FILES allows it.
#
# Globals: $_DEBUG_KEEP_FILES
#
rm_tree()
{
  func_check rm_tree '=' 1 "$@";
  if is_existing "$1"
  then
    rm -f -r "$1" >${_NULL_DEV} 2>&1;
  fi; 
  if is_existing "$1"
  then
    eval "${return_bad}";
  else
    eval "${return_good}";
  fi;
}


########################################################################
# save_stdin ()
#
# Store standard input to temporary file (with decompression).
#
# Variable prefix: ss
#
if obj _HAS_COMPRESSION is_yes
then
  save_stdin()
  {
    func_check save_stdin '=' 0 "$@";
    ss_f="${_TMP_DIR}"/INPUT;
    cat >"${ss_f}";
    cat_z "${ss_f}" >"${_TMP_STDIN}";
    rm_file "${ss_f}";
    eval ${_UNSET} ss_f;
    eval "${return_ok}";
  }
else
  save_stdin()
  {
    func_check save_stdin = 0 "$@";
    cat >"${_TMP_STDIN}";
    eval "${return_ok}";
  }
fi;


########################################################################
# special_filespec ()
#
# Handle special modes like whatis and apropos.
#
special_filespec()
{
  func_check special_setup '=' 0 "$@";
  if obj _OPT_APROPOS is_yes
  then
    if obj _OPT_WHATIS is_yes
    then
      error \
        'special_setup: $_OPT_APROPOS and $_OPT_WHATIS are both "yes"';
    fi;
    apropos_filespec;
    eval "${return_ok}";
  fi;
  if obj _OPT_WHATIS is_yes
  then
    whatis_filespec;
  fi;
  eval "${return_ok}";
}


########################################################################
# special_setup ()
#
# Handle special modes like whatis and apropos.
#
special_setup()
{
  func_check special_setup '=' 0 "$@";
  if obj _OPT_APROPOS is_yes
  then
    if obj _OPT_WHATIS is_yes
    then
      error \
        'special_setup: $_OPT_APROPOS and $_OPT_WHATIS are both "yes"';
    fi;
    apropos_setup;
    eval "${return_ok}";
  fi;
  if obj _OPT_WHATIS is_yes
  then
    whatis_header;
  fi;  
  eval "${return_ok}";
}


########################################################################
landmark '11: stack_*()';
########################################################################

########################################################################
# string_contains (<string> <part>)
#
# Test whether `part' is contained in `string'.
#
# Arguments : 2 text arguments.
# Return    : `0' if arg2 is substring of arg1, `1' otherwise.
#
string_contains()
{
  func_check string_contains '=' 2 "$@";
  case "$1" in
    *"$2"*)
      eval "${return_yes}";
      ;;
    *)
      eval "${return_no}";
      ;;
  esac;
  eval "${return_ok}";
}


########################################################################
# string_not_contains (<string> <part>)
#
# Test whether `part' is not substring of `string'.
#
# Arguments : 2 text arguments.
# Return    : `0' if arg2 is substring of arg1, `1' otherwise.
#
string_not_contains()
{
  func_check string_not_contains '=' 2 "$@";
  if string_contains "$1" "$2"
  then
    eval "${return_no}";
  else
    eval "${return_yes}";
  fi;
  eval "${return_ok}";
}


########################################################################
landmark '12: tmp_*()';
########################################################################

########################################################################
# tmp_cat ()
#
# output the temporary cat file (the concatenation of all input)
#
tmp_cat()
{
  func_check tmp_cat '=' 0 "$@";
  cat "${_TMP_CAT}";
  eval "${return_var}" "$?";
}


########################################################################
# tmp_create (<suffix>?)
#
# Create temporary file.
#
# It's safe to use the shell process ID together with a suffix to
# have multiple temporary files.
#
# Globals: $_TMP_DIR
#
# Output : name of created file
#
# Variable prefix: tc
#
tmp_create()
{
  func_check tmp_create '<=' 1 "$@";
  # the output file does not have `,' as first character, so these are
  # different names from the output file.
  tc_tmp="${_TMP_DIR}/,$1";
  : >"${tc_tmp}"
  obj tc_tmp echo1;
  eval ${_UNSET} tc_tmp;
  eval "${return_ok}";
}


########################################################################
# to_tmp (<filename>)
#
# print file (decompressed) to the temporary cat file
#
to_tmp()
{
  func_check to_tmp '=' 1 "$@";
  if obj _TMP_CAT is_empty
  then
    error 'to_tmp_line: $_TMP_CAT is not yet set';
  fi;
  if is_file "$1"
  then
    if obj _OPT_LOCATION is_yes
    then
      echo2 "$1";
    fi;
    if obj _OPT_WHATIS is_yes
    then
      whatis_filename "$1" >>"${_TMP_CAT}";
    else
      cat_z "$1" >>"${_TMP_CAT}";
    fi;
  else
    error "to_tmp(): could not read file \`$1'.";
  fi;
  eval "${return_ok}";
}


########################################################################
# to_tmp_line ([<text>])
#
# print line to the temporary cat file
#
to_tmp_line()
{
  func_check to_tmp '>=' 0 "$@";
  if obj _TMP_CAT is_empty
  then
    error 'to_tmp_line: $_TMP_CAT is not yet set';
  fi;
  echo1 "$*" >>"${_TMP_CAT}";
  eval "${return_ok}";
}


########################################################################
# trap_set
#
# call function on signal 0
#
trap_set()
{
  func_check trap_set '=' 0 "$@";
  trap 'clean_up' 0 2>${_NULL_DEV} || :;
  eval "${return_ok}";
}


########################################################################
# trap_unset ()
#
# disable trap on signal 0.
#
trap_unset()
{
  func_check trap_unset '=' 0 "$@";
  trap '' 0 2>${_NULL_DEV} || :;
  eval "${return_ok}";
}


########################################################################
# usage ()
#
# print usage information to stderr; for groffer option --help.
#
usage()
{
  func_check usage = 0 "$@";
  echo;
  version;
  echo1 'Usage: groffer [option]... [filespec]...';
  cat <<EOF

Display roff files, standard input, and/or Unix manual pages with a X
Window viewer or in several text modes.  All input is decompressed
on-the-fly with all formats that gzip can handle.

"filespec" is one of
  "filename"       name of a readable file
  "-"              for standard input
  "man:name.n"     man page "name" in section "n"
  "man:name"       man page "name" in first section found
  "name.n"         man page "name" in section "n"
  "name"           man page "name" in first section found
and some more (see groffer(1) for details).

-h --help         print this usage message.
-Q --source       output as roff source.
-T --device=name  pass to groff using output device "name".
-v --version      print version information.
-V                display the groff execution pipe instead of formatting.
-X                display with "gxditview" using groff -X.
-Z --ditroff --intermediate-output
                  generate groff intermediate output without
                  post-processing and viewing, like groff -Z.
All other short options are interpreted as "groff" formatting options.

The most important groffer long options are

--apropos=name    start man's "apropos" program for "name".
--apropos-data=name
                  "apropos" for "name" in man's data sections 4, 5, 7.
--apropos-devel=name
                  "apropos" for "name" in development sections 2, 3, 9.
--apropos-progs=name
                  "apropos" for "name" in man's program sections 1, 6, 8.
--auto            choose mode automatically from the default mode list.
--default         reset all options to the default value.
--default-modes=mode1,mode2,...
                  set sequence of automatically tried modes.
--dvi             display in a viewer for TeX device independent format.
--dvi-viewer=prog choose the viewer program for dvi mode.
--groff           process like groff, disable viewing features.
--help            display this helping output.
--html            display in a web browser.
--html-viewer=program
                  choose the web browser for html mode.
--man             check file parameters first whether they are man pages.
--mode=auto|dvi|groff|html|pdf|ps|source|text|tty|www|x|X
                  choose display mode.
--no-man          disable man-page facility.
--no-special      disable --all, --apropos*, and --whatis
--pager=program   preset the paging program for tty mode.
--pdf             display in a PDF viewer.
--pdf-viewer=prog choose the viewer program for pdf mode.
--ps              display in a Postscript viewer.
--ps-viewer=prog  choose the viewer program for ps mode.
--shell=program   specify a shell under which to run groffer2.sh.
--text            output in a text device without a pager.
--tty             display with a pager on text terminal even when in X.
--tty-viewer=prog select a pager for tty mode; same as --pager.
--whatis          display the file name and description of man pages
--www             same as --html.
--www-viewer=prog same as --html-viewer
--x --X           display with "gxditview" using an X* device.
--x-viewer=prog   choose viewer program for x mode (X mode).
--X-viewer=prog   same as "--xviewer".

Development options that are not useful for normal usage:
--debug, --debug-all, --debug-keep, --debug-lm, --debug-params,
--debug-shell, --debug-stacks, --debug-tmpdir, --debug-user,
--do-nothing, --print=text

Viewer programs for the different modes that run on the terminal:
--dvi-viewer-tty=prog, --html-viewer-tty=prog, --pdf-viewer-tty=prog,
--ps-viewer-tty=prog, --tty-viewer-tty, --X-viewer-tty=prog,
--x-viewer-tty=prog, --www-viewer-tty=prog

The usual X Windows toolkit options transformed into GNU long options:
--background=color, --bd=size, --bg=color, --bordercolor=color,
--borderwidth=size, --bw=size, --display=Xdisplay, --fg=color,
--fn=font, --font=font, --foreground=color, --geometry=geom, --iconic,
--resolution=dpi, --rv, --title=text, --xrm=resource

Long options of GNU "man":
--all, --ascii, --ditroff, --extension=suffix, --locale=language,
--local-file=name, --location, --manpath=dir1:dir2:...,
--sections=s1:s2:..., --systems=s1,s2,..., --where, ...

EOF
  eval "${return_ok}";
}


########################################################################
# version ()
#
# print version information to stderr
#
version()
{
  func_check version = 0 "$@";
  echo1 "groffer ${_PROGRAM_VERSION} of ${_LAST_UPDATE}";
  # also display groff's version, but not the called subprograms
  groff -v 2>&1 | sed -e '/^ *$/q' | sed -e '1s/^/is part of /';
  eval "${return_ok}";
}


########################################################################
# warning (<string>)
#
# Print warning to stderr
#
warning()
{
  echo2 "warning: $*";
}


########################################################################
# whatis_filename (<filename>)
#
# Interpret <filename> as a man page and display its `whatis'
# information as a fragment written in the groff language.
#
# Variable prefix: wf
#
whatis_filename()
{
  func_check whatis_filename = 1 "$@";
  wf_arg="$1";
  if obj wf_arg is_not_file
  then
    error "whatis_filename(): argument is not a readable file."
  fi;
  wf_dot='^\.'"${_SPACE_SED}"'*';
  if obj _FILESPEC_ARG is_equal '-'
  then
    wf_arg='stdin';
  fi;
  cat <<EOF
\f[CR]${wf_arg}\f[]:
.br
EOF

  # get the parts of the file name
  wf_name="$(base_name $1)";
  wf_section="$(echo1 $1 | sed -n -e '
s|^.*/man\('"${_MAN_AUTO_SEC_CHARS}"'\).*$|\1|p
')";
  if obj wf_section is_not_empty
  then
    case "${wf_name}" in
    *.${wf_section}*)
      s='yes';
      ;;
    *)
      s='';
      wf_section='';
      ;;
    esac
    if obj s is_yes
    then
      wf_name="$(echo1 ${wf_name} | sed -e '
s/^\(.*\)\.'${wf_section}'.*$/\1/
')";
    fi;
  fi;

  # traditional man style; grep the line containing `.TH' macro, if any
  wf_res="$(cat_z "$1" | sed -e '
/'"${wf_dot}"'TH /p
d
')";
  exit_test;
  if obj wf_res is_not_empty
  then				# traditional man style
    # get the first line after the first `.SH' macro, by
    # - delete up to first .SH;
    # - print all lines before the next .SH;
    # - quit.
    wf_res="$(cat_z "$1" | sed -n -e '
1,/'"${wf_dot}"'SH/d
/'"${wf_dot}"'SH/q
p
')";

    if obj wf_section is_not_empty
    then
      case "${wf_res}" in
      ${wf_name}${_SPACE_CASE}*-${_SPACE_CASE}*)
        s='yes';
        ;;
      *)
        s='';
        ;;
      esac;
      if obj s is_yes
      then
        wf_res="$(obj wf_res echo1 | sed -e '
s/^'"${wf_name}${_SPACE_SED}"'[^-]*-'"${_SPACE_SED}"'*\(.*\)$/'"${wf_name}"' ('"${wf_section}"') \\[em] \1/
')";
      fi;
    fi;
    obj wf_res echo1;
    echo;
    eval ${_UNSET} wf_arg;
    eval ${_UNSET} wf_dot;
    eval ${_UNSET} wf_name;
    eval ${_UNSET} wf_res;
    eval ${_UNSET} wf_section;
    eval "${return_ok}";
  fi;

  # mdoc style (BSD doc); grep the line containing `.Nd' macro, if any
  wf_res="$(cat_z "$1" | sed -n -e '/'"${wf_dot}"'Nd /s///p')";
  exit_test;
  if obj wf_res is_not_empty
  then				# BSD doc style
    if obj wf_section is_not_empty
    then
      wf_res="$(obj wf_res echo1 | sed -n -e '
s/^\(.*\)$/'"${wf_name}"' ('"${wf_section}"') \\[em] \1/p
')";
    fi;
    obj wf_res echo1;
    echo;
    eval ${_UNSET} wf_arg;
    eval ${_UNSET} wf_dot;
    eval ${_UNSET} wf_name;
    eval ${_UNSET} wf_res;
    eval ${_UNSET} wf_section;
    eval "${return_ok}";
  fi;
  echo1 'is not a man page';
  echo;
  eval ${_UNSET} wf_arg;
  eval ${_UNSET} wf_dot;
  eval ${_UNSET} wf_name;
  eval ${_UNSET} wf_res;
  eval ${_UNSET} wf_section;
  eval "${return_bad}";
}


########################################################################
# whatis_filespec ()
#
# Print the filespec name as .SH to the temporary cat file.
#
whatis_filespec()
{
  func_check whatis_filespec '=' 0 "$@";
  if obj _OPT_WHATIS is_yes
  then
    eval to_tmp_line \
      "'.SH $(echo1 "${_FILESPEC_ARG}" | sed 's/[^\\]-/\\-/g')'";
    exit_test;
  fi;
  eval "${return_ok}";
}


########################################################################
# whatis_header ()
#
# Print the whatis header to the temporary cat file.
#
whatis_header()
{
  func_check whatis_header '=' 0 "$@";
  if obj _OPT_WHATIS is_yes
  then
     to_tmp_line '.TH GROFFER WHATIS';
  fi;
  eval "${return_ok}";
}


########################################################################
# where_is (<program>)
#
# Output path of a program if in $PATH.
#
# Arguments : >=1 (empty allowed)
#   more args are ignored, this allows to specify progs with arguments
# Return    : `0' if arg1 is a program in $PATH, `1' otherwise.
#
# Variable prefix: w
#
where_is()
{
  func_check where_is '>=' 1 "$@";
  w_arg="$1";
  if obj w_arg is_empty
  then
    eval ${_UNSET} w_arg;
    eval "${return_bad}";
  fi;
  case "${w_arg}" in
    /*)
      eval ${_UNSET} w_arg;
      eval ${_UNSET} w_file;
      if test -f "${w_arg}" && test -x "${w_arg}"
      then
        eval "${return_ok}";
      else
        eval "${return_bad}";
      fi;
      ;;
  esac;
  eval set x "$(path_split "${PATH}")";
  exit_test;
  shift;
  for p
  do
    case "$p" in
      */) w_file=${p}${w_arg}; ;;
      *)  w_file=${p}/${w_arg}; ;;
    esac;
    if test -f "${w_file}" && test -x "${w_file}"
    then
      obj w_file echo1;
      eval ${_UNSET} w_arg;
      eval ${_UNSET} w_file;
      eval "${return_ok}";
    fi;
  done;
  eval ${_UNSET} w_arg;
  eval ${_UNSET} w_file;
  eval "${return_bad}";
}


########################################################################
#                        main* Functions
########################################################################

# The main area contains the following parts:
# - main_init(): initialize temporary files and set exit trap
# - main_parse_MANOPT(): parse $MANOPT
# - main_parse_args(): argument parsing
# - main_set_mode (): determine the display mode
# - main_do_fileargs(): process filespec arguments
# - main_set_resources(): setup X resources
# - main_display(): do the displaying
# - main(): the main function that calls all main_*()


#######################################################################
# main_init ()
#
# set exit trap and create temporary files
#
# Globals: $_TMP_DIR, $_TMP_CAT, $_TMP_STDIN
#
# Variable prefix: mi
#
main_init()
{
  func_check main_init = 0 "$@";
  # call clean_up() on shell termination.
  trap_set;

  # create temporary directory
  umask 0022;
  _TMP_DIR='';
  for d in "${GROFF_TMPDIR}" "${TMPDIR}" "${TMP}" "${TEMP}" \
           "${TEMPDIR}" "${HOME}"'/tmp' '/tmp' "${HOME}" '.'
  do
    mi_dir="$d";
    if obj mi_dir is_empty || obj mi_dir is_not_dir || \
       obj mi_dir is_not_writable
    then
      continue;
    fi;

    case "${mi_dir}" in
    */)
      _TMP_DIR="${mi_dir}";
      ;;
    *)
      _TMP_DIR="${mi_dir}"'/';
      ;;
    esac;
    _TMP_DIR="${_TMP_DIR}groffer${_PROCESS_ID}";
    if obj _TMP_DIR rm_tree
    then
      :
    else
      mi_tdir_="${_TMP_DIR}"_;
      mi_n=1;
      mi_tdir_n="${mi_tdir_}${mi_n}";
      while obj mi_tdir_n is_existing
      do
        if obj mi_tdir_n rm_tree
        then
          # directory could not be removed
          mi_n="$(expr "${mi_n}" + 1)";
          mi_tdir_n="${mi_tdir_}${mi_n}";
          continue;
        fi;
      done;
      _TMP_DIR="${mi_tdir_n}";
    fi;
    eval mkdir "${_TMP_DIR}";
    if is_not_equal "$?" 0
    then
      obj _TMP_DIR rm_tree;
      _TMP_DIR='';
      continue;
    fi;
    if obj _TMP_DIR is_dir && obj _TMP_DIR is_writable
    then
      # $_TMP_DIR can now be used as temporary directory
      break;
    fi;
    obj _TMP_DIR rm_tree;
    _TMP_DIR='';
    continue;
  done;
  if obj _TMP_DIR is_empty
  then
    error "main_init: \
Couldn't create a directory for storing temporary files.";
  fi;
  if obj _DEBUG_PRINT_TMPDIR is_yes
  then
    echo2 "temporary directory: ${_TMP_DIR}";
  fi;

  _TMP_CAT="$(tmp_create groffer_cat)";
  _TMP_STDIN="$(tmp_create groffer_input)";
  exit_test;

  eval ${_UNSET} mi_dir;
  eval ${_UNSET} mi_n;
  eval ${_UNSET} mi_tdir_;
  eval ${_UNSET} mi_tdir_n;
  eval "${return_ok}";
} # main_init()


########################################################################
# main_parse_MANOPT ()
#
# Parse $MANOPT to retrieve man options, but only if it is a non-empty
# string; found man arguments can be overwritten by the command line.
#
# Globals:
#   in: $MANOPT, $_OPTS_MANOPT_*
#   out: $_MANOPT_*
#
# Variable prefix: mpm
#
main_parse_MANOPT()
{
  func_check main_parse_MANOPT = 0 "$@";

  if obj MANOPT is_not_empty
  then
    # Delete leading and final spaces
    MANOPT="$(echo1 "${MANOPT}" | sed -e '
s/^'"${_SPACE_SED}"'*//
s/'"${_SPACE_SED}"'*$//
')";
  exit_test;
  fi;
  if obj MANOPT is_empty
  then
    eval "${return_ok}";
  fi;

  mpm_list='';
  # add arguments in $MANOPT by mapping them to groffer options
  eval set x "$(list_from_cmdline _OPTS_MANOPT "${MANOPT}")";
  exit_test;
  shift;
  until test "$#" -le 0 || is_equal "$1" '--'
  do
    mpm_opt="$1";
    shift;
    case "${mpm_opt}" in
    -7|--ascii)
      list_append mpm_list '--ascii';
      ;;
    -a|--all)
      list_append mpm_list '--all';
      ;;
    -c|--catman)
      do_nothing;
      shift;
      ;;
    -d|--debug)
      do_nothing;
      ;;
    -D|--default)
      # undo all man options so far
      mpm_list='';
      ;;
    -e|--extension)
      list_append mpm_list '--extension';
      shift;
      ;;
    -f|--whatis)
      list_append mpm_list '--whatis';
      shift;
      ;;
    -h|--help)
      do_nothing;
      shift;
      ;;
    -k|--apropos)
      # groffer's --apropos takes an argument, but man's does not, so
      do_nothing;
      ;;
    -l|--local-file)
      do_nothing;
      ;;
    -L|--locale)
      list_append mpm_list '--locale' "$1";
      shift;
      ;;
    -m|--systems)
      list_append mpm_list '--systems' "$1";
      shift;
      ;;
    -M|--manpath)
      list_append mpm_list '--manpath' "$1";
      shift;
      ;;
    -p|--preprocessor)
      do_nothing;
      shift;
      ;;
    -P|--pager)
      list_append mpm_list '--pager' "$1";
      shift;
      ;;
    -r|--prompt)
      do_nothing;
      shift;
      ;;
    -S|--sections)
      list_append mpm_list '--sections' "$1";
      shift;
      ;;
    -t|--troff)
      do_nothing;
      shift;
      ;;
    -T|--device)
      list_append mpm_list '-T' "$1";
      shift;
      ;;
    -u|--update)
      do_nothing;
      shift;
      ;;
    -V|--version)
      do_nothing;
      ;;
    -w|--where|--location)
      list_append mpm_list '--location';
      ;;
    -Z|--ditroff)
      do_nothing;
      ;;
    # ignore all other options
    esac;
  done;

  # prepend $mpm_list to the command line
  if obj mpm_list is_not_empty
  then
    eval set x "${mpm_list}" '"$@"';
    shift;
  fi;

  eval ${_UNSET} mpm_list;
  eval ${_UNSET} mpm_opt;
  eval "${return_ok}";
} # main_parse_MANOPT()


########################################################################
# main_parse_args (<command_line_args>*)
#
# Parse arguments; process options and filespec parameters
#
# Arguments: pass the command line arguments unaltered.
# Globals:
#   in:  $_OPTS_*
#   out: $_OPT_*, $_ADDOPTS, $_FILEARGS
#
# Variable prefix: mpa
#
main_parse_args()
{
  func_check main_parse_args '>=' 0 "$@";
  _ALL_PARAMS="$(list_from_cmdline _OPTS_CMDLINE "$@")";
  exit_test;
  if obj _DEBUG_PRINT_PARAMS is_yes
  then
    echo2 "parameters: ${_ALL_PARAMS}";
  fi;
  eval set x "${_ALL_PARAMS}";
  shift;

  # By the call of `eval', unnecessary quoting was removed.  So the
  # positional shell parameters ($1, $2, ...) are now guaranteed to
  # represent an option or an argument to the previous option, if any;
  # then a `--' argument for separating options and
  # parameters; followed by the filespec parameters if any.

  # Note, the existence of arguments to options has already been checked.
  # So a check for `$#' or `--' should not be done for arguments.

  until test "$#" -le 0 || is_equal "$1" '--'
  do
    mpa_opt="$1";		# $mpa_opt is fed into the option handler
    shift;
    case "${mpa_opt}" in
    -h|--help)
      usage;
      leave;
      ;;
    -Q|--source)		# output source code (`Quellcode').
      _OPT_MODE='source';
      ;;
    -T|--device|--troff-device) # device; arg
      _OPT_DEVICE="$1";
      _check_device_with_mode;
      shift;
      ;;
    -v|--version)
      version;
      leave;
      ;;
    -V)
      _OPT_V='yes';
      ;;
    -Z|--ditroff|--intermediate-output) # groff intermediate output
      _OPT_Z='yes';
      ;;
    -X)
      if is_X
      then
        _OPT_MODE=X;
      fi;
      ;;
    -?)
      # delete leading `-'
      mpa_optchar="$(echo1 "${mpa_opt}" | sed -e 's/^-//')";
      exit_test;
      if list_has _OPTS_GROFF_SHORT_NA "${mpa_optchar}"
      then
        list_append _ADDOPTS_GROFF "${mpa_opt}";
      elif list_has _OPTS_GROFF_SHORT_ARG "${mpa_optchar}"
      then
        list_append _ADDOPTS_GROFF "${mpa_opt}" "$1";
        shift;
      else
        error "main_parse_args(): Unknown option : \`$1'";
      fi;
      ;;
    --all)
        _OPT_ALL='yes';
        ;;
    --apropos)			# run `apropos'
      _OPT_APROPOS='yes';
      _APROPOS_SECTIONS='';
      _OPT_WHATIS='no';
      ;;
    --apropos-data)		# run `apropos' for data sections
      _OPT_APROPOS='yes';
      _APROPOS_SECTIONS='457';
      _OPT_WHATIS='no';
      ;;
    --apropos-devel)		# run `apropos' for development sections
      _OPT_APROPOS='yes';
      _APROPOS_SECTIONS='239';
      _OPT_WHATIS='no';
      ;;
    --apropos-progs)		# run `apropos' for program sections
      _OPT_APROPOS='yes';
      _APROPOS_SECTIONS='168';
      _OPT_WHATIS='no';
      ;;
    --ascii)
      list_append _ADDOPTS_GROFF '-mtty-char';
      if obj _OPT_MODE is_empty
      then
        _OPT_MODE='text';
      fi;
      ;;
    --auto)			# the default automatic mode
      _OPT_MODE='';
      ;;
    --bd)			# border color for viewers, arg;
      _OPT_BD="$1";
      shift;
      ;;
    --bg|--backgroud)		# background color for viewers, arg;
      _OPT_BG="$1";
      shift;
      ;;
    --bw)			# border width for viewers, arg;
      _OPT_BW="$1";
      shift;
      ;;
    --debug|--debug-all|--debug-keep|--debug-lm|--debug-params|\
--debug-shell|--debug-stacks|--debug-tmpdir|--debug-user)
      # debug is handled at the beginning
      :;
      ;;
    --default)			# reset variables to default
      reset;
      ;;
    --default-modes)		# sequence of modes in auto mode; arg
      _OPT_DEFAULT_MODES="$1";
      shift;
      ;;
    --display)			# set X display, arg
      _OPT_DISPLAY="$1";
      shift;
      ;;
    --do-nothing)
      _OPT_DO_NOTHING='yes';
      ;;
    --dvi)
      if is_X
      then
        _OPT_MODE='dvi';
      fi;
      ;;
    --dvi-viewer)		# viewer program for dvi mode; arg
      _VIEWER_TERMINAL='no';
      _OPT_VIEWER_DVI="$1";
      shift;
      ;;
    --dvi-viewer-tty)		# viewer program for dvi mode in tty; arg
      _VIEWER_TERMINAL='yes';
      _OPT_VIEWER_DVI="$1";
      shift;
      ;;
    --extension)		# the extension for man pages, arg
      _OPT_EXTENSION="$1";
      shift;
      ;;
    --fg|--foreground)		# foreground color for viewers, arg;
      _OPT_FG="$1";
      shift;
      ;;
    --fn|--font)		# set font for viewers, arg;
      _OPT_FN="$1";
      shift;
      ;;
    --geometry)			# window geometry for viewers, arg;
      _OPT_GEOMETRY="$1";
      shift;
      ;;
    --groff)
      _OPT_MODE='groff';
      ;;
    --html|--www)		# display with web browser
      _OPT_MODE=html;
      ;;
    --html-viewer|--www-viewer) # viewer program for html mode; arg
      _VIEWER_TERMINAL='no';
      _OPT_VIEWER_HTML="$1";
      shift;
      ;;
    --html-viewer-tty|--www-viewer-tty) # viewer for html mode in tty; arg
      _VIEWER_TERMINAL='yes';
      _OPT_VIEWER_HTML="$1";
      shift;
      ;;
    --iconic)			# start viewers as icons
      _OPT_ICONIC='yes';
      ;;
    --locale)			# set language for man pages, arg
      # argument is xx[_territory[.codeset[@modifier]]] (ISO 639,...)
      _OPT_LANG="$1";
      shift;
      ;;
    --local-file)		# force local files; same as `--no-man'
      _MAN_FORCE='no';
      _MAN_ENABLE='no';
      ;;
    --location|--where)		# print file locations to stderr
      _OPT_LOCATION='yes';
      ;;
    --man)		       # force all file params to be man pages
      _MAN_ENABLE='yes';
      _MAN_FORCE='yes';
      ;;
    --manpath)		      # specify search path for man pages, arg
      # arg is colon-separated list of directories
      _OPT_MANPATH="$1";
      shift;
      ;;
    --mode)			# display mode
      mpa_arg="$1";
      shift;
      case "${mpa_arg}" in
      auto|'')		     # search mode automatically among default
        _OPT_MODE='';
        ;;
      groff)			# pass input to plain groff
        _OPT_MODE='groff';
        ;;
      html|www)			# display with a web browser
        _OPT_MODE='html';
        ;;
      dvi)			# display with xdvi viewer
        if is_X
        then
          _OPT_MODE='dvi';
        fi;
        ;;
      pdf)			# display with PDF viewer
        if is_X
        then
          _OPT_MODE='pdf';
        fi;
        ;;
      ps)			# display with Postscript viewer
        if is_X
        then
          _OPT_MODE='ps';
        fi;
        ;;
      text)			# output on terminal
        _OPT_MODE='text';
        ;;
      tty)			# output on terminal
        _OPT_MODE='tty';
        ;;
      X|x)			# output on X roff viewer
        if is_X
        then
          _OPT_MODE='x';
        fi;
        ;;
      Q|source)			# display source code
        _OPT_MODE="source";
        ;;
      *)
        error "main_parse_args(): unknown mode ${mpa_arg}";
        ;;
      esac;
      ;;
    --no-location)		# disable former call to `--location'
      _OPT_LOCATION='yes';
      ;;
    --no-man)			# disable search for man pages
      # the same as --local-file
      _MAN_FORCE='no';
      _MAN_ENABLE='no';
      ;;
    --no-special)		# disable some special former calls
      _OPT_ALL='no'
      _OPT_APROPOS='no'
      _OPT_WHATIS='no'
      ;;
    --pager|--tty-viewer|--tty-viewer-tty)
      # set paging program for tty mode, arg
      _VIEWER_TERMINAL='yes';
      _OPT_PAGER="$1";
      shift;
      ;;
    --pdf)
      if is_X
      then
        _OPT_MODE='pdf';
      fi;
      ;;
    --pdf-viewer)		# viewer program for ps mode; arg
      _VIEWER_TERMINAL='no';
      _OPT_VIEWER_PDF="$1";
      shift;
      ;;
    --pdf-viewer-tty)		# viewer program for ps mode in tty; arg
      _VIEWER_TERMINAL='yes';
      _OPT_VIEWER_PDF="$1";
      shift;
      ;;
    --print)			# for argument test
      echo2 "$1";
      shift;
      ;;
    --ps)
      if is_X
      then
        _OPT_MODE='ps';
      fi;
      ;;
    --ps-viewer)		# viewer program for ps mode; arg
      _VIEWER_TERMINAL='no';
      _OPT_VIEWER_PS="$1";
      shift;
      ;;
    --ps-viewer-tty)		# viewer program for ps mode in tty; arg
      _VIEWER_TERMINAL='yes';
      _OPT_VIEWER_PS="$1";
      shift;
      ;;
    --resolution)		# set resolution for X devices, arg
      mpa_arg="$1";
      shift;
      case "${mpa_arg}" in
      75|75dpi)
        mpa_dpi=75;
        ;;
      100|100dpi)
        mpa_dpi=100;
        ;;
      *)
        error "main_parse_args(): \
only resoutions of 75 or 100 dpi are supported";
        ;;
      esac;
      _OPT_RESOLUTION="${mpa_dpi}";
      ;;
    --rv)
      _OPT_RV='yes';
      ;;
    --sections)			# specify sections for man pages, arg
      # arg is colon-separated list of section names
      _OPT_SECTIONS="$1";
      shift;
      ;;
    --shell)
      # already done during the first run; so ignore the argument
      shift;
      ;;
    --systems)			# man pages for different OS's, arg
      # argument is a comma-separated list
      _OPT_SYSTEMS="$1";
      shift;
      ;;
    --text)			# text mode without pager
      _OPT_MODE=text;
      ;;
    --title)			# title for X viewers; arg
      _OPT_TITLE="$1";
      shift;
      ;;
    --tty)			# tty mode, text with pager
      _OPT_MODE=tty;
      ;;
    --text-device|--tty-device) # device for tty mode; arg
      _OPT_TEXT_DEVICE="$1";
      shift;
      ;;
    --whatis)
      _OPT_WHATIS='yes';
      _OPT_ALL='yes';
      _OPT_APROPOS='no';
      ;;
    --X|--x)
      if is_X
      then
        _OPT_MODE=x;
      fi;
      ;;
    --xrm)			# pass X resource string, arg;
      list_append _OPT_XRM "$1";
      shift;
      ;;
    --x-viewer|--X-viewer)	# viewer program for x mode; arg
      _VIEWER_TERMINAL='no';
      _OPT_VIEWER_X="$1";
      shift;
      ;;
    --x-viewer-tty|--X-viewer-tty) # viewer program for x mode in tty; arg
      _VIEWER_TERMINAL='yes';
      _OPT_VIEWER_X="$1";
      shift;
      ;;
    *)
      error 'main_parse_args(): error on argument parsing : '"\`$*'";
      ;;
    esac;
  done;
  shift;			# remove `--' argument

  if obj _OPT_DO_NOTHING is_yes
  then
    leave;
  fi;

  # Remaining arguments are file names (filespecs).
  # Save them to list $_FILEARGS
  if is_equal "$#" 0
  then				# use "-" for standard input
    set x '-';
    shift;
  fi;
  _FILEARGS='';
  list_append _FILEARGS "$@";
  if list_has _FILEARGS '-'
  then
    save_stdin;
  fi;
  # $_FILEARGS must be retrieved with `eval set x "$_FILEARGS"; shift;'
  eval ${_UNSET} mpa_arg;
  eval ${_UNSET} mpa_dpi;
  eval ${_UNSET} mpa_opt;
  eval ${_UNSET} mpa_optchar;
  eval "${return_ok}";
} # main_parse_args()


# Called from main_parse_args() because double `case' is not possible.
# Globals: $_OPT_DEVICE, $_OPT_MODE
_check_device_with_mode()
{
  func_check _check_device_with_mode = 0 "$@";
  case "${_OPT_DEVICE}" in
  dvi)
    _OPT_MODE=dvi;
    eval "${return_ok}";
    ;;
  html)
    _OPT_MODE=html;
    eval "${return_ok}";
    ;;
  lbp|lj4)
    _OPT_MODE=groff;
    eval "${return_ok}";
    ;;
  ps)
    _OPT_MODE=ps;
    eval "${return_ok}";
    ;;
  ascii|cp1047|latin1|utf8)
    if obj _OPT_MODE is_not_equal text
    then
      _OPT_MODE=tty;		# default text mode
    fi;
    eval "${return_ok}";
    ;;
  X*)
    _OPT_MODE=x;
    eval "${return_ok}";
    ;;
  *)				# unknown device, go to groff mode
    _OPT_MODE=groff;
    eval "${return_ok}";
    ;;
  esac;
  eval "${return_error}";
} # _check_device_with_mode() of main_parse_args()


########################################################################
# main_set_mode ()
#
# Determine the display mode.
#
# Globals:
#   in:  $DISPLAY, $_OPT_MODE, $_OPT_DEVICE
#   out: $_DISPLAY_MODE
#
# Variable prefix: msm
#
main_set_mode()
{
  func_check main_set_mode = 0 "$@";

  # set display
  if obj _OPT_DISPLAY is_not_empty
  then
    DISPLAY="${_OPT_DISPLAY}";
  fi;

  if obj _OPT_V is_yes
  then
    list_append _ADDOPTS_GROFF '-V';
  fi;
  if obj _OPT_Z is_yes
  then
    _DISPLAY_MODE='groff';
    list_append _ADDOPTS_GROFF '-Z';
  fi;
  if obj _OPT_MODE is_equal 'groff'
  then
    _DISPLAY_MODE='groff';
  fi;
  if obj _DISPLAY_MODE is_equal 'groff'
  then
    eval ${_UNSET} msm_modes;
    eval ${_UNSET} msm_viewer;
    eval ${_UNSET} msm_viewers;
    eval "${return_ok}";
  fi;

  if obj _OPT_MODE is_equal 'source'
  then
    _DISPLAY_MODE='source';
    eval ${_UNSET} msm_modes;
    eval ${_UNSET} msm_viewer;
    eval ${_UNSET} msm_viewers;
    eval "${return_ok}";
  fi;

  case "${_OPT_MODE}" in
  '')				# automatic mode
    case "${_OPT_DEVICE}" in
    X*)
     if is_not_X
      then
        error_user "no X display found for device ${_OPT_DEVICE}";
      fi;
      _DISPLAY_MODE='x';
      eval ${_UNSET} msm_modes;
      eval ${_UNSET} msm_viewer;
      eval ${_UNSET} msm_viewers;
      eval "${return_ok}";
      ;;
    ascii|cp1047|latin1|utf8)
      if obj _DISPLAY_MODE is_not_equal 'text'
      then
        _DISPLAY_MODE='tty';
      fi;
      eval ${_UNSET} msm_modes;
      eval ${_UNSET} msm_viewer;
      eval ${_UNSET} msm_viewers;
      eval "${return_ok}";
      ;;
    esac;
    if is_not_X
    then
      _DISPLAY_MODE='tty';
      eval ${_UNSET} msm_modes;
      eval ${_UNSET} msm_viewer;
      eval ${_UNSET} msm_viewers;
      eval "${return_ok}";
    fi;

    if obj _OPT_DEFAULT_MODES is_empty
    then
      msm_modes="${_DEFAULT_MODES}";
    else
      msm_modes="${_OPT_DEFAULT_MODES}";
    fi;
    ;;
  text)
    _DISPLAY_MODE='text';
    eval ${_UNSET} msm_modes;
    eval ${_UNSET} msm_viewer;
    eval ${_UNSET} msm_viewers;
    eval "${return_ok}";
    ;;
  tty)
    _DISPLAY_MODE='tty';
    eval ${_UNSET} msm_modes;
    eval ${_UNSET} msm_viewer;
    eval ${_UNSET} msm_viewers;
    eval "${return_ok}";
    ;;
  html)
    _DISPLAY_MODE='html';
    msm_modes="${_OPT_MODE}";
    ;;
  *)				# display mode was given
    if is_not_X
    then
      error_user "You must be in X Window for ${_OPT_MODE} mode.";
    fi;
    msm_modes="${_OPT_MODE}";
    ;;
  esac;

  # only viewer modes are left
  eval set x "$(list_from_split "${msm_modes}" ',')";
  exit_test;
  shift;
  while test "$#" -gt 0
  do
    m="$1";
    shift;
    case "$m" in
    dvi)
      if obj _OPT_VIEWER_DVI is_not_empty
      then
        msm_viewer="${_OPT_VIEWER_DVI}";
      else
        msm_viewer="$(_get_first_prog "$_VIEWER_DVI}")";
        exit_test;
      fi;
      if obj msm_viewer is_empty
      then
        error 'No viewer for dvi mode available.';
      fi;
      if is_not_equal "$?" 0
      then
        continue;
      fi;
      _DISPLAY_PROG="${msm_viewer}";
      _DISPLAY_MODE="dvi";
      eval ${_UNSET} msm_modes;
      eval ${_UNSET} msm_viewer;
      eval ${_UNSET} msm_viewers;
      eval "${return_ok}";
      ;;
    html)
      if obj _OPT_VIEWER_HTML is_not_empty
      then
        msm_viewer="${_OPT_VIEWER_HTML}";
      else
        if is_X
        then
          msm_viewers="${_VIEWER_HTML_X}";
        else
          msm_viewers="${_VIEWER_HTML_TTY}";
        fi;
        msm_viewer="$(_get_first_prog "${msm_viewers}")";
        exit_test;
      fi;
      if obj msm_viewer is_empty
      then
        error 'No viewer for html mode available.';
      fi;
      if is_not_equal "$?" 0
      then
        continue;
      fi;
      _DISPLAY_PROG="${msm_viewer}";
      _DISPLAY_MODE=html;
      eval ${_UNSET} msm_modes;
      eval ${_UNSET} msm_viewer;
      eval ${_UNSET} msm_viewers;
      eval "${return_ok}";
      ;;
    pdf)
      if obj _OPT_VIEWER_PDF is_not_empty
      then
        msm_viewer="${_OPT_VIEWER_PDF}";
      else
        msm_viewer="$(_get_first_prog "${_VIEWER_PDF}")";
        exit_test;
      fi;
      if obj msm_viewer is_empty
      then
        error 'No viewer for pdf mode available.';
      fi;
      if is_not_equal "$?" 0
      then
        continue;
      fi;
      _DISPLAY_PROG="${msm_viewer}";
      _DISPLAY_MODE="pdf";
      eval ${_UNSET} msm_modes;
      eval ${_UNSET} msm_viewer;
      eval ${_UNSET} msm_viewers;
      eval "${return_ok}";
      ;;
    ps)
      if obj _OPT_VIEWER_PS is_not_empty
      then
        msm_viewer="${_OPT_VIEWER_PS}";
      else
        msm_viewer="$(_get_first_prog "${_VIEWER_PS}")";
        exit_test;
      fi;
      if obj msm_viewer is_empty
      then
        error 'No viewer for ps mode available.';
      fi;
      if is_not_equal "$?" 0
      then
        continue;
      fi;
      _DISPLAY_PROG="${msm_viewer}";
      _DISPLAY_MODE="ps";
     eval ${_UNSET} msm_modes;
      eval ${_UNSET} msm_viewer;
      eval ${_UNSET} msm_viewers;
      eval "${return_ok}";
      ;;
    text)
      _DISPLAY_MODE='text';
      eval ${_UNSET} msm_modes;
      eval ${_UNSET} msm_viewer;
      eval ${_UNSET} msm_viewers;
      eval "${return_ok}";
      ;;
    tty)
      _DISPLAY_MODE='tty';
      eval ${_UNSET} msm_modes;
      eval ${_UNSET} msm_viewer;
      eval ${_UNSET} msm_viewers;
      eval "${return_ok}";
      ;;
    x)
      if obj _OPT_VIEWER_X is_not_empty
      then
        msm_viewer="${_OPT_VIEWER_X}";
      else
        msm_viewer="$(_get_first_prog "${_VIEWER_X}")";
        exit_test;
      fi;
      if obj msm_viewer is_empty
      then
        error 'No viewer for x mode available.';
      fi;
      if is_not_equal "$?" 0
      then
        continue;
      fi;
      _DISPLAY_PROG="${msm_viewer}";
      _DISPLAY_MODE='x';
      eval ${_UNSET} msm_modes;
      eval ${_UNSET} msm_viewer;
      eval ${_UNSET} msm_viewers;
      eval "${return_ok}";
      ;;
    X)
      _DISPLAY_MODE='X';
      eval ${_UNSET} msm_modes;
      eval ${_UNSET} msm_viewer;
      eval ${_UNSET} msm_viewers;
      eval "${return_ok}";
      ;;
    esac;
  done;
  eval ${_UNSET} msm_modes;
  eval ${_UNSET} msm_viewer;
  eval ${_UNSET} msm_viewers;
  error_user "No suitable display mode found.";
} # main_set_mode()


# _get_first_prog (<proglist>)
#
# Retrieve first argument that represents an existing program in $PATH.
# Local function for main_set_mode().
#
# Arguments: 1; a comma-separated list of commands (with options),
#               like $_VIEWER_*.
#
# Return  : `1' if none found, `0' if found.
# Output  : the argument that succeded.
#
# Variable prefix: _gfp
#
_get_first_prog()
{
  if is_equal "$#" 0
  then
    error "_get_first_prog() needs 1 argument.";
  fi;
  if is_empty "$1"
  then
    return "${_BAD}";
  fi;
  eval set x "$(list_from_split "$1" ',')";
  exit_test;
  shift;
  for i
  do
    _gfp_i="$i";
    if obj _gfp_i is_empty
    then
      continue;
    fi;
    if eval is_prog "$(get_first_essential ${_gfp_i})"
    then
      exit_test;
      obj _gfp_i echo1;
      eval ${_UNSET} _gfp_i;
      return "${_GOOD}";
    fi;
  done;
  eval ${_UNSET} _gfp_i;
  return "${_BAD}";
} # _get_first_prog() of main_set_mode()


#######################################################################
# main_do_fileargs ()
#
# Process filespec arguments in $_FILEARGS.
#
# Globals:
#   in: $_FILEARGS (process with `eval set x "$_FILEARGS"; shift;')
#
# Variable prefix: mdfa
#
main_do_fileargs()
{
  func_check main_do_fileargs = 0 "$@";
  special_setup;
  eval set x "${_FILEARGS}";
  shift;
  eval ${_UNSET} _FILEARGS;
  # temporary storage of all input to $_TMP_CAT
  while test "$#" -ge 2
  do
    # test for `s name' arguments, with `s' a 1-char standard section
    mdfa_filespec="$1";
    _FILESPEC_ARG="$1";
    shift;
    case "${mdfa_filespec}" in
    '')
      continue;
      ;;
    '-')
      special_filespec;
      if obj _OPT_APROPOS is_yes
      then
        continue;
      fi;
      register_file '-'
      continue;
      ;;
    ?)
      if obj _OPT_APROPOS is_yes
      then
        special_filespec;
        continue;
      fi;
      if list_has_not _MAN_AUTO_SEC_LIST "${mdfa_filespec}"
      then
        special_filespec;
        do_filearg "${mdfa_filespec}"
        continue;
      fi;
      mdfa_name="$1";
      _FILESPEC_ARG="${_FILESPEC_ARG} $1";
      special_filespec;
      case "${mdfa_name}" in
      */*|man:*|*\(*\)|*."${mdfa_filespec}")
        do_filearg "${mdfa_filespec}"
        continue;
        ;;
      esac;
      shift;
      if do_filearg "man:${mdfa_name}(${mdfa_filespec})"
      then
        continue;
      else
        do_filearg "${mdfa_filespec}"
        continue;
      fi;
      ;;
    *)
      special_filespec;
      if obj _OPT_APROPOS is_yes
      then
        continue;
      fi;
      do_filearg "${mdfa_filespec}"
      continue;
      ;;
    esac;
  done;				# end of `s name' test
  while test "$#" -gt 0
  do
    mdfa_filespec="$1";
    _FILESPEC_ARG="$1";
    shift;
    special_filespec;
    if obj _OPT_APROPOS is_yes
    then
      continue;
    fi;
    do_filearg "${mdfa_filespec}"
  done;
  obj _TMP_STDIN rm_file_with_debug;
  eval ${_UNSET} mdfa_filespec;
  eval ${_UNSET} mdfa_name;
  eval "${return_ok}";
} # main_do_fileargs()


########################################################################
# main_set_resources ()
#
# Determine options for setting X resources with $_DISPLAY_PROG.
#
# Globals: $_DISPLAY_PROG, $_OUTPUT_FILE_NAME
#
# Variable prefix: msr
#
main_set_resources()
{
  func_check main_set_resources = 0 "$@";
  # $msr_prog   viewer program
  # $msr_rl     resource list
  msr_title="$(get_first_essential \
                 "${_OPT_TITLE}" "${_REGISTERED_TITLE}")";
  exit_test;
  _OUTPUT_FILE_NAME='';
  eval set x "${msr_title}";
  shift;
  until is_equal "$#" 0
  do
    msr_n="$1";
    case "${msr_n}" in
    '')
      continue;
      ;;
    ,*)
      msr_n="$(echo1 "$1" | sed -e 's/^,,*//')";
      exit_test;
      ;;
    esac
    if obj msr_n is_empty
    then
      continue;
    fi;
    if obj _OUTPUT_FILE_NAME is_not_empty
    then
      _OUTPUT_FILE_NAME="${_OUTPUT_FILE_NAME}"',';
    fi;
    _OUTPUT_FILE_NAME="${_OUTPUT_FILE_NAME}${msr_n}";
    shift;
  done;
  case "${_OUTPUT_FILE_NAME}" in
  '')
    _OUTPUT_FILE_NAME='-';
    ;;
  ,*)
    error "main_set_resources(): ${_OUTPUT_FILE_NAME} starts with a comma.";
    ;;
  esac;
  _OUTPUT_FILE_NAME="${_TMP_DIR}/${_OUTPUT_FILE_NAME}";

  if obj _DISPLAY_PROG is_empty
  then				# for example, for groff mode
    _DISPLAY_ARGS='';
    eval ${_UNSET} msr_n;
    eval ${_UNSET} msr_prog;
    eval ${_UNSET} msr_rl;
    eval ${_UNSET} msr_title;
    eval "${return_ok}";
  fi;

  eval set x "${_DISPLAY_PROG}";
  shift;
  msr_prog="$(base_name "$1")";
  exit_test;
  shift;
  if test $# != 0
  then
    if obj _DISPLAY_PROG is_empty
    then
      _DISPLAY_ARGS="$*";
    else
      _DISPLAY_ARGS="$* ${_DISPLAY_ARGS}";
    fi;
  fi;
  msr_rl='';
  if obj _OPT_BD is_not_empty
  then
    case "${msr_prog}" in
    ghostview|gv|gxditview|xditview|xdvi)
      list_append msr_rl '-bd' "${_OPT_BD}";
      ;;
    esac;
  fi;
  if obj _OPT_BG is_not_empty
  then
    case "${msr_prog}" in
    ghostview|gv|gxditview|xditview|xdvi)
      list_append msr_rl '-bg' "${_OPT_BG}";
      ;;
    kghostview)
      list_append msr_rl '--bg' "${_OPT_BG}";
      ;;
    xpdf)
      list_append msr_rl '-papercolor' "${_OPT_BG}";
      ;;
    esac;
  fi;
  if obj _OPT_BW is_not_empty
  then
    case "${msr_prog}" in
    ghostview|gv|gxditview|xditview|xdvi)
      _list_append msr_rl '-bw' "${_OPT_BW}";
      ;;
    esac;
  fi;
  if obj _OPT_FG is_not_empty
  then
    case "${msr_prog}" in
    ghostview|gv|gxditview|xditview|xdvi)
      list_append msr_rl '-fg' "${_OPT_FG}";
      ;;
    kghostview)
      list_append msr_rl '--fg' "${_OPT_FG}";
      ;;
    esac;
  fi;
  if is_not_empty "${_OPT_FN}"
  then
    case "${msr_prog}" in
    ghostview|gv|gxditview|xditview|xdvi)
      list_append msr_rl '-fn' "${_OPT_FN}";
      ;;
    kghostview)
      list_append msr_rl '--fn' "${_OPT_FN}";
      ;;
    esac;
  fi;
  if is_not_empty "${_OPT_GEOMETRY}"
  then
    case "${msr_prog}" in
    ghostview|gv|gxditview|xditview|xdvi|xpdf)
      list_append msr_rl '-geometry' "${_OPT_GEOMETRY}";
      ;;
    kghostview)
      list_append msr_rl '--geometry' "${_OPT_GEOMETRY}";
      ;;
    esac;
  fi;
  if is_empty "${_OPT_RESOLUTION}"
  then
    _OPT_RESOLUTION="${_DEFAULT_RESOLUTION}";
    case "${msr_prog}" in
    gxditview|xditview)
      list_append msr_rl '-resolution' "${_DEFAULT_RESOLUTION}";
      ;;
    xpdf)
      case "${_DEFAULT_RESOLUTION}" in
      75)
        # 72dpi is '100'
        list_append msr_rl '-z' '104';
        ;;
      100)
        list_append msr_rl '-z' '139';
        ;;
      esac;
      ;;
    esac;
  else
    case "${msr_prog}" in
    ghostview|gv|gxditview|xditview|xdvi)
      list_append msr_rl '-resolution' "${_OPT_RESOLUTION}";
      ;;
    xpdf)
      case "${_OPT_RESOLUTION}" in
      75)
        list_append msr_rl '-z' '104';
        # '100' corresponds to 72dpi
        ;;
      100)
        list_append msr_rl '-z' '139';
        ;;
      esac;
      ;;
    esac;
  fi;
  if is_yes "${_OPT_ICONIC}"
  then
    case "${msr_prog}" in
    ghostview|gv|gxditview|xditview|xdvi)
      list_append msr_rl '-iconic';
      ;;
    esac;
  fi;
  if is_yes "${_OPT_RV}"
  then
    case "${msr_prog}" in
    ghostview|gv|gxditview|xditview|xdvi)
      list_append msr_rl '-rv';
      ;;
    esac;
  fi;
  if is_not_empty "${_OPT_XRM}"
  then
    case "${msr_prog}" in
    ghostview|gv|gxditview|xditview|xdvi|xpdf)
      eval set x "${_OPT_XRM}";
      shift;
      for i
      do
        list_append msr_rl '-xrm' "$i";
      done;
      ;;
    esac;
  fi;
  if is_not_empty "${msr_title}"
  then
    case "${msr_prog}" in
    gxditview|xditview)
      list_append msr_rl '-title' "${msr_title}";
      ;;
    esac;
  fi;
  _DISPLAY_ARGS="${msr_rl}";
  eval ${_UNSET} msr_n;
  eval ${_UNSET} msr_prog;
  eval ${_UNSET} msr_rl;
  eval ${_UNSET} msr_title;
  eval "${return_ok}";
} # main_set_resources


########################################################################
# main_display ()
#
# Do the actual display of the whole thing.
#
# Globals:
#   in: $_DISPLAY_MODE, $_OPT_DEVICE,
#       $_ADDOPTS_GROFF, $_ADDOPTS_POST, $_ADDOPTS_X,
#       $_TMP_CAT, $_OPT_PAGER, $PAGER, $_MANOPT_PAGER,
#       $_OUTPUT_FILE_NAME
#
# Variable prefix: md
#
main_display()
{
  func_check main_display = 0 "$@";

  export md_addopts;
  export md_groggy;
  export md_modefile;

  if obj _TMP_CAT is_non_empty_file
  then
    md_modefile="${_OUTPUT_FILE_NAME}";
  else
    echo2 'groffer: empty input.';
    clean_up;
    eval ${_UNSET} md_modefile;
    eval "${return_ok}";
  fi;

  # go to the temporary directory to be able to access internal data files
  cd "${_TMP_DIR}" >"${_NULL_DEV}" 2>&1;

  case "${_DISPLAY_MODE}" in
  groff)
    _ADDOPTS_GROFF="${_ADDOPTS_GROFF} ${_ADDOPTS_POST}";
    if obj _OPT_DEVICE is_not_empty
    then
      _ADDOPTS_GROFF="${_ADDOPTS_GROFF} -T${_OPT_DEVICE}";
    fi;
    md_groggy="$(tmp_cat | eval grog "${md_options}")";
    exit_test;
    _do_opt_V;

    obj md_modefile rm_file;
    mv "${_TMP_CAT}" "${md_modefile}";
    trap_unset;
    cat "${md_modefile}" | \
    {
      trap_set;
      eval "${md_groggy}" "${_ADDOPTS_GROFF}";
    } &
    ;;
  text|tty)
    case "${_OPT_DEVICE}" in
    '')
      md_device="$(get_first_essential \
                     "${_OPT_TEXT_DEVICE}" "${_DEFAULT_TTY_DEVICE}")";
      exit_test;
      ;;
    ascii|cp1047|latin1|utf8)
      md_device="${_OPT_DEVICE}";
      ;;
    *)
      warning "main_display(): \
wrong device for ${_DISPLAY_MODE} mode: ${_OPT_DEVICE}";
      ;;
    esac;
    md_addopts="${_ADDOPTS_GROFF} ${_ADDOPTS_POST}";
    md_groggy="$(tmp_cat | grog -T${md_device})";
    exit_test;
    if obj _DISPLAY_MODE is_equal 'text'
    then
      _do_opt_V;
      tmp_cat | eval "${md_groggy}" "${md_addopts}";
    else
      md_pager='';
      for p in "${_OPT_PAGER}" "${PAGER}" "${_MANOPT_PAGER}" \
               'less -r -R' 'more' 'pager' 'cat'
      do
        md_p="$p";
        if eval is_prog ${md_p}
        then		      # no "" for is_prog() allows args for $p
          md_pager="${md_p}";
          break;
        fi;
      done;
      if obj md_pager is_empty
      then
        error 'main_display(): no pager program found for tty mode';
      fi;
      _do_opt_V;
      tmp_cat | eval "${md_groggy}" "${md_addopts}" | \
                eval "${md_pager}";
    fi;
    clean_up;
    ;;
  source)
    tmp_cat;
    clean_up;
    ;;

  #### viewer modes

  dvi)
    case "${_OPT_DEVICE}" in
    ''|dvi) do_nothing; ;;
    *)
      warning "main_display(): \
wrong device for ${_DISPLAY_MODE} mode: ${_OPT_DEVICE}"
      ;;
    esac;
    md_modefile="${md_modefile}".dvi;
    md_groggy="$(tmp_cat | grog -Tdvi)";
    exit_test;
    _do_display;
    ;;
  html)
    case "${_OPT_DEVICE}" in
    ''|html) do_nothing; ;;
    *)
      warning "main_display(): \
wrong device for ${_DISPLAY_MODE} mode: ${_OPT_DEVICE}";
      ;;
    esac;
    md_modefile="${md_modefile}".html;
    md_groggy="$(tmp_cat | grog -Thtml)";
    exit_test;
    _do_display;
    ;;
  pdf)
    case "${_OPT_DEVICE}" in
    ''|ps)
      do_nothing;
      ;;
    *)
      warning "main_display(): \
wrong device for ${_DISPLAY_MODE} mode: ${_OPT_DEVICE}";
      ;;
    esac;
    md_groggy="$(tmp_cat | grog -Tps)";
    exit_test;
    _do_display _make_pdf;
    ;;
  ps)
    case "${_OPT_DEVICE}" in
    ''|ps)
      do_nothing;
      ;;
    *)
      warning "main_display(): \
wrong device for ${_DISPLAY_MODE} mode: ${_OPT_DEVICE}";
      ;;
    esac;
    md_modefile="${md_modefile}".ps;
    md_groggy="$(tmp_cat | grog -Tps)";
    exit_test;
    _do_display;
    ;;
  x)
    case "${_OPT_DEVICE}" in
    X*)
      md_device="${_OPT_DEVICE}"
      ;;
    *)
      case "${_OPT_RESOLUTION}" in
      100)
        md_device='X100';
        if obj _OPT_GEOMETRY is_empty
        then
          case "${_DISPLAY_PROG}" in
          gxditview|xditview)
            # add width of 800dpi for resolution of 100dpi to the args
            list_append _DISPLAY_ARGS '-geometry' '800';
            ;;
          esac;
        fi;
        ;;
      *)
        md_device='X75-12';
        ;;
      esac
    esac;
    md_groggy="$(tmp_cat | grog -T${md_device} -Z)";
    exit_test;
    _do_display;
    ;;
  X)
    case "${_OPT_DEVICE}" in
    '')
      md_groggy="$(tmp_cat | grog -X)";
      exit_test;
      ;;
    X*|dvi|html|lbp|lj4|ps)
      # these devices work with 
      md_groggy="$(tmp_cat | grog -T"${_OPT_DEVICE}" -X)";
      exit_test;
      ;;
    *)
      warning "main_display(): \
wrong device for ${_DISPLAY_MODE} mode: ${_OPT_DEVICE}";
      md_groggy="$(tmp_cat | grog -Z)";
      exit_test;
      ;;
    esac;
    _do_display;
    ;;
  *)
    error "main_display(): unknown mode \`${_DISPLAY_MODE}'";
    ;;
  esac;
  eval ${_UNSET} md_addopts;
  eval ${_UNSET} md_device;
  eval ${_UNSET} md_groggy;
  eval ${_UNSET} md_modefile;
  eval ${_UNSET} md_options;
  eval ${_UNSET} md_p;
  eval ${_UNSET} md_pager;
  eval "${return_ok}";
} # main_display()


########################
# _do_display ([<prog>])
#
# Perform the generation of the output and view the result.  If an
# argument is given interpret it as a function name that is called in
# the midst (actually only for `pdf').
#
# Globals: $md_modefile, $md_groggy (from main_display())
#
_do_display()
{
  func_check _do_display '>=' 0 "$@";
  _do_opt_V;
  if obj _DISPLAY_PROG is_empty
  then
    trap_unset;
    {
      trap_set;
      eval "${md_groggy}" "${_ADDOPTS_GROFF}" "${_TMP_CAT}";
    } &
  else
    obj md_modefile rm_file;
    cat "${_TMP_CAT}" | \
      eval "${md_groggy}" "${_ADDOPTS_GROFF}" > "${md_modefile}";
    if is_not_empty "$1"
    then
      eval "$1";
    fi;
    obj _TMP_CAT rm_file_with_debug;
    if obj _VIEWER_TERMINAL is_yes # for programs that run on tty
    then
      eval "${_DISPLAY_PROG}" ${_DISPLAY_ARGS} "\"${md_modefile}\"";
    else
      case "${_DISPLAY_PROG}" in
#      lynx\ *|less\ *|more\ *) # programs known to run on the terminal
#        eval "${_DISPLAY_PROG}" ${_DISPLAY_ARGS} "\"${md_modefile}\"";
#        ;;
      *)
        trap_unset;
        {
          trap_set;
          eval "${_DISPLAY_PROG}" ${_DISPLAY_ARGS} "\"${md_modefile}\"";
        } &
        ;;
      esac;
    fi;
  fi;
  eval "${return_ok}";
} # _do_display() of main_display()


#############
# _do_opt_V ()
#
# Check on option `-V'; if set print the corresponding output and leave.
#
# Globals: $_ALL_PARAMS, $_ADDOPTS_GROFF, $_DISPLAY_MODE, $_DISPLAY_PROG,
#          $_DISPLAY_ARGS, $md_groggy,  $md_modefile
#
# Variable prefix: _doV
#
_do_opt_V()
{
  func_check _do_opt_V '=' 0 "$@";
  if obj _OPT_V is_yes
  then
    _OPT_V='no';
    echo1 "Parameters:     ${_ALL_PARAMS}";
    echo1 "Display mode:   ${_DISPLAY_MODE}";
    echo1 "Output file:    ${md_modefile}";
    echo1 "Display prog:   ${_DISPLAY_PROG} ${_DISPLAY_ARGS}";
    a="$(eval echo1 "'${_ADDOPTS_GROFF}'")";
    exit_test;
    echo1 "Output of grog: ${md_groggy} $a";
    _doV_res="$(eval "${md_groggy}" "${_ADDOPTS_GROFF}")";
    exit_test;
    echo1 "groff -V:       ${_doV_res}"
    leave;
  fi;
  eval "${return_ok}";
} # _do_opt_V() of main_display()


##############
# _make_pdf ()
#
# Transform to pdf format; for pdf mode in _do_display().
#
# Globals: $md_modefile (from main_display())
# 
# Variable prefix: _mp
#
_make_pdf()
{
  func_check _do_display '=' 0 "$@";
  _mp_psfile="${md_modefile}";
  md_modefile="${md_modefile}.pdf";
  obj md_modefile rm_file;
  if gs -q -dNOPAUSE -dBATCH -sDEVICE=pdfwrite \
        -sOutputFile="${md_modefile}" -c save pop -f "${_mp_psfile}"
  then
    :;
  else
    error '_make_pdf: could not transform into pdf format.';
  fi;
  obj _mp_psfile rm_file_with_debug;
  eval ${_UNSET} _mp_psfile;
  eval "${return_ok}";
} # _make_pdf() of main_display()


########################################################################
# main (<command_line_args>*)
#
# The main function for groffer.
#
# Arguments:
#
main()
{
  func_check main '>=' 0 "$@";
  # Do not change the sequence of the following functions!
  landmark '13: main_init()';
  main_init;
  landmark '14: main_parse_MANOPT()';
  main_parse_MANOPT;
  landmark '15: main_parse_args()';
  main_parse_args "$@";
  landmark '16: main_set_mode()';
  main_set_mode;
  landmark '17: main_do_fileargs()';
  main_do_fileargs;
  landmark '18: main_set_resources()';
  main_set_resources;
  landmark '19: main_display()';
  main_display;
  eval "${return_ok}";
}


########################################################################

main "$@";
