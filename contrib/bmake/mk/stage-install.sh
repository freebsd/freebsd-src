#!/bin/sh

# NAME:
#	stage-install.sh - wrapper around install
#
# SYNOPSIS:
#	stage-install.sh [variable="value"] "args" "dest"
#
# DESCRIPTION:
#	This script is a wrapper around the normal install(1).
#	Its role is to add '.dirdep' files to the destination.
#	The variables we might use are:
#
#	INSTALL
#		Path to actual install(1), default is
#		$REAL_INSTALL
#
#	OBJDIR
#		Path to the dir where '.dirdep' was generated,
#		default is '.'
#
#	_DIRDEP
#		Path to actual '.dirdep' file, default is
#		$OBJDIR/.dirdep
#
#	The "args" and "dest" are passed as is to install(1), and if a
#	'.dirdep' file exists it will be linked or copied to each
#	"file".dirdep placed in "dest" or "dest".dirdep if it happed
#	to be a file rather than a directory.
#
#	Before we run install(1), we check if "dest" needs to be a
#	directory (more than one file in "args") and create it
#	if necessary.
#
# SEE ALSO:
#	meta.stage.mk
#

# RCSid:
#	$Id: stage-install.sh,v 1.11 2024/02/17 17:26:57 sjg Exp $
#
#	SPDX-License-Identifier: BSD-2-Clause
#
#	@(#) Copyright (c) 2013-2020, Simon J. Gerraty
#
#	This file is provided in the hope that it will
#	be of use.  There is absolutely NO WARRANTY.
#	Permission to copy, redistribute or otherwise
#	use this file is hereby granted provided that
#	the above copyright notice and this notice are
#	left intact.
#
#	Please send copies of changes and bug-fixes to:
#	sjg@crufty.net
#

INSTALL=${REAL_INSTALL:-install}
OBJDIR=.

while :
do
    case "$1" in
    *=*) eval "$1"; shift;;
    *) break;;
    esac
done

# get last entry from "$@" without side effects
last_entry() {
    while [ $# -gt 8 ]
    do
        shift 8
    done
    eval last=\$$#
    echo $last
}

# mkdir $dest if needed (more than one file)
mkdir_if_needed() {
    (
        lf=
        while [ $# -gt 8 ]
        do
            shift 4
        done
        for f in "$@"
        do
            [ -f $f ] || continue
            [ $f = $dest ] && continue
            if [ -n "$lf" ]; then
                # dest must be a directory
                mkdir -p $dest
                break
            fi
            lf=$f
        done
    )
}

args="$@"
dest=`last_entry "$@"`
case " $args " in
*" -d "*) ;;
*) [ -e $dest ] || mkdir_if_needed "$@";;
esac

# if .dirdep doesn't exist, just run install and be done
_DIRDEP=${_DIRDEP:-$OBJDIR/.dirdep}
[ -s $_DIRDEP ] && EXEC= || EXEC=exec
$EXEC $INSTALL "$@" || exit 1

# from meta.stage.mk
LnCp() {
    rm -f $2 2> /dev/null
    ln $1 $2 2> /dev/null || cp -p $1 $2
}

StageDirdep() {
  t=$1
  if [ -s $t.dirdep ]; then
      cmp -s $_DIRDEP $t.dirdep && return
      case "${STAGE_CONFLICT:-error}" in
      [Ee]*) STAGE_CONFLICT=ERROR action=exit;;
      *) STAGE_CONFLICT=WARNING action=: ;;
      esac
      echo "$STAGE_CONFLICT: $t installed by `cat $t.dirdep` not `cat $_DIRDEP`" >&2
      $action 1
  fi
  LnCp $_DIRDEP $t.dirdep || exit 1
}

if [ -f $dest ]; then
    # a file, there can be only one .dirdep needed
    StageDirdep $dest
elif [ -d $dest ]; then
    for f in $args
    do
        test -f $f || continue
        StageDirdep $dest/${f##*/}
    done
fi
