#!/bin/sh
# Drop in the SUBMITTER id into a site's installed send-pr script.
# Copyright (C) 1993 Free Software Foundation, Inc.
# Contributed by Brendan Kehoe (brendan@cygnus.com), based on a
# version written by Heinz G. Seidl (hgs@ide.com).
#
# This file is part of GNU GNATS.
#
# GNU GNATS is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or (at your option)
# any later version.
#
# GNU GNATS is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with GNU GNATS; see the file COPYING.  If not, write to
# the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.

COMMAND=`echo $0 | sed -e 's,.*/,,g'`
USAGE="Usage: $COMMAND [--install-dir=prefix] [--help] [--version] submitter-id"

VERSION=3.2

BINDIR=@BINDIR@

SUBMITTER=
TEMP=/tmp/sp$$

if [ $# -eq 0 ]; then
  echo "$USAGE"
  exit 1
fi

while [ $# -gt 0 ]; do
  case "$1" in
    -install-dir=*|--install-dir=*|--install-di=*|--install-d=*|--install-=*|--install=*|--instal=*|--insta=*|--inst=*|--ins=*|--in=*|--i=*)
    I=`echo "$1" | sed 's/-*i[a-z\-]*=//'`
    BINDIR=$I/bin ;;
    --version) echo $COMMAND version $VERSION ; exit 1 ;;
    -*) echo "$USAGE" ; exit 1 ;;
    *) SUBMITTER=$1 ;;
  esac
  shift
done

path=`echo $0 | sed -e "s;${COMMAND};;"`

[ -z "$path" ] && path=.

if [ -f $BINDIR/send-pr ]; then
  SPPATH=$BINDIR/send-pr
elif [ -f $path/send-pr ]; then
  SPPATH=$path/send-pr
else
  echo "$COMMAND: cannot find \`$BINDIR/send-pr' or \`$path/send-pr'" >&2
  exit 1
fi

trap 'rm -f $TEMP ; exit 0' 0
trap 'echo "$COM: Aborting ..."; rm -f $TEMP ; exit 1' 1 2 3 13 15

sed -e "s/^SUBMITTER=.*/SUBMITTER=${SUBMITTER}/" $SPPATH > $TEMP

if grep $SUBMITTER $TEMP > /dev/null; then
  cp $SPPATH $SPPATH.orig &&
  rm -f $SPPATH &&
  cp $TEMP $SPPATH &&
  chmod a+rx $SPPATH &&
  rm -f $TEMP $SPPATH.orig ||
  { echo "$COMMAND: unable to replace send-pr" >&2 ; exit 1; }  
else
  echo "$COMMAND: something went wrong when sed-ing the submitter into send-pr" >&2
  exit 1
fi

echo "$COMMAND: \`$SUBMITTER' is now the default submitter ID for send-pr"

exit 0
