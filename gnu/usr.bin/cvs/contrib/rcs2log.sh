#! /bin/sh

# RCS to ChangeLog generator

# Generate a change log prefix from RCS files and the ChangeLog (if any).
# Output the new prefix to standard output.
# You can edit this prefix by hand, and then prepend it to ChangeLog.

# Ignore log entries that start with `#'.
# Clump together log entries that start with `{topic} ',
# where `topic' contains neither white space nor `}'.

# Author: Paul Eggert <eggert@twinsun.com>

# $Id: rcs2log.sh,v 1.2 1995/07/28 19:48:45 eggert Exp $

# Copyright 1992, 1993, 1994, 1995 Free Software Foundation, Inc.

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
# along with this program; see the file COPYING.  If not, write to
# the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.

tab='	'
nl='
'

# Parse options.

# defaults
: ${AWK=awk}
: ${TMPDIR=/tmp}
hostname= # name of local host (if empty, will deduce it later)
indent=8 # indent of log line
length=79 # suggested max width of log line
logins= # login names for people we know fullnames and mailaddrs of
loginFullnameMailaddrs= # login<tab>fullname<tab>mailaddr triplets
recursive= # t if we want recursive rlog
rlog_options= # options to pass to rlog
tabwidth=8 # width of horizontal tab

while :
do
	case $1 in
	-i)	indent=${2?}; shift;;
	-h)	hostname=${2?}; shift;;
	-l)	length=${2?}; shift;;
	-[nu])	# -n is obsolescent; it is replaced by -u.
		case $1 in
		-n)	case ${2?}${3?}${4?} in
			*"$tab"* | *"$nl"*)
				echo >&2 "$0: -n '$2' '$3' '$4': tabs, newlines not allowed"
				exit 1
			esac
			loginFullnameMailaddrs=$loginFullnameMailaddrs$nl$2$tab$3$tab$4
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
				t=:
			esac
			case $2 in
			*"$t"*"$t"*"$t"*)
				echo >&2 "$0: -u '$2': too many fields"
				exit 1;;
			*"$t"*"$t"*)
				;;
			*)
				echo >&2 "$0: -u '$2': not enough fields"
				exit 1
			esac
			loginFullnameMailaddrs=$loginFullnameMailaddrs$nl$2
			shift
		esac
		logins=$logins$nl$login
		;;
	-r)	rlog_options=$rlog_options$nl${2?}; shift;;
	-R)	recursive=t;;
	-t)	tabwidth=${2?}; shift;;
	-*)	echo >&2 "$0: usage: $0 [options] [file ...]
Options:
	[-h hostname] [-i indent] [-l length] [-R] [-r rlog_option]
	[-t tabwidth] [-u 'login<TAB>fullname<TAB>mailaddr']..."
		exit 1;;
	*)	break
	esac
	shift
done

month_data='
	m[0]="Jan"; m[1]="Feb"; m[2]="Mar"
	m[3]="Apr"; m[4]="May"; m[5]="Jun"
	m[6]="Jul"; m[7]="Aug"; m[8]="Sep"
	m[9]="Oct"; m[10]="Nov"; m[11]="Dec"

	# days in non-leap year thus far, indexed by month (0-12)
	mo[0]=0; mo[1]=31; mo[2]=59; mo[3]=90
	mo[4]=120; mo[5]=151; mo[6]=181; mo[7]=212
	mo[8]=243; mo[9]=273; mo[10]=304; mo[11]=334
	mo[12]=365
'


# Put rlog output into $rlogout.

# If no rlog options are given,
# log the revisions checked in since the first ChangeLog entry.
case $rlog_options in
'')
	date=1970
	if test -s ChangeLog
	then
		# Add 1 to seconds to avoid duplicating most recent log.
		e='
			/^... ... [ 0-9][0-9] [ 0-9][0-9]:[0-9][0-9]:[0-9][0-9] [0-9]+ /{
				'"$month_data"'
				year = $5
				for (i=0; i<=11; i++) if (m[i] == $2) break
				dd = $3
				hh = substr($0,12,2)
				mm = substr($0,15,2)
				ss = substr($0,18,2)
				ss++
				if (ss == 60) {
					ss = 0
					mm++
					if (mm == 60) {
						mm = 0
						hh++
						if (hh == 24) {
							hh = 0
							dd++
							monthdays = mo[i+1] - mo[i]
							if (i == 1 && year%4 == 0 && (year%100 != 0 || year%400 == 0)) monthdays++
							if (dd == monthdays + 1) {
								dd = 1
								i++
								if (i == 12) {
									i = 0
									year++
								}
							}
						}
					}
				}
				# Output comma instead of space to avoid CVS 1.5 bug.
				printf "%d/%02d/%02d,%02d:%02d:%02d\n", year,i+1,dd,hh,mm,ss
				exit
			}
		'
		d=`$AWK "$e" <ChangeLog` || exit
		case $d in
		?*) date=$d
		esac
	fi
	datearg="-d>$date"
esac

# If CVS is in use, examine its repository, not the normal RCS files.
if test ! -f CVS/Repository
then
	rlog=rlog
	repository=
else
	rlog='cvs log'
	repository=`sed 1q <CVS/Repository` || exit
	test ! -f CVS/Root || CVSROOT=`cat <CVS/Root` || exit
	case $CVSROOT in
	*:/*)
		# remote repository
		;;
	*)
		# local repository
		case $repository in
		/*) ;;
		*) repository=${CVSROOT?}/$repository
		esac
		if test ! -d "$repository"
		then
			echo >&2 "$0: $repository: bad repository (see CVS/Repository)"
			exit 1
		fi
	esac
fi

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
					?*) find $RCSdirs -type f -print
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
				RCS/. | RCS/..) continue;;
				RCS/.\* | RCS/\* | .\*,v | \*,v) test -f "$file" || continue
				esac
				files=$files$nl$file
			done
			case $files in
			'') exit 0
			esac
		esac
		set x $files
		shift
		IFS=$oldIFS
	esac
esac

llogout=$TMPDIR/rcs2log$$l
rlogout=$TMPDIR/rcs2log$$r
trap exit 1 2 13 15
trap "rm -f $llogout $rlogout; exit 1" 0

case $rlog_options in
?*) $rlog $rlog_options ${1+"$@"} >$rlogout;;
'') $rlog "$datearg" ${1+"$@"} >$rlogout
esac || exit


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
		loginFullnameMailaddrs=`cat $llogout`
	esac

	oldIFS=$IFS
	IFS=$nl
	for loginFullnameMailaddr in $loginFullnameMailaddrs
	do
		case $loginFullnameMailaddr in
		*"$tab"*) IFS=$tab;;
		*) IFS=:
		esac
		set x $loginFullnameMailaddr
		login=$2
		fullname=$3
		mailaddr=$4
		initialize_fullname="$initialize_fullname
			fullname[\"$login\"] = \"$fullname\""
		initialize_mailaddr="$initialize_mailaddr
			mailaddr[\"$login\"] = \"$mailaddr\""
	done
	IFS=$oldIFS
esac

case $llogout in
?*) sort -u -o $llogout <<EOF || exit
$logins
EOF
esac
output_authors='/^date: / {
	if ($2 ~ /^[0-9]*[-\/][0-9][0-9][-\/][0-9][0-9]$/ && $3 ~ /^[0-9][0-9]:[0-9][0-9]:[0-9][0-9][-+0-9:]*;$/ && $4 == "author:" && $5 ~ /^[^;]*;$/) {
		print substr($5, 1, length($5)-1)
	}
}'
authors=`
	$AWK "$output_authors" <$rlogout |
	case $llogout in
	'') sort -u;;
	?*) sort -u | comm -23 - $llogout
	esac
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
		(cat /etc/passwd; ypmatch $authors passwd) 2>/dev/null |
		$AWK -F: "$awkscript"
	`$initialize_fullname
esac


# Function to print a single log line.
# We don't use awk functions, to stay compatible with old awk versions.
# `Log' is the log message (with \n replaced by \r).
# `files' contains the affected files.
printlogline='{

	# Following the GNU coding standards, rewrite
	#	* file: (function): comment
	# to
	#	* file (function): comment
	if (Log ~ /^\([^)]*\): /) {
		i = index(Log, ")")
		files = files " " substr(Log, 1, i)
		Log = substr(Log, i+3)
	}

	# If "label: comment" is too long, break the line after the ":".
	sep = " "
	if ('"$length"' <= '"$indent"' + 1 + length(files) + index(Log, CR)) sep = "\n" indent_string

	# Print the label.
	printf "%s*%s:", indent_string, files

	# Print each line of the log, transliterating \r to \n.
	while ((i = index(Log, CR)) != 0) {
		logline = substr(Log, 1, i-1)
		if (logline ~ /[^'"$tab"' ]/) {
			printf "%s%s\n", sep, logline
		} else {
			print ""
		}
		sep = indent_string
		Log = substr(Log, i+1)
	}
}'

case $hostname in
'')
	hostname=`(
		hostname || uname -n || uuname -l || cat /etc/whoami
	) 2>/dev/null` || {
		echo >&2 "$0: cannot deduce hostname"
		exit 1
	}
esac


# Process the rlog output, generating ChangeLog style entries.

# First, reformat the rlog output so that each line contains one log entry.
# Transliterate \n to \r so that multiline entries fit on a single line.
# Discard irrelevant rlog output.
$AWK <$rlogout '
	BEGIN { repository = "'"$repository"'" }
	/^RCS file:/ {
		if (repository != "") {
			filename = $3
			if (substr(filename, 1, length(repository) + 1) == repository "/") {
				filename = substr(filename, length(repository) + 2)
			}
			if (filename ~ /,v$/) {
				filename = substr(filename, 1, length(filename) - 2)
			}
		}
	}
	/^Working file:/ { if (repository == "") filename = $3 }
	/^date: /, /^(-----------*|===========*)$/ {
		if ($0 ~ /^branches: /) { next }
		if ($0 ~ /^date: [0-9][- +\/0-9:]*;/) {
			date = $2
			if (date ~ /-/) {
				# An ISO format date.  Replace all "-"s with "/"s.
				newdate = ""
				while ((i = index(date, "-")) != 0) {
					newdate = newdate substr(date, 1, i-1) "/"
					date = substr(date, i+1)
				}
				date = newdate date
			}
			# Ignore any time zone; ChangeLog has no room for it.
			time = substr($3, 1, 8)
			author = substr($5, 1, length($5)-1)
			printf "%s %s %s %s %c", filename, date, time, author, 13
			next
		}
		if ($0 ~ /^(-----------*|===========*)$/) { print ""; next }
		printf "%s%c", $0, 13
	}
' |

# Now each line is of the form
# FILENAME YYYY/MM/DD HH:MM:SS AUTHOR \rLOG
#	where \r stands for a carriage return,
#	and each line of the log is terminated by \r instead of \n.
# Sort the log entries, first by date+time (in reverse order),
# then by author, then by log entry, and finally by file name (just in case).
sort +1 -3r +3 +0 |

# Finally, reformat the sorted log entries.
$AWK '
	BEGIN {
		# Some awk variants do not understand "\r" or "\013", so we have to
		# put a carriage return directly in the file.
		CR="" # <-- There is a single CR between the " chars here.

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

		# Set up date conversion tables.
		# RCS uses a nice, clean, sortable format,
		# but ChangeLog wants the traditional, ugly ctime format.

		# January 1, 0 AD (Gregorian) was Saturday = 6
		EPOCH_WEEKDAY = 6
		# Of course, there was no 0 AD, but the algorithm works anyway.

		w[0]="Sun"; w[1]="Mon"; w[2]="Tue"; w[3]="Wed"
		w[4]="Thu"; w[5]="Fri"; w[6]="Sat"

		'"$month_data"'
	}

	{
		newlog = substr($0, 1 + index($0, CR))

		# Ignore log entries prefixed by "#".
		if (newlog ~ /^#/) { next }

		if (Log != newlog || date != $2 || author != $4) {

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
		if (date != $2  ||  author != $4) {
			# The previous date+author and this date+author differ.
			# Print the new one.
			date = $2
			author = $4

			# Convert nice RCS date like "1992/01/03 00:03:44"
			# into ugly ctime date like "Fri Jan  3 00:03:44 1992".
			# Calculate day of week from Gregorian calendar.
			i = index($2, "/")
			year = substr($2, 1, i-1) + 0
			monthday = substr($2, i+1)
			i = index(monthday, "/")
			month = substr(monthday, 1, i-1) + 0
			day = substr(monthday, i+1) + 0
			leap = 0
			if (2 < month && year%4 == 0 && (year%100 != 0 || year%400 == 0)) leap = 1
			days_since_Sunday_before_epoch = EPOCH_WEEKDAY + year * 365 + int((year + 3) / 4) - int((year + 99) / 100) + int((year + 399) / 400) + mo[month-1] + leap + day - 1

			# Print "date  fullname  (email address)".
			# Get fullname and email address from associative arrays;
			# default to author and author@hostname if not in arrays.
			if (fullname[author])
				auth = fullname[author]
			else
				auth = author
			printf "%s %s %2d %s %d  %s  ", w[days_since_Sunday_before_epoch%7], m[month-1], day, $3, year, auth
			if (mailaddr[author])
				printf "<%s>\n\n", mailaddr[author]
			else
				printf "<%s@%s>\n\n", author, "'"$hostname"'"
		}
		if (! filesknown[$1]) {
			filesknown[$1] = 1
			if (files == "") files = " " $1
			else files = files ", " $1
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

exec rm -f $llogout $rlogout
