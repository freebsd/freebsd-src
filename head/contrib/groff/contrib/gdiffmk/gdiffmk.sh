#! /bin/sh
# Copyright (C) 2004, 2005 Free Software Foundation, Inc.
# Written by Mike Bianchi <MBianchi@Foveal.com <mailto:MBianchi@Foveal.com>>

# This file is part of the gdiffmk utility, which is part of groff.

# groff is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or (at your option)
# any later version.

# groff is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
# or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
# License for more details.

# You should have received a copy of the GNU General Public License
# along with groff; see the files COPYING and LICENSE in the top
# directory of the groff source.  If not, write to the Free Software
# Foundation, 51 Franklin St - Fifth Floor, Boston, MA 02110-1301, USA.
# This file is part of GNU gdiffmk.


cmd=$( basename $0 )

function Usage {
	if test "$#" -gt 0
	then
		echo >&2 "${cmd}:  $@"
	fi
	echo >&2 "\

Usage:  ${cmd} [ OPTIONS ] FILE1 FILE2 [ OUTPUT ]
Place difference marks into the new version of a groff/nroff/troff document.
FILE1 and FILE2 are compared, using \`diff', and FILE2 is output with
groff \`.mc' requests added to indicate how it is different from FILE1.

  FILE1   Previous version of the groff file.  \`-' means standard input.
  FILE2   Current version of the groff file.   \`-' means standard input.
          Either FILE1 or FILE2 can be standard input, but not both.
  OUTPUT  Copy of FILE2 with \`.mc' commands added.
          \`-' means standard output (the default).

OPTIONS:
  -a ADDMARK     Mark for added groff source lines.    Default: \`+'.
  -c CHANGEMARK  Mark for changed groff source lines.  Default: \`|'.
  -d DELETEMARK  Mark for deleted groff source lines.  Default: \`*'.

  -D             Show the deleted portions from changed and deleted text.
                  Default delimiting marks:  \`[[' .... \`]]'.
  -B             By default, the deleted texts marked by the \`-D' option end
                  with an added troff \`.br' command.  This option prevents
                  the added \`.br'.
  -M MARK1 MARK2 Change the delimiting marks for the \`-D' option.

  -x DIFFCMD     Use a different diff(1) command;
                  one that accepts the \`-Dname' option, such as GNU diff.
  --version      Print version information on the standard output and exit.
  --help         Print this message on the standard error.
"
	exit 255
}


function Exit {
	exitcode=$1
	shift
	for arg
	do
		echo >&2 "${cmd}:  $1"
		shift
	done
	exit ${exitcode}
}

#	Usage:  FileRead  exit_code  filename
#
#	Check for existence and readability of given file name.
#	If not found or not readable, print message and exit with EXIT_CODE.
function FileRead {
	case "$2" in
	-)
		return
		;;
	esac

	if test ! -e "$2"
	then
		Exit $1 "File \`$2' not found."
	fi
	if test ! -r "$2"
	then
		Exit $1 "File \`$2' not readable."
	fi
}


#	Usage:  FileCreate  exit_code  filename
#
#	Create the given filename if it doesn't exist.
#	If unable to create or write, print message and exit with EXIT_CODE.
function FileCreate {
	case "$2" in
	-)
		return
		;;
	esac

	if ! touch "$2" 2>/dev/null
	then
		if test ! -e "$2"
		then
			Exit $1 "File \`$2' not created; " \
			  "Cannot write directory \`$( dirname "$2" )'."
		fi
		Exit $1 "File \`$2' not writeable."
	fi
}

function WouldClobber {
	case "$2" in
	-)
		return
		;;
	esac

	if test "$1" -ef "$3"
	then
		Exit 3 \
		  "The $2 and OUTPUT arguments both point to the same file," \
		  "\`$1', and it would be overwritten."
	fi
}

ADDMARK='+'
CHANGEMARK='|'
DELETEMARK='*'
MARK1='[['
MARK2=']]'

function RequiresArgument {
	#	Process flags that take either concatenated or
	#	separated values.
	case "$1" in
	-??*)
		expr "$1" : '-.\(.*\)'
		return 1
		;;
	esac

	if test "$#" -lt 2
	then
		Exit 255 "Option \`$1' requires a value."
	fi

	echo "$2"
	return 0
}

badoption=
DIFFCMD=diff
D_option=
br=.br
for OPTION
do
	case "${OPTION}" in
	-a*)
		ADDMARK=$( RequiresArgument "${OPTION}" $2 )		&&
			shift
		;;
	-c*)
		CHANGEMARK=$( RequiresArgument "${OPTION}" $2 )		&&
			shift
		;;
	-d*)
		DELETEMARK=$( RequiresArgument "${OPTION}" $2 )		&&
			shift
		;;
	-D )
		D_option=D_option
		;;
	-M* )
		MARK1=$( RequiresArgument "${OPTION}" $2 )		&&
			shift
		if [ $# -lt 2 ]
		then
			Usage "Option \`-M' is missing the MARK2 value."
		fi
		MARK2=$2
		shift
		;;
	-B )
		br=.
		;;
	-x* )
		DIFFCMD=$( RequiresArgument "${OPTION}" $2 )		&&
			shift
		;;
	--version)
		echo "GNU ${cmd} (groff) version @VERSION@"
		exit 0
		;;
	--help)
		Usage
		;;
	--)
		#	What follows  --  are file arguments
		shift
		break
		;;
	-)
		break
		;;
	-*)
		badoption="${cmd}:  invalid option \`$1'"
		;;
	*)
		break
		;;
	esac
	shift
done

${DIFFCMD} -Dx /dev/null /dev/null >/dev/null 2>&1  ||
	Usage "The \`${DIFFCMD}' program does not accept"	\
		"the required \`-Dname' option.
Use GNU diff instead.  See the \`-x DIFFCMD' option."

if test -n "${badoption}"
then
	Usage "${badoption}"
fi

if test "$#" -lt 2  -o  "$#" -gt 3
then
	Usage "Incorrect number of arguments."
fi

if test "1$1" = 1-  -a  "2$2" = 2-
then
	Usage "Both FILE1 and FILE2 are \`-'."
fi

FILE1=$1
FILE2=$2

FileRead 1 "${FILE1}"
FileRead 2 "${FILE2}"

if test "$#" = 3
then
	case "$3" in
	-)
		#	output goes to standard output
		;;
	*)
		#	output goes to a file
		WouldClobber "${FILE1}" FILE1 "$3"
		WouldClobber "${FILE2}" FILE2 "$3"

		FileCreate 3 "$3"
		exec >$3
		;;
	esac
fi

#	To make a very unlikely label even more unlikely ...
label=__diffmk_$$__

sed_script='
		/^#ifdef '"${label}"'/,/^#endif \/\* '"${label}"'/ {
		  /^#ifdef '"${label}"'/          s/.*/.mc '"${ADDMARK}"'/
		  /^#endif \/\* '"${label}"'/     s/.*/.mc/
		  p
		  d
		}
		/^#ifndef '"${label}"'/,/^#endif \/\* [!not ]*'"${label}"'/ {
		  /^#else \/\* '"${label}"'/,/^#endif \/\* '"${label}"'/ {
		    /^#else \/\* '"${label}"'/    s/.*/.mc '"${CHANGEMARK}"'/
		    /^#endif \/\* '"${label}"'/   s/.*/.mc/
		    p
		    d
		  }
		  /^#endif \/\* \(not\|!\) '"${label}"'/ {
		   s/.*/.mc '"${DELETEMARK}"'/p
		   a\
.mc
		  }
		  d
		}
		p
	'

if [ ${D_option} ]
then
	sed_script='
		/^#ifdef '"${label}"'/,/^#endif \/\* '"${label}"'/ {
		  /^#ifdef '"${label}"'/          s/.*/.mc '"${ADDMARK}"'/
		  /^#endif \/\* '"${label}"'/     s/.*/.mc/
		  p
		  d
		}
		/^#ifndef '"${label}"'/,/^#endif \/\* [!not ]*'"${label}"'/ {
		  /^#ifndef '"${label}"'/ {
		   i\
'"${MARK1}"'
		   d
		  }
		  /^#else \/\* '"${label}"'/ ! {
		   /^#endif \/\* [!not ]*'"${label}"'/ ! {
		    p
		    d
		   }
		  }
		  /^#else \/\* '"${label}"'/,/^#endif \/\* '"${label}"'/ {
		    /^#else \/\* '"${label}"'/ {
		     i\
'"${MARK2}"'\
'"${br}"'
		     s/.*/.mc '"${CHANGEMARK}"'/
		     a\
.mc '"${CHANGEMARK}"'
		     d
		    }
		    /^#endif \/\* '"${label}"'/   s/.*/.mc/
		    p
		    d
		  }
		  /^#endif \/\* \(not\|!\) '"${label}"'/ {
		   i\
'"${MARK2}"'\
'"${br}"'
		   s/.*/.mc '"${DELETEMARK}"'/p
		   a\
.mc
		  }
		  d
		}
		p
	'
fi

diff -D"${label}" -- ${FILE1} ${FILE2}  |
	sed -n "${sed_script}"

# EOF
