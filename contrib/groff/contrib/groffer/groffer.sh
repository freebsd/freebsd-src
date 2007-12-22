#! /bin/sh

# groffer - display groff files

# Source file position: <groff-source>/contrib/groffer/groffer.sh

# Copyright (C) 2001,2002,2003,2004,2005
# Free Software Foundation, Inc.
# Written by Bernd Warken

# This file is part of `groffer', which is part of `groff' version
# @VERSION@.  See $_GROFF_VERSION.

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

_PROGRAM_VERSION='0.9.22';
_LAST_UPDATE='22 August 2005';

export _PROGRAM_VERSION;
export _LAST_UPDATE;

export GROFFER_OPT;		# option environment for groffer

export _CONF_FILE_ETC;		# configuration file in /etc
export _CONF_FILE_HOME;		# configuration file in $HOME
export _CONF_FILES;		# configuration files
_CONF_FILE_ETC='/etc/groff/groffer.conf';
_CONF_FILE_HOME="${HOME}/.groff/groffer.conf";
_CONF_FILES="${_CONF_FILE_ETC} ${_CONF_FILE_HOME}";

# characters

export _AT;
export _SP;
export _SQ;
export _TAB;

_AT='@';
_SP=' ';
_SQ="'";
_TAB='	';

export _ERROR;
_ERROR='7';			# for syntax errors; no `-1' in `ash'

# @...@ constructs

export _GROFF_VERSION
_GROFF_VERSION='@VERSION@';
if test _@VERSION@_ = _${_AT}VERSION${_AT}_
then
  _GROFF_VERSION='1.19.2';
fi;

export _AT_BINDIR_AT;
export _AT_G_AT;
export _AT_LIBDIR_AT;
export _GROFFER_LIBDIR;
if test _@BINDIR@_ = _${_AT}BINDIR${_AT}_
then
  # script before `make'
  _AT_BINDIR_AT='.';
  _AT_G_AT='';
  _AT_LIBDIR_AT='';
  _GROFFER_LIBDIR='.';
else
  _AT_BINDIR_AT='@BINDIR@';
  _AT_G_AT='@g@';
  _AT_LIBDIR_AT='@libdir@';
  _GROFFER_LIBDIR="${_AT_LIBDIR_AT}"'/groff/groffer';
fi;

export _GROFFER_SH;		# file name of this shell script
case "$0" in
*groffer*)
  _GROFFER_SH="$0";
  # was: _GROFFER_SH="${_AT_BINDIR_AT}/groffer";
  ;;
*)
  echo 'The groffer script should be started directly.' >&2
  exit 1;
  ;;
esac;

export _GROFFER2_SH;		# file name of the script that follows up
_GROFFER2_SH="${_GROFFER_LIBDIR}"/groffer2.sh;

export _NULL_DEV;
if test -c /dev/null
then
  _NULL_DEV="/dev/null";
else
  _NULL_DEV="NUL";
fi;


# Test of the `$()' construct.
if test _"$(echo "$(echo 'test')")"_ \
     != _test_
then
  echo 'The "$()" construct did not work.' >&2;
  exit "${_ERROR}";
fi;

# Test of sed program
if test _"$(echo red | sed -e 's/r/s/')"_ != _sed_
then
  echo 'The sed program did not work.' >&2;
  exit "${_ERROR}";
fi;


########################### configuration

# read and transform the configuration files, execute the arising commands
for f in "${_CONF_FILE_HOME}" "${_CONF_FILE_ETC}"
do
  if test -f "$f"
  then
    o="";			# $o means groffer option
    # use "" quotes because of ksh and posh
    eval "$(cat "$f" | sed -n -e '
# Ignore comments
/^['"${_SP}${_TAB}"']*#/d
# Delete leading and final space
s/^['"${_SP}${_TAB}"']*//
s/['"${_SP}${_TAB}"']*$//
# Print all shell commands
/^[^-]/p
# Replace empty arguments
s/^\(-[^ ]*\)=$/o="${o} \1 '"${_SQ}${_SQ}"'"/p
# Replace division between option and argument by single space
s/[='"${_SP}${_TAB}"']['"${_SP}${_TAB}"']*/'"${_SP}"'/
# Handle lines without spaces
s/^\(-[^'"${_SP}"']*\)$/o="${o} \1"/p
# Print options that have their argument encircled with single quotes
/^-[^ ]* '"${_SQ}"'.*'"${_SQ}"'$/s/^.*$/o="${o} &"/p
# Replace encircled double quotes by single quotes and print the result
s/^\(-[^ ]*\) "\(.*\)"$/o="${o} \1 '"${_SQ}"'\2'"${_SQ}"'"/p
# Encircle the remaining arguments with single quotes
s/^\(-[^ ]*\) \(.*\)$/o="${o} \1 '"${_SQ}"'\2'"${_SQ}"'"/p
')"
    if test _"${o}"_ != __
    then
      if test _"{GROFFER_OPT}"_ = __
      then
        GROFFER_OPT="${o}";
      else
        GROFFER_OPT="${o} ${GROFFER_OPT}";
      fi;
    fi;
  fi;
done;

# integrate $GROFFER_OPT into the command line; it isn't needed any more
if test _"${GROFFER_OPT}"_ != __
then
  eval set x "${GROFFER_OPT}" '"$@"';
  shift;
  GROFFER_OPT='';
fi;


########################### Determine the shell

export _SHELL;

# use "``" instead of "$()" for using the case ")" construct
# do not use "" quotes because of ksh
_SHELL=`
  # $x means list.
  # $s means shell.
  # The command line arguments are taken over.
  # Shifting herein does not have an effect outside.
  export x;  
  case " $*" in
  *\ --sh*)			# abbreviation for --shell
    x='';
    s='';
    # determine all --shell arguments, store them in $x in reverse order
    while test $# != 0
    do
      case "$1" in
      --shell|--sh|--she|--shel)
        if test "$#" -ge 2
        then
          s="$2";
          shift;
        fi;
        ;;
      --shell=*|--sh=*|--she=*|--shel=*)
        # delete up to first "=" character
        s="$(echo x"$1" | sed -e 's/^x[^=]*=//')";
        ;;
      *)
        shift;
        continue;
      esac;
      if test _"${x}"_ = __
      then
        x="'${s}'";
      else
        x="'${s}' ${x}";
      fi;
      shift;
    done;

    # from all possible shells in $x determine the first being a shell
    # or being empty
    s="$(
      # "" quotes because of posh
      eval set x "${x}";
      shift;
      if test $# != 0
      then
        for i
        do
          if test _"$i"_ = __
          then
            # use the empty argument as the default shell
            echo empty;
            break;
          else
            # test $i on being a shell program;
            # use this kind of quoting for posh
            if test _"$(eval "$i -c 'echo ok'" 2>${_NULL_DEV})"_ = _ok_ >&2
            then
              # shell found
              cat <<EOF
${i}
EOF
              break;
            else
              # if not being a shell go on searching
              continue;
            fi;
          fi;
        done;
      fi;
    )";
    if test _"${s}"_ != __
    then
      cat <<EOF
${s}
EOF
    fi;
    ;;
  esac;
`

########################### test fast shells for automatic run

if test _"${_SHELL}"_ = __
then
  for s in ksh ash dash pdksh zsh posh
  do
    if test _"$(eval "$s -c 'echo ok'" 2>${_NULL_DEV})"_ = _ok_ >&2
    then
      _SHELL="$s";
      break;
    fi;
  done;
fi;


########################### start groffer2.sh

if test _"${_SHELL}"_ = _empty_
then
  _SHELL='';
fi;

if test _"${_SHELL}"_ = __
then
  # no shell found, so start groffer2.sh normally
  eval exec "'${_GROFFER2_SH}'" '"$@"';
  exit;
else
  # start groffer2.sh with the found $_SHELL
  # do not quote $_SHELL to allow arguments
  eval exec "${_SHELL} '${_GROFFER2_SH}'" '"$@"';
  exit;
fi;
