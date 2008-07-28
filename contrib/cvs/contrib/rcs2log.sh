#! /bin/sh

# Copyright (C) 1995-2005 The Free Software Foundation, Inc.

# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or (at your option)
# any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# RCS to ChangeLog generator

# Generate a change log prefix from RCS files (perhaps in the CVS repository)
# and the ChangeLog (if any).
# Output the new prefix to standard output.
# You can edit this prefix by hand, and then prepend it to ChangeLog.

# Ignore log entries that start with `#'.
# Clump together log entries that start with `{topic} ',
# where `topic' contains neither white space nor `}'.

Help='The default FILEs are the files registered under the working directory.
Options:

  -c CHANGELOG  Output a change log prefix to CHANGELOG (default ChangeLog).
  -h HOSTNAME  Use HOSTNAME in change log entries (default current host).
  -i INDENT  Indent change log lines by INDENT spaces (default 8).
  -l LENGTH  Try to limit log lines to LENGTH characters (default 79).
  -L FILE  Use rlog-format FILE for source of logs.
  -R  If no FILEs are given and RCS is used, recurse through working directory.
  -r OPTION  Pass OPTION to subsidiary log command.
  -t TABWIDTH  Tab stops are every TABWIDTH characters (default 8).
  -u "LOGIN<tab>FULLNAME<tab>MAILADDR"  Assume LOGIN has FULLNAME and MAILADDR.
  -v  Append RCS revision to file names in log lines.
  --help  Output help.
  --version  Output version number.

Report bugs to <bug-gnu-emacs@gnu.org>.'

Id='$Id: rcs2log,v 1.48 2001/09/05 23:07:46 eggert Exp $'

# Copyright 1992, 1993, 1994, 1995, 1996, 1997, 1998, 2001, 2003
#  Free Software Foundation, Inc.

# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or (at your option)
# any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; see the file COPYING.  If not, write to the
# Free Software Foundation, Inc., 59 Temple Place - Suite 330,
# Boston, MA 02111-1307, USA.

Copyright='Copyright 1992-2003 Free Software Foundation, Inc.
This program comes with NO WARRANTY, to the extent permitted by law.
You may redistribute copies of this program
under the terms of the GNU General Public License.
For more information about these matters, see the files named COPYING.
Author: Paul Eggert <eggert@twinsun.com>'

# functions
@MKTEMP_SH_FUNCTION@

# Use the traditional C locale.
LANG=C
LANGUAGE=C
LC_ALL=C
LC_COLLATE=C
LC_CTYPE=C
LC_MESSAGES=C
LC_NUMERIC=C
LC_TIME=C
export LANG LANGUAGE LC_ALL LC_COLLATE LC_CTYPE LC_MESSAGES LC_NUMERIC LC_TIME

# These variables each contain a single ASCII character.
# Unfortunately, there's no portable way of writing these characters
# in older Unix implementations, other than putting them directly into
# this text file.
SOH='' # SOH, octal code 001
tab='	'
nl='
'

# Parse options.

# defaults
: ${MKTEMP="@MKTEMP@"}
: ${AWK=awk}
: ${TMPDIR=/tmp}

changelog=ChangeLog # change log file name
datearg= # rlog date option
hostname= # name of local host (if empty, will deduce it later)
indent=8 # indent of log line
length=79 # suggested max width of log line
logins= # login names for people we know fullnames and mailaddrs of
loginFullnameMailaddrs= # login<tab>fullname<tab>mailaddr triplets
logTZ= # time zone for log dates (if empty, use local time)
recursive= # t if we want recursive rlog
revision= # t if we want revision numbers
rlog_options= # options to pass to rlog
rlogfile= # log file to read from
tabwidth=8 # width of horizontal tab

while :
do
	case $1 in
	-c)	changelog=${2?}; shift;;
	-i)	indent=${2?}; shift;;
	-h)	hostname=${2?}; shift;;
	-l)	length=${2?}; shift;;
	-L)	rlogfile=${2?}; shift;;
	-[nu])	# -n is obsolescent; it is replaced by -u.
		case $1 in
		-n)	case ${2?}${3?}${4?} in
			*"$tab"* | *"$nl"*)
				echo >&2 "$0: -n '$2' '$3' '$4': tabs, newlines not allowed"
				exit 1;;
			esac
			login=$2
			lfm=$2$tab$3$tab$4
			shift; shift; shift;;
		-u)
			# If $2 is not tab-separated, use colon for separator.
			case ${2?} in
			*"$nl"*)
				echo >&2 "$0: -u '$2': newlines not allowed"
				exit 1;;
			*"$tab"*)
				t=$tab;;
			*)
				t=':';;
			esac
			case $2 in
			*"$t"*"$t"*"$t"*)
				echo >&2 "$0: -u '$2': too many fields"
				exit 1;;
			*"$t"*"$t"*)
				uf="[^$t]*$t" # An unselected field, followed by a separator.
				sf="\\([^$t]*\\)" # The selected field.
				login=`expr "X$2" : "X$sf"`
				lfm="$login$tab"`
					expr "X$2" : "$uf$sf"
				  `"$tab"`
					expr "X$2" : "$uf$uf$sf"
				`;;
			*)
				echo >&2 "$0: -u '$2': not enough fields"
				exit 1;;
			esac
			shift;;
		esac
		case $logins in
		'') logins=$login;;
		?*) logins=$logins$nl$login;;
		esac
		case $loginFullnameMailaddrs in
		'') loginFullnameMailaddrs=$lfm;;
		?*) loginFullnameMailaddrs=$loginFullnameMailaddrs$nl$lfm;;
		esac;;
	-r)
		case $rlog_options in
		'') rlog_options=${2?};;
		?*) rlog_options=$rlog_options$nl${2?};;
		esac
		shift;;
	-R)	recursive=t;;
	-t)	tabwidth=${2?}; shift;;
	-v)	revision=t;;
	--version)
		set $Id
		rcs2logVersion=$3
		echo >&2 "rcs2log (GNU Emacs) $rcs2logVersion$nl$Copyright"
		exit 0;;
	-*)	echo >&2 "Usage: $0 [OPTION]... [FILE ...]$nl$Help"
		case $1 in
		--help) exit 0;;
		*) exit 1;;
		esac;;
	*)	break;;
	esac
	shift
done

month_data='
	m[0]="Jan"; m[1]="Feb"; m[2]="Mar"
	m[3]="Apr"; m[4]="May"; m[5]="Jun"
	m[6]="Jul"; m[7]="Aug"; m[8]="Sep"
	m[9]="Oct"; m[10]="Nov"; m[11]="Dec"
'

logdir=`$MKTEMP -d $TMPDIR/rcs2log.XXXXXX`
test -n "$logdir" || exit
llogout=$logdir/l
trap exit 1 2 13 15
trap "rm -fr $logdir 2>/dev/null" 0

# If no rlog-format log file is given, generate one into $rlogfile.
case $rlogfile in
'')
	rlogfile=$logdir/r

	# If no rlog options are given,
	# log the revisions checked in since the first ChangeLog entry.
	# Since ChangeLog is only by date, some of these revisions may be duplicates of
	# what's already in ChangeLog; it's the user's responsibility to remove them.
	case $rlog_options in
	'')
		if test -s "$changelog"
		then
			e='
				/^[0-9]+-[0-9][0-9]-[0-9][0-9]/{
					# ISO 8601 date
					print $1
					exit
				}
				/^... ... [ 0-9][0-9] [ 0-9][0-9]:[0-9][0-9]:[0-9][0-9] [0-9]+ /{
					# old-fashioned date and time (Emacs 19.31 and earlier)
					'"$month_data"'
					year = $5
					for (i=0; i<=11; i++) if (m[i] == $2) break
					dd = $3
					printf "%d-%02d-%02d\n", year, i+1, dd
					exit
				}
			'
			d=`$AWK "$e" <"$changelog"` || exit
			case $d in
			?*) datearg="-d>$d";;
			esac
		fi;;
	esac

	# Use TZ specified by ChangeLog local variable, if any.
	if test -s "$changelog"
	then
		extractTZ='
			/^.*change-log-time-zone-rule['"$tab"' ]*:['"$tab"' ]*"\([^"]*\)".*/{
				s//\1/; p; q
			}
			/^.*change-log-time-zone-rule['"$tab"' ]*:['"$tab"' ]*t.*/{
				s//UTC0/; p; q
			}
		'
		logTZ=`tail "$changelog" | sed -n "$extractTZ"`
		case $logTZ in
		?*) TZ=$logTZ; export TZ;;
		esac
	fi

	# If CVS is in use, examine its repository, not the normal RCS files.
	if test ! -f CVS/Repository
	then
		rlog=rlog
		repository=
	else
		rlog='cvs -q log'
		repository=`sed 1q <CVS/Repository` || exit
		test ! -f CVS/Root || CVSROOT=`cat <CVS/Root` || exit
		case $CVSROOT in
		*:/*:/*)
			echo >&2 "$0: $CVSROOT: CVSROOT has multiple ':/'s"
			exit 1;;
		*:/*)
			# remote repository
			pository=`expr "X$repository" : '.*:\(/.*\)'`;;
		*)
			# local repository
			case $repository in
			/*) ;;
			*) repository=${CVSROOT?}/$repository;;
			esac
			if test ! -d "$repository"
			then
				echo >&2 "$0: $repository: bad repository (see CVS/Repository)"
				exit 1
			fi
			pository=$repository;;
		esac

		# Ensure that $pository ends in exactly one slash.
		while :
		do
			case $pository in
			*//) pository=`expr "X$pository" : 'X\(.*\)/'`;;
			*/) break;;
			*) pository=$pository/; break;;
			esac
		done

	fi

	# Use $rlog's -zLT option, if $rlog supports it.
	case `$rlog -zLT 2>&1` in
	*' option'*) ;;
	*)
		case $rlog_options in
		'') rlog_options=-zLT;;
		?*) rlog_options=-zLT$nl$rlog_options;;
		esac;;
	esac

	# With no arguments, examine all files under the RCS directory.
	case $# in
	0)
		case $repository in
		'')
			oldIFS=$IFS
			IFS=$nl
			case $recursive in
			t)
				RCSdirs=`find . -name RCS -type d -print`
				filesFromRCSfiles='s|,v$||; s|/RCS/|/|; s|^\./||'
				files=`
					{
						case $RCSdirs in
						?*) find $RCSdirs \
								-type f \
								! -name '*_' \
								! -name ',*,' \
								! -name '.*_' \
								! -name .rcsfreeze.log \
								! -name .rcsfreeze.ver \
								-print;;
						esac
						find . -name '*,v' -print
					} |
					sort -u |
					sed "$filesFromRCSfiles"
				`;;
			*)
				files=
				for file in RCS/.* RCS/* .*,v *,v
				do
					case $file in
					RCS/. | RCS/.. | RCS/,*, | RCS/*_) continue;;
					RCS/.rcsfreeze.log | RCS/.rcsfreeze.ver) continue;;
					RCS/.\* | RCS/\* | .\*,v | \*,v) test -f "$file" || continue;;
					RCS/*,v | RCS/.*,v) ;;
					RCS/* | RCS/.*) test -f "$file" || continue;;
					esac
					case $files in
					'') files=$file;;
					?*) files=$files$nl$file;;
					esac
				done
				case $files in
				'') exit 0;;
				esac;;
			esac
			set x $files
			shift
			IFS=$oldIFS;;
		esac;;
	esac

	case $datearg in
	?*) $rlog $rlog_options "$datearg" ${1+"$@"} >$rlogfile;;
	'') $rlog $rlog_options ${1+"$@"} >$rlogfile;;
	esac || exit;;
esac


# Get the full name of each author the logs mention, and set initialize_fullname
# to awk code that initializes the `fullname' awk associative array.
# Warning: foreign authors (i.e. not known in the passwd file) are mishandled;
# you have to fix the resulting output by hand.

initialize_fullname=
initialize_mailaddr=

case $loginFullnameMailaddrs in
?*)
	case $loginFullnameMailaddrs in
	*\"* | *\\*)
		sed 's/["\\]/\\&/g' >$llogout <<EOF || exit
$loginFullnameMailaddrs
EOF
		loginFullnameMailaddrs=`cat $llogout`;;
	esac

	oldIFS=$IFS
	IFS=$nl
	for loginFullnameMailaddr in $loginFullnameMailaddrs
	do
		IFS=$tab
		set x $loginFullnameMailaddr
		login=$2
		fullname=$3
		mailaddr=$4
		initialize_fullname="$initialize_fullname
			fullname[\"$login\"] = \"$fullname\""
		initialize_mailaddr="$initialize_mailaddr
			mailaddr[\"$login\"] = \"$mailaddr\""
	done
	IFS=$oldIFS;;
esac

case $logins in
?*)
	sort -u -o $llogout <<EOF
$logins
EOF
	;;
'')
	: ;;
esac >$llogout || exit

output_authors='/^date: / {
	if ($2 ~ /^[0-9]*[-\/][0-9][0-9][-\/][0-9][0-9]$/ && $3 ~ /^[0-9][0-9]:[0-9][0-9]:[0-9][0-9][-+0-9:]*;$/ && $4 == "author:" && $5 ~ /^[^;]*;$/) {
		print substr($5, 1, length($5)-1)
	}
}'
authors=`
	$AWK "$output_authors" <"$rlogfile" | sort -u | comm -23 - $llogout
`
case $authors in
?*)
	cat >$llogout <<EOF || exit
$authors
EOF
	initialize_author_script='s/["\\]/\\&/g; s/.*/author[\"&\"] = 1/'
	initialize_author=`sed -e "$initialize_author_script" <$llogout`
	awkscript='
		BEGIN {
			alphabet = "abcdefghijklmnopqrstuvwxyz"
			ALPHABET = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
			'"$initialize_author"'
		}
		{
			if (author[$1]) {
				fullname = $5
				if (fullname ~ /[0-9]+-[^(]*\([0-9]+\)$/) {
					# Remove the junk from fullnames like "0000-Admin(0000)".
					fullname = substr(fullname, index(fullname, "-") + 1)
					fullname = substr(fullname, 1, index(fullname, "(") - 1)
				}
				if (fullname ~ /,[^ ]/) {
					# Some sites put comma-separated junk after the fullname.
					# Remove it, but leave "Bill Gates, Jr" alone.
					fullname = substr(fullname, 1, index(fullname, ",") - 1)
				}
				abbr = index(fullname, "&")
				if (abbr) {
					a = substr($1, 1, 1)
					A = a
					i = index(alphabet, a)
					if (i) A = substr(ALPHABET, i, 1)
					fullname = substr(fullname, 1, abbr-1) A substr($1, 2) substr(fullname, abbr+1)
				}

				# Quote quotes and backslashes properly in full names.
				# Do not use gsub; traditional awk lacks it.
				quoted = ""
				rest = fullname
				for (;;) {
					p = index(rest, "\\")
					q = index(rest, "\"")
					if (p) {
						if (q && q<p) p = q
					} else {
						if (!q) break
						p = q
					}
					quoted = quoted substr(rest, 1, p-1) "\\" substr(rest, p, 1)
					rest = substr(rest, p+1)
				}

				printf "fullname[\"%s\"] = \"%s%s\"\n", $1, quoted, rest
				author[$1] = 0
			}
		}
	'

	initialize_fullname=`
		{
			(getent passwd $authors) ||
			(
				cat /etc/passwd
				for author in $authors
				do NIS_PATH= nismatch $author passwd.org_dir
				done
				ypmatch $authors passwd
			)
		} 2>/dev/null |
		$AWK -F: "$awkscript"
	`$initialize_fullname;;
esac


# Function to print a single log line.
# We don't use awk functions, to stay compatible with old awk versions.
# `Log' is the log message.
# `files' contains the affected files.
printlogline='{

	# Following the GNU coding standards, rewrite
	#	* file: (function): comment
	# to
	#	* file (function): comment
	if (Log ~ /^\([^)]*\): /) {
		i = index(Log, ")")
		filefunc = substr(Log, 1, i)
		while ((j = index(filefunc, "\n"))) {
			files = files " " substr(filefunc, 1, j-1)
			filefunc = substr(filefunc, j+1)
		}
		files = files " " filefunc
		Log = substr(Log, i+3)
	}

	# If "label: comment" is too long, break the line after the ":".
	sep = " "
	i = index(Log, "\n")
	if ('"$length"' <= '"$indent"' + 1 + length(files) + i) sep = "\n" indent_string

	# Print the label.
	printf "%s*%s:", indent_string, files

	# Print each line of the log.
	while (i) {
		logline = substr(Log, 1, i-1)
		if (logline ~ /[^'"$tab"' ]/) {
			printf "%s%s\n", sep, logline
		} else {
			print ""
		}
		sep = indent_string
		Log = substr(Log, i+1)
		i = index(Log, "\n")
	}
}'

# Pattern to match the `revision' line of rlog output.
rlog_revision_pattern='^revision [0-9]+\.[0-9]+(\.[0-9]+\.[0-9]+)*(['"$tab"' ]+locked by: [^'"$tab"' $,.0-9:;@]*[^'"$tab"' $,:;@][^'"$tab"' $,.0-9:;@]*;)?['"$tab"' ]*$'

case $hostname in
'')
	hostname=`(
		hostname || uname -n || uuname -l || cat /etc/whoami
	) 2>/dev/null` || {
		echo >&2 "$0: cannot deduce hostname"
		exit 1
	}

	case $hostname in
	*.*) ;;
	*)
		domainname=`(domainname) 2>/dev/null` &&
		case $domainname in
		*.*) hostname=$hostname.$domainname;;
		esac;;
	esac;;
esac


# Process the rlog output, generating ChangeLog style entries.

# First, reformat the rlog output so that each line contains one log entry.
# Transliterate \n to SOH so that multiline entries fit on a single line.
# Discard irrelevant rlog output.
$AWK '
	BEGIN {
		pository = "'"$pository"'"
		SOH="'"$SOH"'"
	}
	/^RCS file: / {
		if (pository != "") {
			filename = substr($0, 11)
			if (substr(filename, 1, length(pository)) == pository) {
				filename = substr(filename, length(pository) + 1)
			}
			if (filename ~ /,v$/) {
				filename = substr(filename, 1, length(filename) - 2)
			}
			if (filename ~ /(^|\/)Attic\/[^\/]*$/) {
				i = length(filename)
				while (substr(filename, i, 1) != "/") i--
				filename = substr(filename, 1, i - 6) substr(filename, i + 1)
			}
		}
		rev = "?"
	}
	/^Working file: / { if (repository == "") filename = substr($0, 15) }
	/'"$rlog_revision_pattern"'/, /^(-----------*|===========*)$/ {
		line = $0
		if (line ~ /'"$rlog_revision_pattern"'/) {
			rev = $2
			next
		}
		if (line ~ /^date: [0-9][- +\/0-9:]*;/) {
			date = $2
			if (date ~ /\//) {
				# This is a traditional RCS format date YYYY/MM/DD.
				# Replace "/"s with "-"s to get ISO format.
				newdate = ""
				while ((i = index(date, "/")) != 0) {
					newdate = newdate substr(date, 1, i-1) "-"
					date = substr(date, i+1)
				}
				date = newdate date
			}
			time = substr($3, 1, length($3) - 1)
			author = substr($5, 1, length($5)-1)
			printf "%s%s%s%s%s%s%s%s%s%s", filename, SOH, rev, SOH, date, SOH, time, SOH, author, SOH
			rev = "?"
			next
		}
		if (line ~ /^branches: /) { next }
		if (line ~ /^(-----------*|===========*)$/) { print ""; next }
		if (line == "Initial revision" || line ~ /^file .+ was initially added on branch .+\.$/) {
			line = "New file."
		}
		printf "%s%s", line, SOH
	}
' <"$rlogfile" |

# Now each line is of the form
# FILENAME@REVISION@YYYY-MM-DD@HH:MM:SS[+-TIMEZONE]@AUTHOR@LOG
#	where @ stands for an SOH (octal code 001),
#	and each line of LOG is terminated by SOH instead of \n.
# Sort the log entries, first by date+time (in reverse order),
# then by author, then by log entry, and finally by file name and revision
# (just in case).
sort -t"$SOH" +2 -4r +4 +0 |

# Finally, reformat the sorted log entries.
$AWK -F"$SOH" '
	BEGIN {
		logTZ = "'"$logTZ"'"
		revision = "'"$revision"'"

		# Initialize the fullname and mailaddr associative arrays.
		'"$initialize_fullname"'
		'"$initialize_mailaddr"'

		# Initialize indent string.
		indent_string = ""
		i = '"$indent"'
		if (0 < '"$tabwidth"')
			for (;  '"$tabwidth"' <= i;  i -= '"$tabwidth"')
				indent_string = indent_string "\t"
		while (1 <= i--)
			indent_string = indent_string " "
	}

	{
		newlog = ""
		for (i = 6; i < NF; i++) newlog = newlog $i "\n"

		# Ignore log entries prefixed by "#".
		if (newlog ~ /^#/) { next }

		if (Log != newlog || date != $3 || author != $5) {

			# The previous log and this log differ.

			# Print the old log.
			if (date != "") '"$printlogline"'

			# Logs that begin with "{clumpname} " should be grouped together,
			# and the clumpname should be removed.
			# Extract the new clumpname from the log header,
			# and use it to decide whether to output a blank line.
			newclumpname = ""
			sep = "\n"
			if (date == "") sep = ""
			if (newlog ~ /^\{[^'"$tab"' }]*}['"$tab"' ]/) {
				i = index(newlog, "}")
				newclumpname = substr(newlog, 1, i)
				while (substr(newlog, i+1) ~ /^['"$tab"' ]/) i++
				newlog = substr(newlog, i+1)
				if (clumpname == newclumpname) sep = ""
			}
			printf sep
			clumpname = newclumpname

			# Get ready for the next log.
			Log = newlog
			if (files != "")
				for (i in filesknown)
					filesknown[i] = 0
			files = ""
		}
		if (date != $3  ||  author != $5) {
			# The previous date+author and this date+author differ.
			# Print the new one.
			date = $3
			time = $4
			author = $5

			zone = ""
			if (logTZ && ((i = index(time, "-")) || (i = index(time, "+"))))
				zone = " " substr(time, i)

			# Print "date[ timezone]  fullname  <email address>".
			# Get fullname and email address from associative arrays;
			# default to author and author@hostname if not in arrays.
			if (fullname[author])
				auth = fullname[author]
			else
				auth = author
			printf "%s%s  %s  ", date, zone, auth
			if (mailaddr[author])
				printf "<%s>\n\n", mailaddr[author]
			else
				printf "<%s@%s>\n\n", author, "'"$hostname"'"
		}
		if (! filesknown[$1]) {
			filesknown[$1] = 1
			if (files == "") files = " " $1
			else files = files ", " $1
			if (revision && $2 != "?") files = files " " $2
		}
	}
	END {
		# Print the last log.
		if (date != "") {
			'"$printlogline"'
			printf "\n"
		}
	}
' &&


# Exit successfully.

exec rm -fr $logdir

# Local Variables:
# tab-width:4
# End:
