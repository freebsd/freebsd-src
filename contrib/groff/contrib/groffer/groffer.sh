#!/bin/sh

# groffer - display groff files

# Source file position: <groff-source>/contrib/groffer/groffer.sh

# Copyright (C) 2001,2002,2003 Free Software Foundation, Inc.
# Written by Bernd Warken <bwarken@mayn.de>

# This file is part of groff.

# groff is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or (at your option)
# any later version.

# groff is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
# or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
# License for more details.

# You should have received a copy of the GNU General Public License
# along with groff; see the file COPYING.  If not, write to the
# Free Software Foundation, 59 Temple Place - Suite 330, Boston,
# MA 02111-1307, USA.

export _PROGRAM_NAME;
export _PROGRAM_VERSION;
export _LAST_UPDATE;

_PROGRAM_NAME='groffer';
_PROGRAM_VERSION='0.9.4';
_LAST_UPDATE='22 Jan 2003';

# This program is installed with groff version @VERSION@.

########################################################################
# Determine the shell under which to run this script;
# if `ash' is available restart the script using `ash';
# otherwise just go on.

if test "${_groffer_run}" != 'second'; then
  # only reached during the first run of the script

  export GROFFER_OPT;
  export _groffer_run;
  export _this;

  _this="@BINDIR@/${_PROGRAM_NAME}";

  ###########################
  # _get_opt_shell ("$@")
  #
  # Determine whether `--shell' was specified in $GROFF_OPT or in $*;
  # if so echo its argument.
  #
  _get_opt_shell()
  {
    local i;
    local _sh;
    case " ${GROFFER_OPT} $*" in
      *\ --shell\ *|*\ --shell=*)
        (
          eval set -- "${GROFFER_OPT}" '"$@"';
          _sh='';
          for i in "$@"; do
            case "$1" in
              --shell)
                if test "$#" -ge 2; then
                  _sh="$2";
                  shift;
                fi;
                ;;
              --shell=?*)
                # delete up to first `=' character
                _sh="$(echo -n "$1" | sed -e 's/^[^=]*=//')";
                ;;
            esac;
            shift;
          done;
          echo -n "${_sh}";
        )
        ;;
    esac;
  }


  ###########################
  # _test_on_shell (<name>)
  #
  # Test whether <name> is a shell program of Bourne type (POSIX sh).
  #
  _test_on_shell()
  {
    if test "$#" -le 0 || test "$1" = ''; then
      return 1;
    fi;
    # do not quote $1 to allow arguments
    test "$($1 -c 's=ok; echo -n "$s"' 2>/dev/null)" = 'ok';
  }

  # do the shell determination
  _shell="$(_get_opt_shell "$@")";
  if test "${_shell}" = ''; then
    _shell='ash';
  fi;
  if _test_on_shell "${_shell}"; then
    _groffer_run='second';
    # do not quote $_shell to allow arguments
    exec ${_shell} "${_this}" "$@";
    exit;
  fi;

  # clean-up of shell determination
  unset _shell;
  unset _this;
  unset _groffer_run;
  _get_opt_shell()
  {
    return 0;
  }
  _test_on_shell()
  {
    return 0;
  }

fi;


########################################################################
# diagnostic messages
#
export _DEBUG;
_DEBUG='no';			# disable debugging information
#_DEBUG='yes';			# enable debugging information

export _DEBUG_LM;
_DEBUG_LM='no';			# disable landmark messages
#_DEBUG_LM='yes';		# enable landmark messages


########################################################################
#                          Description
########################################################################

# Display groff files and man pages on X or tty, even when compressed.


### Usage

# Input comes from either standard input or command line parameters
# that represent either names of exisiting roff files or standardized
# specifications for man pages.  All of these can be compressed in a
# format that is decompressible by `gzip'.

# The following displaying modes are available:
# - Display formatted input with the X roff viewer `gxditview',
# - with a Prostcript viewer,
# - with a dvi viewer,
# - with a web browser.
# - Display formatted input in a text terminal using a text device.
# - Generate output for some groff device on stdout without a viewer.
# - Output only the source code without any groff processing.
# - Generate the troff intermediate output on standard output
#   without groff postprocessing.
# By default, the program tries to display with `gxditview' (1); if
# this does not work, text display (2) is used.


### Error handling

# Error handling and exit behavior is complicated by the fact that
# `exit' can only escape from the current shell; trouble occurs in
# subshells.  This was solved by sending kill signals, see
# $_PROCESS_ID and error().


### Compatibility

# This shell script is compatible to the both the GNU and the POSIX
# shell and utilities.  Care was taken to restrict the programming
# technics used here in order to achieve POSIX compatibility as far
# back as POSIX P1003.2 Draft 11.2 of September 1991.

# The only non-builtin used here is POSIX `sed'.  This script was
# tested under `bash', `ash', and `ksh'.  The speed under `ash' is
# more than double when compared to the larger shells.

# This script provides its own option parser.  It is compatible to the
# usual GNU style command line (option clusters, long options, mixing
# of options and non-option file names), except that it is not
# possible to abbreviate long option names.

# The mixing of options and file names can be prohibited by setting
# the environment variable `$POSIXLY_CORRECT' to a non-empty value.
# This enables the rather wicked POSIX behavior to terminate option
# parsing when the first non-option command line argument is found.


########################################################################
#            Survey of functions defined in this document
########################################################################

# The elements specified within paranthesis `(<>)' give hints to what
# the arguments are meant to be; the argument names are irrelevant.
# <>?     0 or 1
# <>*     arbitrarily many such arguments, incl. none
# <>+     one or more such arguments
# <>      exactly 1

# A function that starts with an underscore `_' is an internal
# function for some function.  The internal functions are defined just
# after their corresponding function; they are not mentioned in the
# following.

# abort (text>*)
# base_name (path)
# catz (<file>)
# clean_up ()
# diag (text>*)
# dirname_append (<path> [<dir...>])
# dirname_chop (<path>)
# do_filearg (<filearg>)
# do_nothing ()
# echo2 (<text>*)
# echo2n (<text>*)
# error (<text>*)
# get_first_essential (<arg>*)
# is_dir (<name>)
# is_empty (<string>)
# is_equal (<string1> <string2>)
# is_file (<name>)
# is_not_empty (<string>)
# is_not_equal (<string1> <string2>)
# is_not_file (<name>)
# is_not_prog (<name>)
# is_prog (<name>)
# is_yes (<string>)
# leave ()
# landmark (<text>)
# list_append (<list> <element>...)
# list_check (<list>)
# list_from_args (<arg>...)
# list_from_cmdline (<s_n> <s_a> <l_n> <l_n> [<cmdline_arg>...])
# list_from_split (<string> <separator>)
# list_has (<list> <element>)
# list_has_not (<list> <element>)
# list_length (<list>)
#   main_*(), see after the functions
# man_do_filespec (<filespec>)
# man_setup ()
# man_register_file (<file> [<name> [<section>]])
# man_search_section (<name> <section>)
# man_set()
# manpath_add_lang(<path> <language>)
# manpath_add_system()
# manpath_from_path ()
# normalize_args (<shortopts> <longopts> <arg>*)
# path_chop (<path>)
# path_clean (<path>)
# path_contains (<path> <dir>)
# path_not_contains (<path> <dir>)
# path_split (<path>)
# register_file (<filename>)
# register_title (<filespec>)
# reset ()
# save_stdin ()
# string_contains (<string> <part>)
# string_not_contains (<string> <part>)
# tmp_cat ()
# tmp_create (<suffix>?)
# to_tmp (<filename>)
# trap_clean ()
# trap_set (<functionname>)
# usage ()
# version ()
# warning (<string>)
# whatis (<filename>)
# where (<program>)


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
# External environment variables

# If these variables are exported here then the `ash' shell coughs
# when calling `groff' in `main_display()'.

if test "${GROFFER_EXPORT_EXTERNALS}" = 'yes'; then

  # external system environment variables that are explicitly used
  export DISPLAY;		# Presets the X display.
  export LANG;			# For language specific man pages.
  export LC_ALL;		# For language specific man pages.
  export LC_MESSAGES;		# For language specific man pages.
  export PAGER;			# Paging program for tty mode.
  export PATH;			# Path for the programs called (: list).

  # groffer native environment variables
  export GROFFER_OPT		# preset options for groffer.

  # all groff environment variables are used, see groff(1)
  export GROFF_BIN_PATH;	# Path for all groff programs.
  export GROFF_COMMAND_PREFIX;	# '' (normally) or 'g' (several troffs).
  export GROFF_FONT_PATH;	# Path to non-default groff fonts.
  export GROFF_TMAC_PATH;	# Path to non-default groff macro files.
  export GROFF_TMPDIR;		# Directory for groff temporary files.
  export GROFF_TYPESETTER;	# Preset default device.

  # all GNU man environment variables are used, see man(1).
  export MANOPT;		# Preset options for man pages.
  export MANPATH;		# Search path for man pages (: list).
  export MANROFFSEQ;		# Ignored because of grog guessing.
  export MANSECT;		# Search man pages only in sections (:).
  export SYSTEM;		# Man pages for different OS's (, list).

fi;


########################################################################
# read-only variables (global to this file)
########################################################################

# characters

export _BQUOTE;
export _BSLASH;
export _DQUOTE;
export _NEWLINE;
export _LBRACK;
export _LPAR;
export _RBRACK;
export _RPAR;
export _SPACE;
export _SQUOTE;
export _TAB;

_BQUOTE='`';
_BSLASH='\';
_DQUOTE='"';
_NEWLINE='
';
_LBRACK='[';
_LPAR='(';
_RBRACK=']';
_RPAR=')';
_SPACE=' ';
_SQUOTE="'";
_TAB='	';

# function return values; `0' means ok; other values are error codes
export _ALL_EXIT;
export _BAD;
export _ERROR;
export _GOOD;
export _NO;
export _OK;
export _YES;

_GOOD='0';			# return ok
_BAD='1';			# return negatively, error code `1'
_ERROR='7';			# for syntax errors; no `-1' in `ash'

_ALL_EXIT="${_GOOD} ${_BAD} ${_ERROR}"; # all exit codes (for `trap_set')

_NO="${_BAD}";
_YES="${_GOOD}";
_OK="${_GOOD}";

# quasi-functions, call with `eval'
export return_ok;
export return_good;
export return_bad;
export return_yes;
export return_no;
export return_error;
return_ok="func_pop; return ${_OK}";
return_good="func_pop; return ${_GOOD}";
return_bad="func_pop; return ${_BAD}";
return_yes="func_pop; return ${_YES}";
return_no="func_pop; return ${_NO}";
return_error="func_pop; return ${_ERROR}";


export _CONFFILES;
_CONFFILES="/etc/groff/groffer.conf ${HOME}/.groff/groffer.conf";

export _DEFAULT_MODES;
_DEFAULT_MODES='ps,x,tty';
export _DEFAULT_RESOLUTION;
_DEFAULT_RESOLUTION='100';

export _DEFAULT_TTY_DEVICE;
_DEFAULT_TTY_DEVICE='latin1';

# _VIEWER_* viewer programs for different modes (only X is necessary)
# _VIEWER_* a comma-separated list of viewer programs (with options)
export _VIEWER_DVI;		# viewer program for dvi mode
export _VIEWER_PS;		# viewer program for ps mode
export _VIEWER_WWW_X;		# viewer program for www mode in X
export _VIEWER_WWW_TTY;		# viewer program for www mode in tty
_VIEWER_DVI='xdvi,dvilx';
_VIEWER_PDF='xpdf,acroread';
_VIEWER_PS='gv,ghostview,gs_x11,gs';
_VIEWER_WWW='mozilla,netscape,opera,amaya,arena';
_VIEWER_X='gxditview,xditview';

# Search automatically in standard sections `1' to `8', and in the
# traditional sections `9', `n', and `o'.  On many systems, there
# exist even more sections, mostly containing a set of man pages
# special to a specific program package.  These aren't searched for
# automatically, but must be specified on the command line.
export _MAN_AUTO_SEC;
_MAN_AUTO_SEC="'1' '2' '3' '4' '5' '6' '7' '8' '9' 'n' 'o'"

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
export _OPTS_MAN_SHORT_ARG;
export _OPTS_MAN_SHORT_NA;
export _OPTS_MAN_LONG_ARG;
export _OPTS_MAN_LONG_NA;
export _OPTS_GROFFER_LONG;
export _OPTS_GROFFER_SHORT;
export _OPTS_GROFF_LONG;
export _OPTS_GROFF_SHORT;
export _OPTS_CMDLINE_SHORT_NA;
export _OPTS_CMDLINE_SHORT_ARG;
export _OPTS_CMDLINE_SHORT;
export _OPTS_CMDLINE_LONG_NA;
export _OPTS_CMDLINE_LONG_ARG;
export _OPTS_CMDLINE_LONG;


###### native groffer options

_OPTS_GROFFER_SHORT_NA="'h' 'Q' 'v' 'V' 'X' 'Z'";
_OPTS_GROFFER_SHORT_ARG="'T'";

_OPTS_GROFFER_LONG_NA="'all' 'apropos' 'ascii' 'auto' 'default' 'dvi' \
'groff' 'help' 'intermediate-output' 'local-file' 'location' 'man' \
'no-location' 'no-man' 'pdf' 'ps' 'rv' 'source' 'tty' 'tty-device' \
'version' 'whatis' 'where' 'www' 'x'";

_OPTS_GROFFER_LONG_ARG="'background' 'bd' 'bg' 'bw' 'default-modes' \
'device' 'display' 'dvi-viewer' 'extension' 'fg' 'fn' 'font' \
'foreground' 'geometry' 'locale' 'manpath' 'mode' 'pager' \
'pdf-viewer' 'ps-viewer' 'resolution' 'sections' 'shell' \
'systems' 'title' 'troff-device' 'www-viewer' 'xrm' 'x-viewer'";

##### options inhereted from groff

_OPTS_GROFF_SHORT_NA="'a' 'b' 'c' 'C' 'e' 'E' 'g' 'G' 'i' 'l' 'N' 'p' \
'R' 's' 'S' 't' 'U' 'V' 'z'";
_OPTS_GROFF_SHORT_ARG="'d' 'f' 'F' 'I' 'L' 'm' 'M' 'n' 'o' 'P' 'r' \
'w' 'W'";
_OPTS_GROFF_LONG_NA="";
_OPTS_GROFF_LONG_ARG="";

###### man options (for parsing $MANOPT only)

_OPTS_MAN_SHORT_NA="'7' 'a' 'c' 'd' 'D' 'f' 'h' 'k' 'l' 't' 'u' \
'V' 'w' 'Z'";
_OPTS_MAN_SHORT_ARG="'e' 'L' 'm' 'M' 'p' 'P' 'r' 'S' 'T'";

_OPTS_MAN_LONG_NA="'all' 'ascii' 'apropos' 'catman' 'debug' 'default' \
'ditroff' 'help' 'local-file' 'location' 'troff' 'update' 'version' \
'whatis' 'where'";

_OPTS_MAN_LONG_ARG="'extension' 'locale' 'manpath' \
'pager' 'preprocessor' 'prompt' 'sections' 'systems' 'troff-device'";

###### collections of options

# groffer

_OPTS_GROFFER_LONG="${_OPTS_GROFFER_LONG_ARG} ${_OPTS_GROFFER_LONG_NA}";
_OPTS_GROFFER_SHORT=\
"${_OPTS_GROFFER_SHORT_ARG} ${_OPTS_GROFFER_SHORT_NA}";

# groff

_OPTS_GROFF_LONG="${_OPTS_GROFF_LONG_ARG} ${_OPTS_GROFF_LONG_NA}";
_OPTS_GROFF_SHORT="${_OPTS_GROFF_SHORT_ARG} ${_OPTS_GROFF_SHORT_NA}";

# all command line options

_OPTS_CMDLINE_SHORT_NA="\
${_OPTS_GROFFER_SHORT_NA} ${_OPTS_GROFF_SHORT_NA}";
_OPTS_CMDLINE_SHORT_ARG="\
${_OPTS_GROFFER_SHORT_ARG} ${_OPTS_GROFF_SHORT_ARG}";
_OPTS_CMDLINE_SHORT="${_OPTS_GROFFER_SHORT} ${_OPTS_GROFF_SHORT}";

_OPTS_CMDLINE_LONG_NA="${_OPTS_GROFFER_LONG_NA} \
${_OPTS_GROFF_LONG_NA} ${_OPTS_MAN_LONG_NA}";
_OPTS_CMDLINE_LONG_ARG="${_OPTS_GROFFER_LONG_ARG} \
${_OPTS_GROFF_LONG_ARG} ${_OPTS_MAN_LONG_ARG}";
_OPTS_CMDLINE_LONG="${_OPTS_GROFFER_LONG} ${_OPTS_GROFF_LONG}";


########################################################################
# read-write variables (global to this file)
########################################################################

export _ADDOPTS_GROFF;		# Transp. options for groff (`eval').
export _ADDOPTS_POST;		# Transp. options postproc (`eval').
export _ADDOPTS_X;		# Transp. options X postproc (`eval').
export _DEFAULT_MODES;		# Set default modes.
export _DISPLAY_MODE;		# Display mode.
export _DISPLAY_PROG;		# Viewer program to be used for display.
export _DISPLAY_ARGS;		# X resources for the viewer program.
export _FILEARGS;		# Stores filespec parameters.
export _FUNC_STACK;		# Store debugging information.
export _REGISTERED_TITLE;	# Processed file names.
# _HAS_* from availability tests
export _HAS_COMPRESSION;	# `yes' if compression is available
export _HAS_OPTS_GNU;		# `yes' if GNU `getopt' is available
export _HAS_OPTS_POSIX;		# `yes' if POSIX `getopts' is available
# _MAN_* finally used configuration of man searching
export _MAN_ALL;		# search all man pages per filespec
export _MAN_ENABLE;		# enable search for man pages
export _MAN_EXT;		# extension for man pages
export _MAN_FORCE;		# force file parameter to be man pages
export _MAN_IS_SETUP;		# setup man variables only once
export _MAN_LANG;		# language for man pages
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
export _OPT_APROPOS;		# branch to `apropos' program.
export _OPT_BD;			# set border color in some modes.
export _OPT_BG;			# set background color in some modes.
export _OPT_BW;			# set border width in some modes.
export _OPT_DEBUG;		# print debugging information on stderr.
export _OPT_DEFAULT_MODES;	# `,'-list of modes when no mode given.
export _OPT_DEVICE;		# device option.
export _OPT_DISPLAY;		# set X display.
export _OPT_FG;			# set foreground color in some modes.
export _OPT_FN;			# set font in some modes.
export _OPT_GEOMETRY;		# set size and position of viewer in X.
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
export _OPT_TTY_DEVICE;		# set device for tty mode.
export _OPT_V;			# groff option -V.
export _OPT_VIEWER_DVI;		# viewer program for dvi mode
export _OPT_VIEWER_PDF;		# viewer program for pdf mode
export _OPT_VIEWER_PS;		# viewer program for ps mode
export _OPT_VIEWER_WWW;		# viewer program for www mode
export _OPT_VIEWER_X;		# viewer program for x mode
export _OPT_WHATIS;		# print the one-liner man info
export _OPT_X;			# groff option -X.
export _OPT_XRM;		# specify X resource.
export _OPT_Z;			# groff option -Z.
# _TMP_* temporary files
export _TMP_DIR;		# groff directory for temporary files
export _TMP_DIR_SUB;		# groffer directory for temporary files
export _TMP_CAT;		# stores concatenation of everything
export _TMP_STDIN;		# stores stdin, if any

# these variables are preset in section `Preset' after the rudim. test


########################################################################
#             Test of rudimentary shell functionality
########################################################################


########################################################################
# Test of `test'.
#
test "a" = "a" || exit 1;


########################################################################
# Test of `echo' and the `$()' construct.
#
echo -n '' >/dev/null || exit "${_ERROR}";
if test "$(echo -n 'te' && echo -n '' && echo -n 'st')" != "test"; then
  exit "${_ERROR}";
fi;


########################################################################
# Test of function definitions.
#
_t_e_s_t_f_u_n_c_()
{
  return "${_OK}";
}

if _t_e_s_t_f_u_n_c_ 2>/dev/null; then
  :
else
  echo 'shell does not support function definitions.' >&2;
  exit "${_ERROR}";
fi;


########################################################################
# Preset and reset of read-write global variables
########################################################################


# For variables that can be reset by option `--default', see reset().

_FILEARGS='';

# _HAS_* from availability tests
_HAS_COMPRESSION='';
_HAS_OPTS_GNU='';
_HAS_OPTS_POSIX='';

# _TMP_* temporary files
_TMP_DIR='';
_TMP_DIR_SUB='';
_TMP_CAT='';
_TMP_STDIN='';


########################################################################
# reset ()
#
# Reset the variables that can be affected by options to their default.
#
reset()
{
  if test "$#" -ne 0; then
    error "reset() does not have arguments.";
  fi;

  _ADDOPTS_GROFF='';
  _ADDOPTS_POST='';
  _ADDOPTS_X='';
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
  _MAN_LANG_DONE='no';
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
  _OPT_DEBUG='no';
  _OPT_DEFAULT_MODES='';
  _OPT_DEVICE='';
  _OPT_DISPLAY='';
  _OPT_FG='';
  _OPT_FN='';
  _OPT_GEOMETRY='';
  _OPT_LANG='';
  _OPT_LOCATION='no';
  _OPT_MODE='';
  _OPT_MANPATH='';
  _OPT_PAGER='';
  _OPT_RESOLUTION='';
  _OPT_RV='';
  _OPT_SECTIONS='';
  _OPT_SYSTEMS='';
  _OPT_TITLE='';
  _OPT_TTY_DEVICE='';
  _OPT_V='no';
  _OPT_VIEWER_DVI='';
  _OPT_VIEWER_PDF='';
  _OPT_VIEWER_PS='';
  _OPT_VIEWER_WWW='';
  _OPT_VIEWER_X='';
  _OPT_WHATIS='no';
  _OPT_X='no';
  _OPT_XRM='';
  _OPT_Z='no';

}

reset;


########################################################################
#          Functions for error handling and debugging
########################################################################


##############
# landmark (<text>)
#
# Print <text> to standard error as a debugging aid.
#
# Globals: $_DEBUG_LM
#
landmark()
{
  if test "${_DEBUG_LM}" = 'yes'; then
    echo ">>> $*" >&2;
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
  if test -d "${_TMP_DIR}"; then
    rm -f "${_TMP_DIR}"/*;
    rmdir "${_TMP_DIR}";
  fi;
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
  echo "$*" >&2;
}


##############
# echo2n (<text>*)
#
# Output to stderr.
#
# Arguments : arbitrary text.
#
echo2n()
{
  echo -n "$*" >&2;
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
# Print an error message to standard error; exit with an error condition
#
error()
{
  local i;
  local _code;
  _code="${_ERROR}";
  case "$#" in
    0) true; ;;
    1) echo2 'groffer error: '"$1"; ;;
    2)
      echo2 'groffer error: '"$1";
      _code="$2";
      ;;
    *) echo2 'groffer error: wrong number of arguments in error().'; ;;
  esac;
  if test "${_DEBUG}" = 'yes'; then
    func_stack_dump;
  fi;
  clean_up;
  kill "${_PROCESS_ID}" >/dev/null 2>&1;
  kill -9 "${_PROCESS_ID}" >/dev/null 2>&1;
  exit "${_code}";
}


#############
# abort (<text>*)
#
# Terminate program with error condition
#
abort()
{
  error "Program aborted.";
  exit 1;
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
func_check()
{
  local _comp;
  local _fname;
  local _nargs;
  local _op;
  local _s;
  if test "$#" -lt 3; then
    error 'func_check() needs at least 3 arguments.';
  fi;
  _fname="$1";
  case "$3" in
    1)
      _nargs="$3";
      _s='';
      ;;
    0|[2-9])
      _nargs="$3";
      _s='s';
      ;;
    *)
      error "func_check(): third argument must be a digit.";
      ;;
  esac;
  case "$2" in
    '='|'-eq')
      _op='-eq';
      _comp='exactly';
      ;;
    '>='|'-ge')
      _op='-ge';
      _comp='at least';
      ;;
    '<='|'-le')
      _op='-le';
      _comp='at most';
      ;;
    '<'|'-lt')
      _op='-lt';
      _comp='less than';
      ;;
    '>'|'-gt')
      _op='-gt';
      _comp='more than';
      ;;
    '!='|'-ne')
      _op='-ne';
      _comp='not';
      ;;
    *) 
      error \
        'func_check(): second argument is not a relational operator.';
      ;;
  esac;
  shift 3;
  if test "$#" "${_op}" "${_nargs}"; then
    do_nothing;
  else
    error \
      "${_fname}"'() needs '"${_comp} ${_nargs}"' argument'"${_s}"'.';
  fi;
  if test "${_DEBUG}" = 'yes'; then
    func_push "${_fname} $*";
  fi;
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
  if test "${_DEBUG}" = 'yes'; then
    if test "$#" -ne 0; then
      error 'func_pop() does not have arguments.';
    fi;
    case "${_FUNC_STACK}" in
      '')
        error 'func_pop(): stack is empty.';
        ;;
      *!*)
        # split at first bang `!'.
        _FUNC_STACK="$(echo -n ${_FUNC_STACK} \
                       | sed -e 's/^[^!]*!//')";
        ;;
      *)
        _FUNC_STACK='';
        ;;
    esac;
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
func_push()
{
  local _element;
  if test "${_DEBUG}" = 'yes'; then
    if test "$#" -ne 1; then
      error 'func_push() needs 1 argument.';
    fi;
    case "$1" in
      *'!'*)
        # remove all bangs `!'.
        _element="$(echo -n "$1" | sed -e 's/!//g')";
        ;;
      *)
        _element="$1";
        ;;
    esac;
    if test "${_FUNC_STACK}" = ''; then
      _FUNC_STACK="${_element}";
    else
      _FUNC_STACK="${_element}!${_FUNC_STACK}";
    fi;
  fi;
}


#############
# func_stack_dump ()
#
# Print the content of the stack.  Ignore the arguments.
#
func_stack_dump()
{
  diag 'call stack:';
  case "${_FUNC_STACK}" in
    *!*)
      _rest="${_FUNC_STACK}";
      while test "${_rest}" != ''; do
        # get part before the first bang `!'.
        diag "$(echo -n "${_rest}" | sed -e 's/!.*$//')";
        # delete part before and including the first bang `!'.
        _rest="$(echo -n "${_rest}" | sed -e 's/^[^!]*!//')";
      done;
      ;;
    *)
      diag "${_FUNC_STACK}";
      ;;
  esac;
}


########################################################################
#                        System Test
########################################################################

landmark "2: system test";

# Test the availability of the system utilities used in this script.


########################################################################
# Test of `true'.
#
if true >/dev/null 2>&1; then
  true;
else
  true()
  {
    return "${_GOOD}";
  }

  false()
  {
    return "${_BAD}";
  }
fi;


########################################################################
# Test of `unset'.
#
_test='test';
if unset _test >/dev/null 2>&1 && test "${_test}" = ''; then
  true;
else
  unset()
  {
    for v in "$@"; do
      eval "$v"='';
    done;
  }
fi;
unset _test;

########################################################################
# Test of builtin `local'
#

_t_e_s_t_f_u_n_c_()
{
  local _test >/dev/null 2>&1 || return "${_BAD}";
}

if _t_e_s_t_f_u_n_c_; then
  :
else
  local()
  {
    if test "$1" != ''; then
      error "overriding global variable \`$1' with local value.";
    fi;
  }
fi;


########################################################################
# Test of global setting in functions
#
_global='outside';
_clobber='outside';

_t_e_s_t_f_u_n_c_()
{
  local _clobber;
  _global='inside';
  _clobber='inside';
}

_t_e_s_t_f_u_n_c_;
if test "${_global}" != 'inside' || test "${_clobber}" != 'outside';
then
  error "Cannot assign to global variables from within functions.";
fi;

unset _global;
unset _clobber;


########################################################################
# Test of function `sed'.
#
if test "$(echo xTesTx \
           | sed -e 's/^.\([Tt]e*x*sTT*\).*$/\1/' \
           | sed -e '\|T|s||t|g')" != 'test';
then
  error 'Test of "sed" command failed.';
fi;


########################################################################
# Test of function `cat'.
#
if test "$(echo test | cat)" != "test"; then
  error 'Test of "cat" command failed.';
fi;


########################################################################
# Test for compression.
#
if test "$(echo 'test' | gzip -c -d -f - 2>/dev/null)" = 'test'; then
  _HAS_COMPRESSION='yes';
  if echo 'test' | bzip2 -c 2>/dev/null | bzip2 -t 2>/dev/null \
     && test "$(echo 'test' | bzip2 -c 2>/dev/null \
                            | bzip2 -d -c 2>/dev/null)" \
             = 'test'; then
    _HAS_BZIP='yes';
  else
    _HAS_BZIP='no';
  fi;
else
  _HAS_COMPRESSION='no';
  _HAS_BZIP='no';
fi;


########################################################################
_t_e_s_t_f_u_n_c_()
{
  :
}


########################################################################
#                   Definition of normal Functions
########################################################################
landmark "3: functions";

########################################################################
# abort (<text>*)
#
# Unconditionally terminate the program with error code;
# useful for debugging.
#
# defined above


########################################################################
# base_name (<path>)
#
# Get the file name part of <path>, i.e. delete everything up to last
# `/' from the beginning of <path>.
#
# Arguments : 1
# Output    : the file name part (without slashes)
#
base_name()
{
  func_check base_name = 1 "$@";
  case "$1" in
    */)
      do_nothing;
      ;;
    */*)
      # delete everything before and including the last slash `/'.
      echo -n "$1" | sed -e '\|^.*//*\([^/]*\)$|s||\1|';
      ;;
    *)
      echo -n "$1";
      ;;
  esac;
  eval "${return_ok}";
}


########################################################################
# catz (<file>)
#
# Decompress if possible or just print <file> to standard output.
#
# gzip, bzip2, and .Z decompression is supported.
#
# Arguments: 1, a file name.
# Output: the content of <file>, possibly decompressed.
#
if test "${_HAS_COMPRESSION}" = 'yes'; then
  catz()
  {
    func_check catz = 1 "$@";
    case "$1" in
      '')
        error 'catz(): empty file name';
        ;;
      '-')
        error 'catz(): for standard input use save_stdin()';
        ;;
    esac;
    if is_yes "${_HAS_BZIP}"; then
      if bzip2 -t "$1" 2>/dev/null; then
        bzip2 -c -d "$1" 2>/dev/null;
        eval "${return_ok}";
      fi;
    fi;
    gzip -c -d -f "$1" 2>/dev/null;
    eval "${return_ok}";
  }
else
  catz()
  {
    func_check catz = 1 "$@";
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
  local _res;
  if is_empty "$1"; then
    error "dir_append(): first argument is empty.";
  fi;
  if is_empty "$2"; then
    echo -n "$1";
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
dirname_chop()
{
  func_check dirname_chop = 1 "$@";
  local _arg;
  local _res;
  local _sep;
  # replace all multiple slashes by a single slash `/'.
  _res="$(echo -n "$1" | sed -e '\|///*|s||/|g')";
  case "${_res}" in
    ?*/)
      # remove trailing slash '/';
      echo -n "${_res}" | sed -e '\|/$|s|||';
      ;;
    *) echo -n "${_res}"; ;;
  esac;
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
#   - name of an existing files.
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
do_filearg()
{
  func_check do_filearg = 1 "$@";
  local _filespec;
  local i;
  _filespec="$1";
  # store sequence into positional parameters
  case "${_filespec}" in
    '')
       eval "${return_good}";
       ;;
    '-')
      register_file '-';
      eval "${return_good}";
      ;;
    */*)			# with directory part; so no man search
      set -- 'File';
      ;;
    *)
      if is_yes "${_MAN_ENABLE}"; then
        if is_yes "${_MAN_FORCE}"; then
          set -- 'Manpage' 'File';
        else
          set -- 'File' 'Manpage';
        fi;
      else
        set -- 'File';
      fi;
      ;;
  esac;
  for i in "$@"; do
    case "$i" in
      File)
        if test -f "${_filespec}"; then
          if test -r "${_filespec}"; then
            register_file "${_filespec}";
            eval "${return_good}";
          else
	    echo2 "could not read \`${_filespec}'";
            eval "${return_bad}";
          fi;
        else
          continue;
        fi;
        ;;
      Manpage)			# parse filespec as man page
        if is_not_yes "${_MAN_IS_SETUP}"; then
          man_setup;
        fi;
        if man_do_filespec "${_filespec}"; then
          eval "${return_good}";
        else
          continue;
	fi;
        ;;
    esac;
  done;
  eval "${return_bad}";
} # do_filearg()


########################################################################
# do_nothing ()
#
# Dummy function.
#
do_nothing()
{
  return "${_OK}";
}


########################################################################
# echo2 (<text>*)
#
# Print to standard error with final line break.
#
# defined above


########################################################################
# echo2n (<text>*)
#
# Print to standard error without final line break.
#
# defined above


########################################################################
# error (<text>*)
#
# Print error message and exit with error code.
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
get_first_essential()
{
  func_check get_first_essential '>=' 0 "$@";
  local i;
  if test "$#" -eq 0; then
    eval "${return_ok}";
  fi;
  for i in "$@"; do
    if is_not_empty "$i"; then
      echo -n "$i";
      eval "${return_ok}";
    fi;
  done;
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
  func_check is_dir = 1 "$@";
  if is_not_empty "$1" && test -d "$1" && test -r "$1"; then
    eval "${return_yes}";
  else
    eval "${return_no}";
  fi;
  eval "${return_ok}";
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
  func_check is_empty = 1 "$@";
  if test -z "$1"; then
    eval "${return_yes}";
  else
    eval "${return_no}";
  fi;
  eval "${return_ok}";
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
  func_check is_equal = 2 "$@";
  if test "$1" = "$2"; then
    eval "${return_yes}";
  else
    eval "${return_no}";
  fi;
  eval "${return_ok}";
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
  func_check is_file = 1 "$@";
  if is_not_empty "$1" && test -f "$1" && test -r "$1"; then
    eval "${return_yes}";
  else
    eval "${return_no}";
  fi;
  eval "${return_ok}";
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
  func_check is_not_dir = 1 "$@";
  if is_dir "$1"; then
    eval "${return_no}";
  else
    eval "${return_yes}";
  fi;
  eval "${return_ok}";
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
  func_check is_not_empty = 1 "$@";
  if is_empty "$1"; then
    eval "${return_no}";
  else
    eval "${return_yes}";
  fi;
  eval "${return_ok}";
}


########################################################################
# is_not_equal (<string1> <string2>)
#
# Test whether `string1' and <string2> differ.
#
# Arguments : 2
#
is_not_equal()
{
  func_check is_not_equal = 2 "$@";
  if is_equal "$1" "$2"; then
    eval "${return_no}";
  else
    eval "${return_yes}";
  fi;
  eval "${return_ok}";
}


########################################################################
# is_not_file (<filename>)
#
# Test whether `name' is a not readable file.
#
# Arguments : >=1 (empty allowed), more args are ignored
#
is_not_file()
{
  func_check is_not_file '>=' 1 "$@";
  if is_file "$1"; then
    eval "${return_no}";
  else
    eval "${return_yes}";
  fi;
  eval "${return_ok}";
}


########################################################################
# is_not_prog (<name>)
#
# Verify that arg is a not program in $PATH.
#
# Arguments : >=1 (empty allowed)
#   more args are ignored, this allows to specify progs with arguments
#
is_not_prog()
{
  func_check is_not_prog '>=' 1 "$@";
  if where "$1" >/dev/null; then
    eval "${return_no}";
  else
    eval "${return_yes}";
  fi;
  eval "${return_ok}";
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
  if test "$1" = 'yes'; then
    eval "${return_no}";
  else
    eval "${return_yes}";
  fi;
  eval "${return_ok}";
}


########################################################################
# is_prog (<name>)
#
# Determine whether arg is a program in $PATH
#
# Arguments : >=1 (empty allowed)
#   more args are ignored, this allows to specify progs with arguments
#
is_prog()
{
  func_check is_prog '>=' 1 "$@";
  if where "$1" >/dev/null; then
    eval "${return_yes}";
  else
    eval "${return_no}";
  fi;
  eval "${return_ok}";
}


########################################################################
# is_yes (<string>)
#
# Test whether `string' has value "yes".
#
# Arguments : <=1
# Return    : `0' if arg1 is `yes', `1' otherwise.
#
is_yes()
{
  func_check is_yes = 1 "$@";
  if test "$1" = 'yes'; then
    eval "${return_yes}";
  else
    eval "${return_no}";
  fi;
  eval "${return_ok}";
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
# leave ()
#
# Clean exit without an error.
#
leave()
{
  clean_up;
  exit "${_OK}";
}


########################################################################
landmark '6: list_*()';
########################################################################

########################################################################
# list_append (<list> <element>...)
#
# Arguments: >=2
#   <list>:   a space-separated list of single-quoted elements.
#   <element>: some sequence of characters.
# Output:
#   if <list> is empty:  "'<element>' '...'"
#   otherwise:           "<list> '<element>' ..."
#
list_append()
{
  func_check list_append '>=' 2 "$@";
  local _element;
  local _res;
  _res="$1";
  shift;
  for s in "$@"; do
    case "$s" in
      *\'*)
        # escape each single quote by replacing each "'" (squote)
        # by "'\''" (squote bslash squote squote);
        # note that the backslash must be doubled for `sed'.
        _element="$(echo -n "$s" | sed -e 's/'"${_SQUOTE}"'/&\\&&/g')";
        ;;
      *)
        _element="$s";
        ;;
    esac;
    _res="${_res} '${_element}'";
  done;
  echo -n "${_res}";
  eval "${return_ok}";
}


########################################################################
# list_check (<list>)
#
# Check whether <list> is a space-separated list of '-quoted elements.
#
# If the test fails an error is raised.
# If the test succeeds the argument is echoed.
#
# Testing criteria:
#   A list has the form "'first' 'second' '...' 'last'".
#   So it has a leading and a final quote and the elements are separated
#   by "' '" constructs.  If these are all removed there should not be
#   any single-quotes left.  Watch out for escaped single quotes; they
#   have the form '\'' (sq bs sq sq).
#
# Arguments: 1
# Output: the argument <list> unchanged, it the check succeeded.
#
list_check()
{
  func_check list_check = 1 "$@";
  local _list;
  if is_empty "$1"; then
    eval "${return_ok}";
  fi;
  case "$1" in
    \'*\') _list="$1"; ;;
    *)
      error "list_check() bad list: $1"
      ;;
  esac;
  # Remove leading single quote,
  # remove final single quote,
  # remove escaped single quotes (squote bslash squote squote)
  #   [note that `sed' doubles the backslash (bslash bslash)],
  # remove field separators (squote space squote).
  _list="$(echo -n "${_list}" \
    | sed -e 's/^'"${_SQUOTE}"'//' \
    | sed -e 's/'"${_SQUOTE}"'$//' \
    | sed -e \
        's/'"${_SQUOTE}${_BSLASH}${_BSLASH}${_SQUOTE}${_SQUOTE}"'//g' \
    | sed -e 's/'"${_SQUOTE}${_SPACE}${_SPACE}"'*'"${_SQUOTE}"'//g')";
  case "${_list}" in
    *\'*)			# criterium fails if squote is left
      error 'list_check() bad list: '"${_list}";
      ;;
  esac;
  echo -n "$1";
  eval "${return_ok}";
}


########################################################################
# list_element_from_arg (<arg>)
#
# Arguments: 1
#   <arg>: some sequence of characters (also single quotes allowed).
# Output: the list element generated from <arg>.
#
list_element_from_arg()
{
  func_check list_element_from_arg = 1 "$@";
  local _res;
  echo -n "'";
  # replace each single quote "'" by "'\''". 
  echo -n "$1" | sed -e 's/'\''/&\\&&/g'; # ' for emacs
  echo -n "'";
  eval "${return_ok}";
}


########################################################################
# list_from_args (<arg>...)
#
# Generate a space-separated list of single-quoted elements from args.
#
# Arguments:
#   <arg>: some sequence of characters.
# Output: "'<arg>' '...'"
#
list_from_args()
{
  func_check list_from_args '>=' 1 "$@";
  local _list;
  _list="";
  for s in "$@"; do
    _list="$(list_append "${_list}" "$s")";
  done;
  echo -n "${_list}";
  eval "${return_ok}";
}


########################################################################
# list_from_cmdline (<s_n> <s_a> <l_n> <l_a> [<cmdline_arg>...])
#
# Transform command line arguments into a normalized form.
#
# Options, option arguments, and file parameters are identified and
# output each as a single-quoted argument of its own.  Options and
# file parameters are separated by a '--' argument.
#
# Arguments: >=4
#   <s_n>: space-separated list of short options without an arg.
#   <s_a>: space-separated list of short options that have an arg.
#   <l_n>: space-separated list of long options without an arg.
#   <l_a>: space-separated list of long options that have an arg.
#   <cmdline_arg>...: the arguments from the command line (by "$@").
#
# Globals: $POSIXLY_CORRECT (only kept for compatibility).
#
# Output: ['-[-]opt' ['optarg']]... '--' ['filename']...
#
# Example:
#     list_normalize 'a b' 'c' '' 'long' -a f1 -bcarg --long=larg f2
#   will result in printing:
#     '-a' '-b' '-c' 'arg' '--long' 'larg' '--' 'f1' 'f2'
#   If $POSIXLY_CORRECT is not empty, the result will be:
#     '-a' '--' 'f1' '-bcarg' '--long=larg' 'f2'
#
#   Rationale:
#     In POSIX, the first non-option ends the option processing.
#     In GNU mode (default), non-options are sorted behind the options.
#
#   Use this function only in the following way:
#     eval set -- "$(args_norm '...' '...' '...' '...' "$@")";
#     while test "$1" != '--'; do
#       case "$1" in
#       ...
#       esac;
#       shift;
#     done;
#     shift;         #skip '--'
#     # all positional parameters ("$@") left are file name parameters.
#
list_from_cmdline()
{
  func_check list_from_cmdline '>=' 4 "$@";
  local _fparams;
  local _fn;
  local _result;
  local _long_a;
  local _long_n;
  local _short_a;
  local _short_n;
  _short_n="$(list_check "$1")"; # short options, no argument
  _short_a="$(list_check "$2")"; # short options with argument
  _long_n="$(list_check "$3")";	 # long options, no argument
  _long_a="$(list_check "$4")";	 # long options with argument
  shift 4;
  _fn='list_from_cmdline():';	 # for error messages
  if test "$#" -eq 0; then
    echo -n "'--'";
    eval "${return_ok}";
  fi;
  _fparams='';
  _result='';
  while test "$#" -ge 1; do
    _arg="$1";
    shift;
    case "$_arg" in
      --) break; ;;
      --?*)
        # delete leading '--';
        _opt="$(echo -n "${_arg}" | sed -e 's/^..//')";
        if list_has "${_long_n}" "${_opt}"; then
          # long option, no argument
          _result="$(list_append "${_result}" "--${_opt}")";
          continue;
        fi;
        if list_has "${_long_a}" "${_opt}"; then
          # long option with argument
          _result="$(list_append "${_result}" "--${_opt}")";
          if test "$#" -le 0; then
            error "${_fn} no argument for option --${_opt}."
          fi;
          _result="$(list_append "${_result}" "$1")";
          shift;
          continue;
        fi;
        # test on `--opt=arg'
        if string_contains "${_opt}" '='; then
          # extract option by deleting from the first '=' to the end
          _lopt="$(echo -n "${_opt}" | sed -e 's/=.*$//')";
          if list_has "${_long_a}" "${_lopt}"; then
            # get the option argument by deleting up to first `='
            _optarg="$(echo -n "${_opt}" | sed -e 's/^[^=]*=//')";
            _result="$(list_append "${_result}" \
                                   "--${_lopt}" "${_optarg}")";
            continue;
          fi;
        fi;
        error "${_fn} --${_opt} is not an option."
        ;;
      -?*)			# short option (cluster)
        # delete leading `-';
        _rest="$(echo -n "${_arg}" | sed -e 's/^.//')";
        while is_not_empty "${_rest}"; do
          # get next short option from cluster (first char of $_rest)
          _optchar="$(echo -n "${_rest}" | sed -e 's/^\(.\).*$/\1/')";
          # remove first character from ${_rest};
          _rest="$(echo -n "${_rest}" | sed -e 's/^.//')";
          if list_has "${_short_n}" "${_optchar}"; then
            _result="$(list_append "${_result}" "-${_optchar}")";
            continue;
          elif list_has "${_short_a}" "${_optchar}"; then
            # remove leading character
            case "${_optchar}" in
              /)
                _rest="$(echo -n "${_rest}" | sed -e '\|^.|s|||')";
                ;;
              ?)
                _rest="$(echo -n "${_rest}" | sed -e 's/^.//')";
                ;;
              ??*)
                error "${_fn} several chars parsed for short option."
                ;;
            esac;
            if is_empty "${_rest}"; then
              if test "$#" -ge 1; then
                _result="$(list_append "${_result}" \
                                          "-${_optchar}" "$1")";
                shift;
                continue;
              else
                error \
                  "${_fn}"' no argument for option -'"${_optchar}."
              fi;
            else		# rest is the argument
              _result="$(list_append "${_result}" \
                                     "-${_optchar}" "${_rest}")";
              _rest='';
              continue;
            fi;
          else
            error "${_fn} unknown option -${_optchar}."
          fi;
        done;
        ;;
      *)
	# Here, $_arg is not an option, so a file parameter.
        # When $POSIXLY_CORRECT is set this ends option parsing;
        # otherwise, the argument is stored as a file parameter and
        # option processing is continued.
        _fparams="$(list_append "${_fparams}" "${_arg}")";
	if is_not_empty "$POSIXLY_CORRECT"; then
          break;
        fi;
        ;;
    esac;
  done;
  _result="$(list_append "${_result}" '--')";
  if is_not_empty "${_fparams}"; then
    _result="${_result} ${_fparams}";
  fi;
  if test "$#" -gt 0; then
    _result="$(list_append "${_result}" "$@")";
  fi;
  echo -n "$_result";
  eval "${return_ok}";
} # list_from_cmdline()


########################################################################
# list_from_lists (<list1> <list2>...)
#
# Generate a list from the concatenation of the lists in the arguments.
#
# Arguments: >=2
#   <list*>: string of space-separated single-quoted elements.
# Output: "'<element1_of_list1>' ..."
#
list_from_lists()
{
  func_check list_from_lists '>=' 2 "$@";
  _list='';
  echo -n "$*";
  eval "${return_ok}";
}

########################################################################
# list_from_split (<string> <separator>)
#
# In <string> escape white space and replace each <separator> by space.
#
# Arguments: 2: a <string> that is to be split into parts divided by
#               <separator>
# Output:    the resulting string
#
list_from_split()
{
  func_check list_from_split = 2 "$@";
  local _s;

  # precede each space or tab by a backslash `\' (doubled for `sed')
  _s="$(echo -n "$1" | sed -e 's/\(['"${_SPACE}${_TAB}"']\)/\\\1/g')";

  # replace split character of string by the list separator ` ' (space).
  case "$2" in
    /)				# cannot use normal `sed' separator
      echo -n "${_s}" | sed -e '\|'"$2"'|s|| |g';
      ;;
    ?)				# use normal `sed' separator
      echo -n "${_s}" | sed -e 's/'"$2"'/ /g';
      ;;
    ??*)
      error 'list_from_split(): separator must be a single character.';
      ;;
  esac;
  eval "${return_ok}";
}


########################################################################
# list_has (<list> <element>)
#
# Arguments: 2
#   <list>:    a space-separated list of single-quoted elements.
#   <element>: some sequence of characters.
# Output:
#   if <list> is empty:  "'<element>' '...'"
#   otherwise:           "<list> '<element>' ..."
#
list_has()
{
  func_check list_has = 2 "$@";
  if is_empty "$1"; then
    eval "${return_no}";
  fi;
  _list="$1";
  _element="$2";
  case "$2" in
    \'*\')  _element="$2"; ;;
    *)      _element="'$2'"; ;;
  esac;
  if string_contains "${_list}" "${_element}"; then
    eval "${return_yes}";
  else
    eval "${return_no}";
  fi;
  eval "${return_ok}";
}


########################################################################
# list_has_not (<list> <element>)
#
# Arguments: 2
#   <list>:    a space-separated list of single-quoted elements.
#   <element>: some sequence of characters.
# Output:
#   if <list> is empty:  "'<element>' '...'"
#   otherwise:           "<list> '<element>' ..."
#
list_has_not()
{
  func_check list_has_not = 2 "$@";
  if is_empty "$1"; then
    eval "${return_yes}";
  fi;
  _list="$1";
  _element="$2";
  case "$2" in
    \'*\')  _element="$2"; ;;
    *)      _element="'$2'"; ;;
  esac;
  if string_contains "${_list}" "${_element}"; then
    eval "${return_no}";
  else
    eval "${return_yes}";
  fi;
  eval "${return_ok}";
}


########################################################################
# list_length (<list>)
#
# Arguments: 1
#   <list>:    a space-separated list of single-quoted elements.
# Output: the number of elements in <list>
#
list_length()
{
  func_check list_length = 1 "$@";
  eval set -- "$1";
  echo -n "$#";
  eval "${return_ok}";
}


########################################################################
# list_prepend (<list> <element>...)
#
# Insert new <element> at the beginning of <list>
#
# Arguments: >=2
#   <list>:   a space-separated list of single-quoted elements.
#   <element>: some sequence of characters.
# Output:
#   if <list> is empty:  "'<element>' ..."
#   otherwise:           "'<element>' ... <list>"
#
list_prepend()
{
  func_check list_prepend '>=' 2 "$@";
  local _res;
  _res="$1";
  shift;
  for s in "$@"; do
    # escape single quotes in list style (squote bslash squote squote).
    _element="$(echo -n "$s" | sed -e 's/'\''/&\\&&/g')";
    _res="'${_element}' ${_res}";
  done;
  echo -n "${_res}";
  eval "${return_ok}";
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
# Only called from do_fileargs(), checks on $MANPATH and
# $_MAN_ENABLE are assumed.
#
man_do_filespec()
{
  func_check man_do_filespec = 1 "$@";
  local _got_one;
  local _name;
  local _prevsec;
  local _res;
  local _section;
  local _spec;
  local _string;
  local s;
  if is_empty "${_MAN_PATH}"; then
    eval "${return_bad}";
  fi;
  if is_empty "$1"; then
    eval "${return_bad}";
  fi;
  _spec="$1";
  _name='';
  _section='';
  case "${_spec}" in
    */*)			# not a man spec when it contains '/'
      eval "${return_bad}";
      ;;
    man:?*\(?*\))		# man:name(section)
      _name="$(echo -n "${_spec}" \
               | sed -e 's/^man:\(..*\)(\(..*\))$/\1/')";
      _section="$(echo -n "${_spec}" \
               | sed -e 's/^man:\(..*\)(\(..*\))$/\2/')";
      ;;
    man:?*.?*)			# man:name.section
      _name="$(echo -n "${_spec}" \
               | sed -e 's/^man:\(..*\)\.\(..*\)$/\1/')";
      _section="$(echo -n "${_spec}" \
               | sed -e 's/^man:\(..*\)\.\(..*\)$/\2/')";
      ;;
    man:?*)			# man:name
      _name="$(echo -n "${_spec}" | sed -e 's/^man://')";
      ;;
    ?*\(?*\))			# name(section)
      _name="$(echo -n "${_spec}" \
               | sed -e 's/^\(..*\)(\(..*\))$/\1/')";
      _section="$(echo -n "${_spec}" \
               | sed -e 's/^\(..*\)(\(..*\))$/\2/')";
      ;;
    ?*.?*)			# name.section
      _name="$(echo -n "${_spec}" \
               | sed -e 's/^\(..*\)\.\(..*\)$/\1/')";
      _section="$(echo -n "${_spec}" \
               | sed -e 's/^\(..*\)\.\(..*\)$/\2/')";
      ;;
    ?*)
      _name="${_filespec}";
      ;;
  esac;
  if is_empty "${_name}"; then
    eval "${return_bad}";
  fi;
  _got_one='no';
  if is_empty "${_section}"; then
    eval set -- "${_MAN_AUTO_SEC}";
    for s in "$@"; do
      if man_search_section "${_name}" "$s"; then # found
        if is_yes "${_MAN_ALL}"; then
          _got_one='yes';
        else
          eval "${return_good}";
        fi;
      fi;
    done;
  else
    if man_search_section "${_name}" "${_section}"; then
      eval "${return_good}";
    else
      eval "${return_bad}";
    fi;
  fi;
  if is_yes "${_MAN_ALL}" && is_yes "${_got_one}"; then
    eval "${return_good}";
  fi;
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
  if is_empty "$1"; then
    error 'man_register_file(): file name is empty';
  fi;
  to_tmp "$1";
  case "$#" in
    2)
       register_title "man:$2";
       eval "${return_ok}";
       ;;
    3)
       register_title "$2($3)";
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
man_search_section()
{
  func_check man_search_section = 2 "$@";
  local _dir;
  local _ext;
  local _got_one;
  local _name;
  local _prefix
  local _section;
  local d;
  local f;
  if is_empty "${_MAN_PATH}"; then
    eval "${return_bad}";
  fi;
  if is_empty "$1"; then
    eval "${return_bad}";
  fi;
  if is_empty "$2"; then
    eval "${return_bad}";
  fi;
  _name="$1";
  _section="$2";
  eval set -- "$(path_split "${_MAN_PATH}")";
  _got_one='no';
  if is_empty "${_MAN_EXT}"; then
    for d in "$@"; do
      _dir="$(dirname_append "$d" "man${_section}")";
      if is_dir "${_dir}"; then
        _prefix="$(dirname_append "${_dir}" "${_name}.${_section}")";
        for f in $(echo -n ${_prefix}*); do
          if is_file "$f"; then
            if is_yes "${_got_one}"; then
              register_file "$f";
            elif is_yes "${_MAN_ALL}"; then
              man_register_file "$f" "${_name}";
            else
              man_register_file "$f" "${_name}" "${_section}";
              eval "${return_good}";
            fi;
            _got_one='yes';
          fi;
        done;
      fi;
    done;
  else
    _ext="${_MAN_EXT}";
    # check for directory name having trailing extension
    for d in "$@"; do
      _dir="$(dirname_append $d man${_section}${_ext})";
      if is_dir "${_dir}"; then
        _prefix="$(dirname_append "${_dir}" "${_name}.${_section}")";
        for f in ${_prefix}*; do
          if is_file "$f"; then
            if is_yes "${_got_one}"; then
              register_file "$f";
            elif is_yes "${_MAN_ALL}"; then
              man_register_file "$f" "${_name}";
            else
              man_register_file "$f" "${_name}" "${_section}";
              eval "${return_good}";
            fi;
            _got_one='yes';
          fi;
        done;
      fi;
    done;
    # check for files with extension in directories without extension
    for d in "$@"; do
      _dir="$(dirname_append "$d" "man${_section}")";
      if is_dir "${_dir}"; then
        _prefix="$(dirname_append "${_dir}" \
                                  "${_name}.${_section}${_ext}")";
        for f in ${_prefix}*; do
          if is_file "$f"; then
            if is_yes "${_got_one}"; then
              register_file "$f";
            elif is_yes "${_MAN_ALL}"; then
              man_register_file "$f" "${_name}";
            else
              man_register_file "$f" "${_name}" "${_section}";
              eval "${return_good}";
            fi;
            _got_one='yes';
          fi;
        done;
      fi;
    done;
  fi;
  if is_yes "${_MAN_ALL}" && is_yes "${_got_one}"; then
    eval "${return_good}";
  fi;
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
man_setup()
{
  func_check main_man_setup = 0 "$@";
  local _lang;

  if is_yes "${_MAN_IS_SETUP}"; then
    eval "${return_ok}";
  fi;
  _MAN_IS_SETUP='yes';

  if is_not_yes "${_MAN_ENABLE}"; then
    eval "${return_ok}";
  fi;

  # determine basic path for man pages
  _MAN_PATH="$(get_first_essential \
               "${_OPT_MANPATH}" "${_MANOPT_PATH}" "${MANPATH}")";
  if is_empty "${_MAN_PATH}"; then
    if is_prog 'manpath'; then
      _MAN_PATH="$(manpath 2>/dev/null)"; # not always available
    fi;
  fi;
  if is_empty "${_MAN_PATH}"; then
    manpath_set_from_path;
  else
    _MAN_PATH="$(path_clean "${_MAN_PATH}")";
  fi;
  if is_empty "${_MAN_PATH}"; then
    _MAN_ENABLE="no";
    eval "${return_ok}";
  fi;

  _MAN_ALL="$(get_first_essential "${_OPT_ALL}" "${_MANOPT_ALL}")";
  if is_empty "${_MAN_ALL}"; then
    _MAN_ALL='no';
  fi;

  _MAN_SYS="$(get_first_essential \
              "${_OPT_SYSTEMS}" "${_MANOPT_SYS}" "${SYSTEM}")";
  _lang="$(get_first_essential \
           "${_OPT_LANG}" "${LC_ALL}" "${LC_MESSAGES}" "${LANG}")";
  case "${_lang}" in
    C|POSIX)
      _MAN_LANG="";
      _MAN_LANG2="";
      ;;
    ?)
      _MAN_LANG="${_lang}";
      _MAN_LANG2="";
      ;;
    *)
      _MAN_LANG="${_lang}";
      # get first two characters of $_lang
      _MAN_LANG2="$(echo -n "${_lang}" | sed -e 's/^\(..\).*$/\1/')";
      ;;
  esac;
  # from now on, use only $_LANG, forget about $_OPT_LANG, $LC_*.

  manpath_add_lang_sys;		# this is very slow

  _MAN_SEC="$(get_first_essential \
              "${_OPT_SECT}" "${_MANOPT_SEC}" "${MANSEC}")";
  if is_empty "${_MAN_PATH}"; then
    _MAN_ENABLE="no";
    eval "${return_ok}";
  fi;

  _MAN_EXT="$(get_first_essential \
              "${_OPT_EXTENSION}" "${_MANOPT_EXTENSION}")";
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
manpath_add_lang_sys()
{
  func_check manpath_add_lang_sys = 0 "$@";
  local p;
  local _mp;
  if is_empty "${_MAN_PATH}"; then
    eval "${return_ok}";
  fi;
  # twice test both sys and lang
  eval set -- "$(path_split "${_MAN_PATH}")";
  _mp='';
  for p in "$@"; do		# loop on man path directories
    _mp="$(_manpath_add_lang_sys_single "${_mp}" "$p")";
  done;
  eval set -- "$(path_split "${_mp}")";
  for p in "$@"; do		# loop on man path directories
    _mp="$(_manpath_add_lang_sys_single "${_mp}" "$p")";
  done;
  _MAN_PATH="$(path_chop "${_mp}")";
  eval "${return_ok}";
}


_manpath_add_lang_sys_single()
{
  # To the directory in $1 append existing sys/lang subdirectories
  # Function is necessary to split the OS list.
  #
  # globals: in: $_MAN_SYS, $_MAN_LANG, $_MAN_LANG2
  # argument: 2: `man_path' and `dir'
  # output: colon-separated path of the retrieved subdirectories
  #
  local d;
#  if test "$#" -ne 2; then
#    error "manpath_add_system_single() needs 2 arguments.";
#  fi;
  _res="$1";
  _parent="$2";
  eval set -- "$(list_from_split "${_MAN_SYS}" ',')";
  for d in "$@" "${_MAN_LANG}" "${_MAN_LANG2}"; do
    _dir="$(dirname_append "${_parent}" "$d")";
    if path_not_contains "${_res}" "${_dir}" && is_dir "${_dir}"; then
      _res="${_res}:${_dir}";
    fi;
  done;
  if path_not_contains "${_res}" "${_parent}"; then
    _res="${_res}:${_parent}";
  fi;
  path_chop "${_res}";
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
manpath_set_from_path()
{
  func_check manpath_set_from_path = 0 "$@";
  local _base;
  local _mandir;
  local _manpath;
  local d;
  local e;
  _manpath='';

  # get a basic man path from $PATH
  if is_not_empty "${PATH}"; then
    eval set -- "$(path_split "${PATH}")";
    for d in "$@"; do
      # delete the final `/bin' part
      _base="$(echo -n "$d" | sed -e '\|//*bin/*$|s|||')";
      for e in /share/man /man; do
        _mandir="${_base}$e";
        if test -d "${_mandir}" && test -r "${_mandir}"; then
        _manpath="${_manpath}:${_mandir}";
        fi;
      done;
    done;
  fi;

  # append some default directories
  for d in /usr/local/share/man /usr/local/man \
            /usr/share/man /usr/man \
            /usr/X11R6/man /usr/openwin/man \
            /opt/share/man /opt/man \
            /opt/gnome/man /opt/kde/man; do
    if path_not_contains "${_manpath}" "$d" && is_dir "$d"; then
      _manpath="${_manpath}:$d";
    fi;
  done;

  _MAN_PATH="${_manpath}";
  eval "${return_ok}";
} # manpath_set_from_path()


########################################################################
landmark '9: path_*()';
########################################################################

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
  local _res;

  # replace multiple colons by a single colon `:'
  # remove leading and trailing colons
  echo -n "$1" | sed -e 's/:::*/:/g' |
                 sed -e 's/^:*//' |
                 sed -e 's/:*$//';
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
path_clean()
{
  func_check path_clean = 1 "$@";
  local _arg;
  local _dir;
  local _res;
  local i;
  if test "$#" -ne 1; then
    error 'path_clean() needs 1 argument.';
  fi;
  _arg="$1";
  eval set -- "$(path_split "${_arg}")";
  _res="";
  for i in "$@"; do
    if is_not_empty "$i" \
       && path_not_contains "${_res}" "$i" \
       && is_dir "$i";
    then
      case "$i" in
        ?*/) _res="${_res}$(dirname_chop "$i")"; ;;
        *)  _res="${_res}:$i";
      esac;
    fi;
  done;
  if path_chop "${_res}"; then
    eval "${return_ok}";
  else
    eval "${return_badk}";
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
#-
# Test whether `dir' is not contained in colon separated `path'.
#
# Arguments : 2 arguments.
#
path_not_contains()
{
  func_check path_not_contains = 2 "$@";
  if path_contains "$1" "$2"; then
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
# Output:    the resulting list, process with `eval set --'
#
path_split()
{
  func_check path_split = 1 "$@";
  list_from_split "$1" ':';
  eval "${return_ok}";
}


########################################################################
# reset ()
#
# Reset the variables that can be affected by options to their default.
#
#
# Defined in section `Preset' after the rudimentary shell tests.


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
  if is_empty "$1"; then
    error 'register_file(): file name is empty';
  fi;
  if is_equal "$1" '-'; then
    to_tmp "${_TMP_STDIN}";
    register_title '-';
  else
    to_tmp "$1";
    register_title "$(base_name "$1")";
  fi;
  eval "${return_ok}";
}


########################################################################
# register_title (<filespec>)
#
# Create title element from <filespec> and append to $_REGISTERED_TITLE
#
# Globals: $_REGISTERED_TITLE (rw)
#
register_title()
{
  func_check register_title = 1 "$@";
  local _title;
  if is_empty "$1"; then
    eval "${return_ok}";
  fi;
  _title="$(base_name "$1")";	# remove directory part
  
  # remove extension `.gz'
  _title="$(echo -n "${_title}" | sed -e 's/\.gz$//')";
  # remove extension `.Z'
  _title="$(echo -n "${_title}" | sed -e 's/\.Z$//')";

  if is_empty "${_title}"; then
    eval "${return_ok}";
  fi;
  _REGISTERED_TITLE="${_REGISTERED_TITLE} ${_title}";
  eval "${return_ok}";
}


########################################################################
# save_stdin ()
#
# Store standard input to temporary file (with decompression).
#
if test "${_HAS_COMPRESSION}" = 'yes'; then
  save_stdin()
  {
    local _f;
    func_check save_stdin = 0 "$@";
     _f="${_TMP_DIR}"/INPUT;
    cat >"${_f}";
    catz "${_f}" >"${_TMP_STDIN}";
    rm -f "${_f}";
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
  func_check string_contains = 2 "$@";
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
  func_check string_not_contains = 2 "$@";
  if string_contains "$1" "$2"; then
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
  cat "${_TMP_CAT}";
}


########################################################################
# tmp_create (<suffix>?)
#
# create temporary file
#
# It's safe to use the shell process ID together with a suffix to
# have multiple temporary files.
#
# Output : name of created file
#
tmp_create()
{
  func_check tmp_create '<=' 1 "$@";
  local _tmp;
  _tmp="${_TMP_DIR}/$1";
  echo -n >"${_tmp}";
  echo -n "${_tmp}";		# output file name
  eval "${return_ok}";
}


########################################################################
# to_tmp (<filename>)
#
# print file (decompressed) to the temporary cat file
#
to_tmp()
{
  func_check to_tmp = 1 "$@";
  if is_file "$1"; then
    if is_yes "${_OPT_LOCATION}"; then
      echo2 "$1";
    fi;
    if is_yes "${_OPT_WHATIS}"; then
      what_is "$1" >>"${_TMP_CAT}";
    else
      catz "$1" >>"${_TMP_CAT}";
    fi;
  else
    error "to_tmp(): could not read file \`$1'.";
  fi;
  eval "${return_ok}";
}


########################################################################
# trap_clean ()
#
# disable trap on all exit codes ($_ALL_EXIT)
#
# Arguments: 0
# Globals:   $_ALL_EXIT
#
trap_clean()
{
  func_check trap_clean = 0 "$@";
  local i;
  for i in ${_ALL_EXIT}; do
    trap "" "$i" 2>/dev/null || true;
  done;
  eval "${return_ok}";
}


########################################################################
# trap_set (<functionname>)
#
# call function on all exit codes ($_ALL_EXIT)
#
# Arguments: 1 (name of a shell function)
# Globals:   $_ALL_EXIT
#
trap_set()
{
  func_check trap_set = 1 "$@";
  local i;
  for i in ${_ALL_EXIT}; do
    trap "$1" "$i" 2>/dev/null || true;
  done;
  eval "${return_ok}";
}


########################################################################
# usage ()
#
# print usage information to stderr
#
usage()
{
  func_check usage = 0 "$@";
  echo2;
  version;
  cat >&2 <<EOF
Copyright (C) 2001 Free Software Foundation, Inc.
This is free software licensed under the GNU General Public License.

EOF

  echo2 "Usage: ${_PROGRAM_NAME} [option]... [filespec]...";

  cat >&2 <<EOF

where "filespec" is one of
  "filename"       name of a readable file
  "-"              for standard input
  "man:name.n"     man page "name" in section "n"
  "man:name"       man page "name" in first section found
  "name.n"         man page "name" in section "n"
  "name"           man page "name" in first section found
and some more (see groffer(1) for details).

Display roff files, standard input, and/or Unix manual pages with
in a X window viewer or in a text pager.
"-" stands for including standard input.
"manpage" is the name of a man page, "x" its section.
All input is decompressed on-the-fly (by gzip).

-h --help        print this usage message.
-Q --source      output as roff source.
-T --device=name pass to groff using output device "name".
-v --version     print version information.

All other short options are interpreted as "groff" formatting
parameters and are transferred unmodified.  The following groff
options imply groff mode (groffer viewing disabled):

-X               display with "gxditview" using groff -X.
-V               display the groff execution pipe instead of formatting.
-Z --ditroff --intermediate-output
                 generate groff intermediate output without 
                 post-processing and viewing like groff -Z.

The most important long options are

--auto-modes=mode1,mode2,...
                 set sequence of automatically tried modes.
--bg             set background color (not for all modes).
--default        reset effects of option processing so far.
--display        set the X device when displaying in X.
--dpi=res        set resolution to "res" ("75" (default) or "100").
--dvi            display in a viewer for TeX device independent format.
--dvi-viewer     choose the viewer program for dvi mode.
--extension=ext  restrict man pages to section suffix.
--fg             set foreground color (not for all modes).
--geometry=geom  set the window size and position when displaying in X.
--groff          process like groff, disable viewing features.
--local-file     same as --no-man.
--locale=lang    preset the language for man pages.
--location       print file locations additionally to standard error.
--man            check file parameters first whether they are man pages.
--mode=auto|dvi|groff|pdf|ps|source|tty|www|x
                 choose display mode.
--no-location    disable former call to "--location".
--no-man         disable man-page facility.
--pager=program  preset the paging program for tty mode.
--pdf            display in a PDF viewer.
--pdf-viewer     choose the viewer program for pdf mode.
--ps             display in a Postscript viewer.
--ps-viewer      choose the viewer program for ps mode.
--shell          specify shell under which to run this program.
--systems=os1,os2,...
                 search man pages for different operating systems.
--title='text'   set the title of the viewer window (not in all modes.
--tty            force paging on text terminal even when in X.
--www            display in a web browser.
--www-viewer     choose the web browser for www mode.
--x              display in an X roff viewer.
--x-viewer       choose viewer program for x mode.
--xrm='resource' set X resouce.

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
  echo2 "${_PROGRAM_NAME} ${_PROGRAM_VERSION} of ${_LAST_UPDATE}";
  # also display groff's version, but not the called subprograms
  groff -v 2>&1 | sed -e '/^ *$/q' | sed -e '1s/^/is part of /' >&2;  
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
# what_is (<filename>)
#
# Interpret <filename> as a man page and display its `whatis'
# information as a fragment written in the groff language.
#
what_is()
{
  func_check what_is = 1 "$@";
  local _res;
  local _dot;
  if is_not_file "$1"; then
    error "what_is(): argument is not a readable file."
  fi;
  _dot='^\.['"${_SPACE}${_TAB}"']*';
  echo '.br';
  echo "$1: ";
    echo '.br';
  echo -n '  ';
  # grep the line containing `.TH' macro, if any
  _res="$(catz "$1" | sed -e '/'"${_dot}"'TH /p
d')";
  if is_not_empty "${_res}"; then	# traditional man style
    # get the text between the first and the second `.SH' macro, by
    # - delete up to first .SH;
    # - of this, print everything up to next .SH, and delete the rest;
    # - of this, delete the final .SH line;
    catz "$1" | sed -e '1,/'"${_dot}"'SH/d' \
              | sed -e '1,/'"${_dot}"'SH/p
d' \
              | sed -e '/'"${_dot}"'SH/d';
    eval "${return_ok}";
  fi;
  # grep the line containing `.Dd' macro, if any
  _res="$(catz "$1" | sed -e '/'"${_dot}"'Dd /p
d')";
  if is_not_empty "${_res}"; then	# BSD doc style
    # get the text between the first and the second `.Nd' macro, by
    # - delete up to first .Nd;
    # - of this, print everything up to next .Nd, and delete the rest;
    # - of this, delete the final .Nd line;
    catz "$1" | sed -e '1,/'"${_dot}"'Nd/d' \
              | sed -e '1,/'"${_dot}"'Nd/p
d' \
              | sed -e '/'"${_dot}"'Nd/d';
    eval "${return_ok}";
  fi;
  echo 'is not a man page.';
  eval "${return_bad}";
}


########################################################################
# where (<program>)
#
# Output path of a program if in $PATH.
#
# Arguments : >=1 (empty allowed)
#   more args are ignored, this allows to specify progs with arguments
# Return    : `0' if arg1 is a program in $PATH, `1' otherwise.
#
where()
{
  func_check where '>=' 1 "$@";
  local _file;
  local _arg;
  local p;
  _arg="$1";
  if is_empty "${_arg}"; then
    eval "${return_bad}";
  fi;
  case "${_arg}" in
    /*)
      if test -f "${_arg}" && test -x "${_arg}"; then
        eval "${return_ok}";
      else
        eval "${return_bad}";
      fi;
      ;;
  esac;
  eval set -- "$(path_split "${PATH}")";
  for p in "$@"; do
    case "$p" in
      */) _file=$p${_arg}; ;;
      *)  _file=$p/${_arg}; ;;
    esac;
    if test -f "${_file}" && test -x "${_file}"; then
      echo -n "${_file}";
      eval "${return_ok}";
    fi;
  done;
  eval "${return_bad}";
}


########################################################################
#                              main
########################################################################

# The main area contains the following parts:
# - main_init(): initialize temporary files and set exit trap
# - parse $MANOPT
# - main_parse_args(): argument parsing
# - determine display mode
# - process filespec arguments
# - setup X resources
# - do the displaying

# These parts are implemented as functions, being defined below in the
# sequence they are called in the main() function.


#######################################################################
# main_init ()
#
# set exit trap and create temporary files
#
# Globals: $_TMP_CAT, $_TMP_STDIN
#
landmark '13: main_init()';
main_init()
{
  func_check main_init = 0 "$@";
  # call clean_up() on any signal
  trap_set clean_up;

  for f in ${_CONFFILES}; do
    if is_file "$f"; then
      . "$f";
    fi;
  done;

  # determine temporary directory
  umask 000;
  _TMP_DIR='';
  for d in "${GROFF_TMPDIR}" "${TMPDIR}" "${TMP}" "${TEMP}" \
           "${TEMPDIR}" "${HOME}"'/tmp' '/tmp' "${HOME}" '.';
  do
    if test "$d" != ""; then
      if test -d "$d" && test -r "$d" && test -w "$d"; then
        _TMP_DIR="${d}/${_PROGRAM_NAME}${_PROCESS_ID}";
        if test -d "${_TMP_DIR}"; then
	  rm -f "${_TMP_DIR}"/*;
          break;
        else
          mkdir "${_TMP_DIR}";
          if test ! -d "${_TMP_DIR}"; then
	    _TMP_DIR='';
	    continue;
  	  fi;
          break;
	fi;
      fi;
      if test ! -w "${_TMP_DIR}"; then
	_TMP_DIR='';
	continue;
      fi;
    fi;
  done;
  unset d;
  if test "${_TMP_DIR}" = ''; then
    error "Couldn't create a directory for storing temporary files.";
  fi;

  _TMP_CAT="$(tmp_create groffer_cat)";
  _TMP_STDIN="$(tmp_create groffer_input)";
  eval "${return_ok}";
} # main_init()


########################################################################
# main_parse_MANOPT ()
#
# Parse $MANOPT to retrieve man options, but only if it is a non-empty
# string; found man arguments can be overwritten by the command line.
#
# Globals:
#   in: $MANOPT, $_OPTS_MAN_*
#   out: $_MANOPT_*
#   in/out: $GROFFER_OPT
#
landmark '14: main_parse_MANOPT()';
main_parse_MANOPT()
{
  func_check main_parse_MANOPT = 0 "$@";
  local _opt;
  local _list;
  _list='';
  if test "${MANOPT}" != ''; then
    MANOPT="$(echo -n "${MANOPT}" | \
      sed -e 's/^'"${_SPACE}${_SPACE}"'*//')";
  fi;
  if test "${MANOPT}" = ''; then
    eval "${return_ok}";
  fi;
  # add arguments in $MANOPT by mapping them to groffer options
  eval set -- "$(list_from_cmdline \
    "${_OPTS_MAN_SHORT_NA}" "${_OPTS_MAN_SHORT_ARG}" \
    "${_OPTS_MAN_LONG_NA}" "${_OPTS_MAN_LONG_ARG}" \
    "${MANOPT}")";
  until test "$#" -le 0 || is_equal "$1" '--'; do
    _opt="$1";
    shift;
    case "${_opt}" in
      -7|--ascii)
        _list="$(list_append "${_list}" '--ascii')";
        ;;
      -a|--all)
        _list="$(list_append "${_list}" '--all')";
        ;;
      -c|--catman)
        do_nothing;
        shift;
        ;;
      -d|--debug)
        _list="$(list_append "${_list}" '--debug')";
        ;;
      -D|--default)
        # undo all man options so far
        _list='';
        ;;
      -e|--extension)
        _list="$(list_append "${_list}" '--extension')";
        shift;
        ;;
      -f|--whatis)
        _list="$(list_append "${_list}" '--whatis')";
        shift;
        ;;
      -h|--help)
        do_nothing;
        shift;
        ;;
      -k|--apropos)
        _list="$(list_append "${_list}" '--apropos')";
        shift;
        ;;
      -l|--local-file)
        _list="$(list_append "${_list}" '--local-file')";
        ;;
      -L|--locale)
        _list="$(list_append "${_list}" '--locale' "$1")";
        shift;
        ;;
      -m|--systems)
        _list="$(list_append "${_list}" '--systems' "$1")";
        shift;
        ;;
      -M|--manpath)
        _list="$(list_append "${_list}" '--manpath' "$1")";
        shift;
        ;;
      -p|--preprocessor)
        do_nothing;
        shift;
        ;;
      -P|--pager)
        _list="$(list_append "${_list}" '--pager' "$1")";
        shift;
        ;;
      -r|--prompt)
        do_nothing;
        shift;
        ;;
      -S|--sections)
        _list="$(list_append "${_list}" '--sections' "$1")";
        shift;
        ;;
      -t|--troff)
        do_nothing;
        shift;
        ;;
      -T|--device)
        _list="$(list_append "${_list}" '-T' "$1")";
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
        _list="$(list_append "${_list}" '--location')";
        ;;
      -Z|--ditroff)
        _list="$(list_append "${_list}" '-Z' "$1")";
        shift;
        ;;
      # ignore all other options
    esac;
  done;
  GROFFER_OPT="$(list_from_lists "${_list}" "${GROFFER_OPT}")";
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
landmark '15: main_parse_args()';
main_parse_args()
{
  func_check main_parse_args '>=' 0 "$@";
  local _arg;
  local _code;
  local _dpi;
  local _longopt;
  local _mode;
  local _opt;
  local _optchar;
  local _optarg;
  local _opts;
  local _string;

  eval set -- "${GROFFER_OPT}" '"$@"';

  eval set -- "$(list_from_cmdline \
   "$_OPTS_CMDLINE_SHORT_NA" "$_OPTS_CMDLINE_SHORT_ARG" \
   "$_OPTS_CMDLINE_LONG_NA" "$_OPTS_CMDLINE_LONG_ARG" \
   "$@")";

# By the call of `eval', unnecessary quoting was removed.  So the
# positional shell parameters ($1, $2, ...) are now guaranteed to
# represent an option or an argument to the previous option, if any;
# then a `--' argument for separating options and
# parameters; followed by the filespec parameters if any.

# Note, the existence of arguments to options has already been checked.
# So a check for `$#' or `--' should not be done for arguments.

  until test "$#" -le 0 || is_equal "$1" '--'; do
    _opt="$1";			# $_opt is fed into the option handler
    shift;
    case "${_opt}" in
      -h|--help)
        usage;
        leave;
        ;;
      -Q|--source)		# output source code (`Quellcode').
        _OPT_MODE='source';
        ;;
      -T|--device|--troff-device)
				# device; arg
        _OPT_DEVICE="$1";
        shift;
        ;;
      -v|--version)
        version;
        leave;
        ;;
      -V)
        _OPT_V='yes';
        ;;
      -X)
        _OPT_X='yes';
        ;;
      -Z|--ditroff|--intermediate-output)
				# groff intermediate output
        _OPT_Z='yes';
        ;;
      -?)
        # delete leading `-'
        _optchar="$(echo -n "${_opt}" | sed -e 's/^.//')";
        if list_has "${_OPTS_GROFF_SHORT_NA}" "${_optchar}";
        then
          _ADDOPTS_GROFF="$(list_append "${_ADDOPTS_GROFF}" \
                                        "${_opt}")";
        elif list_has "${_OPTS_GROFF_SHORT_ARG}" "${_optchar}";
        then
          _ADDOPTS_GROFF="$(list_append "${_ADDOPTS_GROFF}" \
                                        "${_opt}" "$1")";
           shift;
        else
          error "Unknown option : \`$1'";
        fi;
        ;;
      --all)
          _OPT_ALL="yes";
          ;;
      --ascii)
        _ADDOPTS_GROFF="$(list_append "${_ADDOPTS_GROFF}" \
                                      '-mtty-char')";
        ;;
      --apropos)
        _OPT_APROPOS="yes";
        ;;
      --auto)			# the default automatic mode
        _mode='';
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
      --default)		# reset variables to default
        reset;
        ;;
      --default-modes)		# sequence of modes in auto mode; arg
        _OPT_DEFAULT_MODES="$1";
        shift;
        ;;
      --debug)		# sequence of modes in auto mode; arg
        _OPT_DEBUG='yes';
        ;;
      --display)		# set X display, arg
        _OPT_DISPLAY="$1";
        shift;
        ;;
      --dvi)
        _OPT_MODE='dvi';
        ;;
      --dvi-viewer)		# viewer program for dvi mode; arg
        _OPT_VIEWER_DVI="$1";
        shift;
        ;;
      --extension)		# the extension for man pages, arg
        _OPT_EXTENSION="$1";
        shift;
        ;;
      --fg|--foreground)	# foreground color for viewers, arg;
        _OPT_FG="$1";
        shift;
        ;;
      --fn|--font)		# set font for viewers, arg;
        _OPT_FN="$1";
        shift;
        ;;
      --geometry)		# window geometry for viewers, arg;
        _OPT_GEOMETRY="$1";
        shift;
        ;;
      --groff)
        _OPT_MODE='groff';
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
      --location|--where)	# print file locations to stderr
        _OPT_LOCATION='yes';
        ;;
      --man)			# force all file params to be man pages
        _MAN_ENABLE='yes';
        _MAN_FORCE='yes';
        ;;
      --manpath)		# specify search path for man pages, arg
        # arg is colon-separated list of directories
        _OPT_MANPATH="$1";
        shift;
        ;;
      --mode)			# display mode
        _arg="$1";
        shift;
        case "${_arg}" in
          auto|'')		# the default automatic mode
	    _mode='';
            ;;
          groff)		# pass input to plain groff
            _mode='groff';
            ;;
          dvi)			# display with xdvi viewer
            _mode='dvi';
            ;;
          pdf)			# display with PDF viewer
            _mode='pdf';
            ;;
          ps)			# display with Postscript viewer
            _mode='ps';
            ;;
          X|x)			# output on X roff viewer
            _mode='x';
            ;;
          tty)			# output on terminal
            _mode='tty';
            ;;
          Q|source)		# display source code
            _mode="source";
            ;;
	  *)
            error "unknown mode ${_arg}";
            ;;
        esac;
        _OPT_MODE="${_mode}";
        ;;
      --no-location)		# disable former call to `--location'
        _OPT_LOCATION='yes';
        ;;
      --no-man)			# disable search for man pages
        # the same as --local-file
        _MAN_FORCE="no";
        _MAN_ENABLE="no";
        ;;
      --pager)			# set paging program for tty mode, arg
        _OPT_PAGER="$1";
        shift;
        ;;
      --pdf)
        _OPT_MODE='pdf';
        ;;
      --pdf-viewer)		# viewer program for ps mode; arg
        _OPT_VIEWER_PDF="$1";
        shift;
        ;;
      --ps)
        _OPT_MODE='ps';
        ;;
      --ps-viewer)		# viewer program for ps mode; arg
        _OPT_VIEWER_PS="$1";
        shift;
        ;;
      --resolution)		# set resolution for X devices, arg
        _arg="$1";
        shift;
        case "${_arg}" in
          75|75dpi)
            _dpi=75;
            ;;
          100|100dpi)
            _dpi=100;
            ;;
          *)
            error "only resoutions of 75 or 100 dpi are supported";
            ;;
        esac;
        _OPT_RESOLUTION="${_dpi}";
        ;;
      --rv)
        _OPT_RV='yes';
        ;;
      --sections)		# specify sections for man pages, arg
        # arg is colon-separated list of section names
        _OPT_SECTIONS="$1";
        shift;
        ;;
      --shell)
        shift;
        ;;
      --systems)		# man pages for different OS's, arg
        # argument is a comma-separated list
        _OPT_SYSTEMS="$1";
        shift;
        ;;
      --title)			# title for X viewers; arg
        _OPT_TITLE="$1";
        shift;
        ;;
      --tty)
        _OPT_MODE="tty";
        ;;
      --tty-device)		# device for tty mode; arg
        _OPT_TTY_DEVICE="$1";
        shift;
        ;;
      --whatis)
        _OPT_WHATIS='yes';
        ;;
      --www)			# display with web browser
        _OPT_MODE='www';
        ;;
      --www-viewer)		# viewer program for www mode; arg
        _OPT_VIEWER_WWW="$1";
        shift;
        ;;
      --x)
        _OPT_MODE='x';
        ;;
      --xrm)			# pass X resource string, arg;
        _OPT_XRM="$(list_append "${_OPT_XRM}" "$1")";
        shift;
        ;;
      --x-viewer)		# viewer program for x mode; arg
        _OPT_VIEWER_X="$1";
        shift;
        ;;
      *)
        error 'error on argument parsing : '"\`$*'";
        ;;
    esac;
  done;
  shift;			# remove `--' argument

  if test "${_DEBUG}" != 'yes'; then
    if test "${_OPT_DEBUG}" = 'yes'; then
      _DEBUG='yes';
    fi;
  fi;

  # Remaining arguments are file names (filespecs).
  # Save them to list $_FILEARGS
  if test "$#" -eq 0; then         # use "-" for standard input
    set -- '-';
  fi;
  _FILEARGS="$(list_from_args "$@")";
  if list_has "$_FILEARGS" '-'; then
    save_stdin;
  fi;
  # $_FILEARGS must be retrieved with `eval set -- "$_FILEARGS"'
  eval "${return_ok}";
} # main_parse_args()


########################################################################
# main_set_mode ()
#
# Determine the display mode.
#
# Globals:
#   in:  $DISPLAY, $_OPT_MODE, $_OPT_DEVICE
#   out: $_DISPLAY_MODE
#

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
landmark '16: main_set_mode()';
main_set_mode()
{
  func_check main_set_mode = 0 "$@";
  local m;
  local _modes;
  local _viewer;
  local _viewers;

  # handle apropos
  if is_yes "${_OPT_APROPOS}"; then
    apropos "$@";
    _code="$?";
    clean_up;
    exit "${_code}";
  fi;

  # set display
  if is_not_empty "${_OPT_DISPLAY}"; then
    DISPLAY="${_OPT_DISPLAY}";
  fi;

  if is_yes "${_OPT_V}"; then
    _DISPLAY_MODE='groff';
    _ADDOPTS_GROFF="$(list_append "${_ADDOPTS_GROFF}" '-V')";
  fi;
  if is_yes "${_OPT_X}"; then
    _DISPLAY_MODE='groff';
    _ADDOPTS_GROFF="$(list_append "${_ADDOPTS_GROFF}" '-X')";
  fi;
  if is_yes "${_OPT_Z}"; then
    _DISPLAY_MODE='groff';
    _ADDOPTS_GROFF="$(list_append "${_ADDOPTS_GROFF}" '-Z')";
  fi;
  if is_equal "${_OPT_MODE}" 'groff'; then
    _DISPLAY_MODE='groff';
  fi;
  if is_equal "${_DISPLAY_MODE}" 'groff'; then
    eval "${return_ok}";
  fi;

  if is_equal "${_OPT_MODE}" 'source'; then
    _DISPLAY_MODE='source';
    eval "${return_ok}";
  fi;

  case "${_OPT_MODE}" in
    '')				# automatic mode
      case "${_OPT_DEVICE}" in
        X*)
          if is_empty "${DISPLAY}"; then
            error "no X display found for device ${_OPT_DEVICE}";
          fi;
          _DISPLAY_MODE='x';
          eval "${return_ok}";
          ;;
        ascii|cp1047|latin1|utf8)
          _DISPLAY_MODE='tty';
          eval "${return_ok}";
          ;;
      esac;
      if is_empty "${DISPLAY}"; then
        _DISPLAY_MODE='tty';
        eval "${return_ok}";
      fi;

      if is_empty "${_OPT_DEFAULT_MODES}"; then
        _modes="${_DEFAULT_MODES}";
      else
        _modes="${_OPT_DEFAULT_MODES}";
      fi;
      ;;
    tty)
      _DISPLAY_MODE='tty';
      eval "${return_ok}";
      ;;
    *)				# display mode was given
      if is_empty "${DISPLAY}"; then
        error "you must be in X Window for ${_OPT_MODE} mode.";
      fi;
      _modes="${_OPT_MODE}";
      ;;
  esac;

  # only viewer modes are left
  eval set -- "$(list_from_split "${_modes}" ',')";
  while test "$#" -gt 0; do
    m="$1";
    shift;
    case "$m" in
      tty)
        _DISPLAY_MODE='tty';
        eval "${return_ok}";
        ;;
      x)
        if is_not_empty "${_OPT_VIEWER_X}"; then
          _viewers="${_OPT_VIEWER_X}";
        else
          _viewers="${_VIEWER_X}";
        fi;
        _viewer="$(_get_first_prog "${_viewers}")";
        if test "$?" -ne 0; then
          continue;
        fi;
        _DISPLAY_PROG="${_viewer}";
        _DISPLAY_MODE='x';
        eval "${return_ok}";
        ;;
      dvi)
        if is_not_empty "${_OPT_VIEWER_DVI}"; then
          _viewers="${_OPT_VIEWER_DVI}";
        else
          _viewers="${_VIEWER_DVI}";
        fi;
        _viewer="$(_get_first_prog "${_viewers}")";
        if test "$?" -ne 0; then
          continue;
        fi;
        _DISPLAY_PROG="${_viewer}";
        _DISPLAY_MODE="dvi";
        eval "${return_ok}";
        ;;
      pdf)
        if is_not_empty "${_OPT_VIEWER_PDF}"; then
          _viewers="${_OPT_VIEWER_PDF}";
        else
          _viewers="${_VIEWER_PDF}";
        fi;
        _viewer="$(_get_first_prog "${_viewers}")";
        if test "$?" -ne 0; then
          continue;
        fi;
        _DISPLAY_PROG="${_viewer}";
        _DISPLAY_MODE="pdf";
        eval "${return_ok}";
        ;;
      ps)
        if is_not_empty "${_OPT_VIEWER_PS}"; then
          _viewers="${_OPT_VIEWER_PS}";
        else
          _viewers="${_VIEWER_PS}";
        fi;
        _viewer="$(_get_first_prog "${_viewers}")";
        if test "$?" -ne 0; then
          continue;
        fi;
        _DISPLAY_PROG="${_viewer}";
        _DISPLAY_MODE="ps";
        eval "${return_ok}";
        ;;
      www)
        if is_not_empty "${_OPT_VIEWER_WWW}"; then
          _viewers="${_OPT_VIEWER_WWW}";
        else
          _viewers="${_VIEWER_WWW}";
        fi;
        _viewer="$(_get_first_prog "${_viewers}")";
        if test "$?" -ne 0; then
          continue;
        fi;
        _DISPLAY_PROG="${_viewer}";
        _DISPLAY_MODE="www";
        eval "${return_ok}";
        ;;
    esac;
  done;
  error "no suitable display mode found.";
}

_get_first_prog()
{
  local i;
  if test "$#" -eq 0; then
    error "_get_first_prog() needs 1 argument.";
  fi;
  if is_empty "$1"; then
    return "${_BAD}";
  fi;
  eval set -- "$(list_from_split "$1" ',')";
  for i in "$@"; do
    if is_empty "$i"; then
      continue;
    fi;
    if is_prog "$(get_first_essential $i)"; then
      echo -n "$i";
      return "${_GOOD}";
    fi;
  done;
  return "${_BAD}";
} # main_set_mode()


#######################################################################
# main_do_fileargs ()
#
# Process filespec arguments in $_FILEARGS.
#
# Globals:
#   in: $_FILEARGS (process with `eval set -- "$_FILEARGS"')
#
landmark '17: main_do_fileargs()';
main_do_fileargs()
{
  func_check main_do_fileargs = 0 "$@";
  local _exitcode;
  local _filespec;
  local _name;
  _exitcode="${_BAD}";
  eval set -- "${_FILEARGS}";
  unset _FILEARGS;
  # temporary storage of all input to $_TMP_CAT
  while test "$#" -ge 2; do
    # test for `s name' arguments, with `s' a 1-char standard section
    _filespec="$1";
    shift;
    case "${_filespec}" in
      '')
        continue;
        ;;
      '-')
        if register_file '-'; then
          _exitcode="${_GOOD}";
        fi;
        continue;
        ;;
      ?)
        if list_has_not "${_MAN_AUTO_SEC}" "${_filespec}"; then
          if do_filearg "${_filespec}"; then
            _exitcode="${_GOOD}";
          fi;
          continue;
        fi;
        _name="$1";
        case "${_name}" in
          */*|man:*|*\(*\)|*."${_filespec}")
            if do_filearg "${_filespec}"; then
              _exitcode="${_GOOD}";
            fi;
            continue;
            ;;
        esac;
        if do_filearg "man:${_name}(${_filespec})"; then
          _exitcode="${_GOOD}";
          shift;
          continue;
        else
          if do_filearg "${_filespec}"; then
            _exitcode="${_GOOD}";
          fi;
          continue;
        fi;
        ;;
      *)
        if do_filearg "${_filespec}"; then
          _exitcode="${_GOOD}";
        fi;
        continue;
        ;;
    esac;
  done;				# end of `s name' test
  while test "$#" -gt 0; do
    _filespec="$1";
    shift;
    if do_filearg "${_filespec}"; then
      _exitcode="${_GOOD}";
    fi;
  done;
  rm -f "${_TMP_STDIN}";
  if is_equal "${_exitcode}" "${_BAD}"; then
    eval "${return_bad}";
  fi;
  eval "${return_ok}";
} # main_do_fileargs()


########################################################################
# main_set_resources ()
#
#  Determine options for setting X resources with $_DISPLAY_PROG.
#
landmark '18: main_set_resources()';
main_set_resources()
{
  func_check main_set_resources = 0 "$@";
  local _prog;			# viewer program
  local _rl;			# resource list
  _rl='';
  if is_empty "${_DISPLAY_PROG}"; then
    eval "${return_ok}";
  fi;
  set -- ${_DISPLAY_PROG};
  _prog="$(base_name "$1")";
  if is_not_empty "${_OPT_BD}"; then
    case "${_prog}" in
      ghostview|gv|gxditview|xditview|xdvi)
        _rl="$(list_append "$_rl" '-bd' "${_OPT_BD}")";
        ;;
    esac;
  fi;
  if is_not_empty "${_OPT_BG}"; then
    case "${_prog}" in
      ghostview|gv|gxditview|xditview|xdvi)
        _rl="$(list_append "$_rl" '-bg' "${_OPT_BG}")";
        ;;
      xpdf)
        _rl="$(list_append "$_rl" '-papercolor' "${_OPT_BG}")";
        ;;
    esac;
  fi;
  if is_not_empty "${_OPT_BW}"; then
    case "${_prog}" in
      ghostview|gv|gxditview|xditview|xdvi)
        _rl="$(list_append "$_rl" '-bw' "${_OPT_BW}")";
        ;;
    esac;
  fi;
  if is_not_empty "${_OPT_FG}"; then
    case "${_prog}" in
      ghostview|gv|gxditview|xditview|xdvi)
        _rl="$(list_append "$_rl" '-fg' "${_OPT_FG}")";
        ;;
    esac;
  fi;
  if is_not_empty "${_OPT_FN}"; then
    case "${_prog}" in
      ghostview|gv|gxditview|xditview|xdvi)
        _rl="$(list_append "$_rl" '-fn' "${_OPT_FN}")";
        ;;
    esac;
  fi;
  if is_not_empty "${_OPT_GEOMETRY}"; then
    case "${_prog}" in
      ghostview|gv|gxditview|xditview|xdvi|xpdf)
        _rl="$(list_append "$_rl" '-geometry' "${_OPT_GEOMETRY}")";
        ;;
    esac;
  fi;
  if is_empty "${_OPT_RESOLUTION}"; then
    case "${_prog}" in
      gxditview|xditview)
        _rl="$(list_append "$_rl" \
               '-resolution' "${_DEFAULT_RESOLUTION}")";
        ;;
      xpdf)
        case "${_DEFAULT_RESOLUTION}" in
          75)
            _rl="$(list_append "$_rl" '-z' '2')";
            ;;
          100)
            _rl="$(list_append "$_rl" '-z' '3')";
            ;;
        esac;
        ;;
    esac;
  else
    case "${_prog}" in
      ghostview|gv|gxditview|xditview|xdvi)
        _rl="$(list_append "$_rl" '-resolution' "${_OPT_RESOLUTION}")";
        ;;
      xpdf)
        case "${_OPT_RESOLUTION}" in
          75)
            _rl="$(list_append "$_rl" '-z' '2')";
            ;;
          100)
            _rl="$(list_append "$_rl" '-z' '3')";
            ;;
        esac;
        ;;
    esac;
  fi;
  if is_not_empty "${_OPT_RV}"; then
    case "${_prog}" in
      ghostview|gv|gxditview|xditview|xdvi)
        _rl="$(list_append "$_rl" '-rv')";
        ;;
    esac;
  fi;
  if is_not_empty "${_OPT_XRM}"; then
    case "${_prog}" in
      ghostview|gv|gxditview|xditview|xdvi|xpdf)
        eval set -- "{$_OPT_XRM}";
        for i in "$@"; do
          _rl="$(list_append "$_rl" '-xrm' "$i")";
        done;
        ;;
    esac;
  fi;
  _title="$(get_first_essential \
                "${_OPT_TITLE}" "${_REGISTERED_TITLE}")";
  if is_not_empty "${_title}"; then
    case "${_prog}" in
      gxditview|xditview)
        _rl="$(list_append "$_rl" '-title' "${_title}")";
        ;;
    esac;
  fi;
  _DISPLAY_ARGS="${_rl}"; 
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
#       $_REGISTERED_TITLE, $_TMP_CAT,
#       $_OPT_PAGER $PAGER $_MANOPT_PAGER
#
landmark '19: main_display()';
main_display()
{
  func_check main_display = 0 "$@";
  local p;
  local _addopts;
  local _device;
  local _groggy;
  local _modefile;
  local _options;
  local _pager;
  local _title;
  export _addopts;
  export _groggy;
  export _modefile;

  # Some display programs have trouble with empty input.  
  # This is avoided by feeding a space character in this case.
  # Test on non-empty file by tracking a line with at least 1 character.
  if is_empty "$(tmp_cat | sed -e '/./q')"; then
    echo ' ' > "${_TMP_CAT}";
  fi;

  case "${_DISPLAY_MODE}" in
    groff)
      _ADDOPTS_GROFF="${_ADDOPTS_GROFF} ${_ADDOPTS_POST}";
      if is_not_empty "${_OPT_DEVICE}"; then
        _ADDOPTS_GROFF="${_ADDOPTS_GROFF} -T${_OPT_DEVICE}";
      fi;
      _groggy="$(tmp_cat | eval grog "${_options}")";
      trap_clean;
      # start a new shell program to get another process ID.
      sh -c '
        set -e;
        _PROCESS_ID="$$";
        _modefile="${_TMP_DIR}/${_PROGRAM_NAME}${_PROCESS_ID}";
        rm -f "${_modefile}";
        mv "${_TMP_CAT}" "${_modefile}";
        rm -f "${_TMP_CAT}";
        cat "${_modefile}" | \
        (
          clean_up()
          {
            rm -f "${_modefile}";
          }
          trap clean_up 0 2>/dev/null || true;
          eval "${_groggy}" "${_ADDOPTS_GROFF}";
        ) &'
      ;;
    tty)
      case "${_OPT_DEVICE}" in
        '')
          _device="$(get_first_essential \
                     "${_OPT_TTY_DEVICE}" "${_DEFAULT_TTY_DEVICE}")";
          ;;
        ascii|cp1047|latin1|utf8)
          _device="${_OPT_DEVICE}";
          ;;
        *)
          warning \
            "wrong device for ${_DISPLAY_MODE} mode: ${_OPT_DEVICE}";
          ;;
      esac;
      _addopts="${_ADDOPTS_GROFF} ${_ADDOPTS_POST}";
      _groggy="$(tmp_cat | grog -T${_device})";
      _pager='';
      for p in "${_OPT_PAGER}" "${PAGER}" "${_MANOPT_PAGER}" \
               'less' 'more' 'cat'; do
        if is_prog "$p"; then
          _pager="$p";
          break;
        fi;
      done;
      if is_empty "${_pager}"; then
        error 'no pager program found for tty mode';
      fi;
      tmp_cat | eval "${_groggy}" "${_addopts}" | \
                eval "${_pager}";
      clean_up;
      ;;

    #### viewer modes

    dvi)
      case "${_OPT_DEVICE}" in
        ''|dvi) do_nothing; ;;
        *)
          warning \
            "wrong device for ${_DISPLAY_MODE} mode: ${_OPT_DEVICE}";
          ;;
      esac;
      _groggy="$(tmp_cat | grog -Tdvi)";
      _do_display;
      ;;
    pdf)
      case "${_OPT_DEVICE}" in
        ''|ps)
          do_nothing;
          ;;
        *)
          warning \
            "wrong device for ${_DISPLAY_MODE} mode: ${_OPT_DEVICE}";
          ;;
      esac;
      _groggy="$(tmp_cat | grog -Tps)";
      trap_clean;
      # start a new shell program to get another process ID.
      sh -c '
        set -e;
        _PROCESS_ID="$$";
        _psfile="${_TMP_DIR}/${_PROGRAM_NAME}${_PROCESS_ID}";
        _modefile="${_TMP_DIR}/${_PROGRAM_NAME}${_PROCESS_ID}.pdf";
        rm -f "${_psfile}";
        rm -f "${_modefile}";
        cat "${_TMP_CAT}" | \
        eval "${_groggy}" "${_ADDOPTS_GROFF}" > "${_psfile}";
        gs -q -dNOPAUSE -dBATCH -sDEVICE=pdfwrite \
           -sOutputFile="${_modefile}" -c save pop -f "${_psfile}";
        rm -f "${_psfile}";
        rm -f "${_TMP_CAT}";
        (
          clean_up()
          {
            rm -f "${_modefile}";
          }
          trap clean_up 0 2>/dev/null || true;
          eval "${_DISPLAY_PROG}" ${_DISPLAY_ARGS} "${_modefile}";
        ) &'
      ;;
    ps)
      case "${_OPT_DEVICE}" in
        ''|ps)
          do_nothing;
          ;;
        *)
          warning \
            "wrong device for ${_DISPLAY_MODE} mode: ${_OPT_DEVICE}";
          ;;
      esac;
      _groggy="$(tmp_cat | grog -Tps)";
      _do_display;
      ;;
    source)
      tmp_cat;
      clean_up;
      ;;
    www)
      case "${_OPT_DEVICE}" in
        ''|html) do_nothing; ;;
        *)
          warning \
            "wrong device for ${_DISPLAY_MODE} mode: ${_OPT_DEVICE}";
          ;;
      esac;
      _groggy="$(tmp_cat | grog -Thtml)";
      _do_display;
      ;;
    x)
      case "${_OPT_DEVICE}" in
        '')
          _groggy="$(tmp_cat | grog -Z)";
          ;;
        X*|ps)
          _groggy="$(tmp_cat | grog -T"${_OPT_DEVICE}" -Z)";
          ;;
        *)
          warning \
            "wrong device for ${_DISPLAY_MODE} mode: ${_OPT_DEVICE}";
          _groggy="$(tmp_cat | grog -Z)";
          ;;
      esac;
      _do_display;
      ;;
    *)
      error "unknown mode \`${_DISPLAY_MODE}'";
      ;;
  esac;
  eval "${return_ok}";
} # main_display()

_do_display()
{
  trap_clean;
  # start a new shell program for another process ID and better
  # cleaning-up of the temporary files.
  sh -c '
    set -e;
    _PROCESS_ID="$$";
    _modefile="${_TMP_DIR}/${_PROGRAM_NAME}${_PROCESS_ID}";
    rm -f "${_modefile}";
    cat "${_TMP_CAT}" | \
      eval "${_groggy}" "${_ADDOPTS_GROFF}" > "${_modefile}";
    rm -f "${_TMP_CAT}";
    (
      clean_up()
      {
        if test -d "${_TMP_DIR}"; then
          rm -f "${_TMP_DIR}"/*;
          rmdir "${_TMP_DIR}";
        fi;
      }
      trap clean_up 0 2>/dev/null || true;
      eval "${_DISPLAY_PROG}" ${_DISPLAY_ARGS} "${_modefile}";
    ) &'
}


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
  main_init;
  main_parse_MANOPT;
  main_parse_args "$@";
  main_set_mode;
  main_do_fileargs;
  main_set_resources;
  main_display;
  eval "${return_ok}";
}

landmark '20: end of function definitions';

########################################################################

main "$@";
