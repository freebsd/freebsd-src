#!/bin/sh
#
# Copyright (c) 2002 Michael Telahun Makonnen. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 3. The name of the author may not be used to endorse or promote products
#    derived from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
# IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
# OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
# IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
# INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
# NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
#	Email: Mike Makonnen <mtm@identd.net>
#
# $FreeBSD$
#

ATJOBDIR="/var/at/jobs"
CRONJOBDIR="/var/cron/tabs"
MAILSPOOL="/var/mail"
SIGKILL="-KILL"
TEMPDIRS="/tmp /var/tmp"
THISCMD=`/usr/bin/basename $0`

# err msg
#	Display $msg on stderr.
#
err() {
	echo 1>&2 ${THISCMD}: $*
}

# rm_files login
#	Removes files or empty directories belonging to $login from various
#	temporary directories.
#
rm_files() {
	# The argument is required
	[ -n $1 ] && login=$1 || return
	
	for _dir in ${TEMPDIRS} ; do
		if [ ! -d $_dir ]; then
			err "$_dir is not a valid directory."
			continue
		fi
		echo -n "Removing files owned by ($login) in $_dir:"
		filecount=`find 2>/dev/null "$_dir" -user "$login" -delete -print | \
		    wc -l | sed 's/ *//'`
		echo " $filecount removed."
	done
}

# rm_mail login
#	Removes unix mail and pop daemon files belonging to the user
#	specified in the $login argument.
#
rm_mail() {
	# The argument is required
	[ -n $1 ] && login=$1 || return

	echo -n "Removing mail spool(s) for ($login):"
	if [ -f ${MAILSPOOL}/$login ]; then
		echo -n " ${MAILSPOOL}/$login"
		rm ${MAILSPOOL}/$login
	fi
	if [ -f ${MAILSPOOL}/${login}.pop ]; then
		echo -n " ${MAILSPOOL}/${login}.pop"
		rm ${MAILSPOOL}/${login}.pop
	fi
	echo '.'
}

# kill_procs login
#	Send a SIGKILL to all processes owned by $login.
#
kill_procs() {
	# The argument is required
	[ -n $1 ] && login=$1 || return

	echo -n "Terminating all processes owned by ($login):"
	killcount=0
	proclist=`ps 2>/dev/null -U $login | grep -v '^\ *PID' | awk '{print $1}'`
	for _pid in $proclist ; do
		kill 2>/dev/null ${SIGKILL} $_pid
		killcount=$(($killcount + 1))
	done
	echo " ${SIGKILL} signal sent to $killcount processes."
}

# rm_at_jobs login
#	Remove at (1) jobs belonging to $login.
#
rm_at_jobs() {
	# The argument is required
	[ -n $1 ] && login=$1 || return

	atjoblist=`find 2>/dev/null ${ATJOBDIR} -maxdepth 1 -user $login -print`
	jobcount=0
	echo -n "Removing at(1) jobs owned by ($login):"
	for _atjob in $atjoblist ; do
		rm -f $_atjob
		jobcount=$(($jobcount + 1))
	done
	echo " $jobcount removed."
}

# rm_crontab login
#	Removes crontab file belonging to user $login.
#
rm_crontab() {
	# The argument is required
	[ -n $1 ] && login=$1 || return

	echo -n "Removing crontab for ($login):"
	if [ -f ${CRONJOBDIR}/$login ]; then
		echo -n " ${CRONJOBDIR}/$login"
		rm -f ${CRONJOBDIR}/$login
	fi
	echo '.'
}

# rm_user login
#	Remove user $login from the system. This subroutine makes use
#	of the pw(8) command to remove a user from the system. The pw(8)
#	command will remove the specified user from the user database
#	and group file and remove any crontabs. His home
#	directory will be removed if it is owned by him and contains no 
#	files or subdirectories owned by other users. Mail spool files will
#	also be removed.
#
rm_user() {
	# The argument is required
	[ -n $1 ] && login=$1 || return

	echo -n "Removing user ($login)"
	[ -n "$pw_rswitch" ] && echo -n " (including home directory)"
	echo -n " from the system:"
	pw userdel -n $login $pw_rswitch
	echo ' Done.'
}

# prompt_yesno msg
#	Prompts the user with a $msg. The answer is expected to be
#	yes, no, or some variation thereof. This subroutine returns 0
#	if the answer was yes, 1 if it was not.
#
prompt_yesno() {
	# The argument is required
	[ -n "$1" ] && msg="$1" || return

        while : ; do
                echo -n "$msg"
                read _ans
                case $_ans in
                [Nn][Oo]|[Nn])
			return 1
                        ;;
                [Yy][Ee][Ss]|[Yy][Ee]|[Yy])
                        return 0
                        ;;
                *)
                        ;;
                esac
	done
}

# show_usage
#	(no arguments)
#	Display usage message.
#
show_usage() {
	echo "usage: ${THISCMD} [-y] [-f file] [user ...]"
	echo "       if the -y switch is used, either the -f switch or"
	echo "       one or more user names must be given"
}

#### END SUBROUTINE DEFENITION ####

ffile=
fflag=
procowner=
pw_rswitch=
userlist=
yflag=

procowner=`/usr/bin/id -u`
if [ "$procowner" != "0" ]; then
	err 'you must be root (0) to use this utility.'
	exit 1
fi

args=`getopt 2>/dev/null yf: $*`
if [ "$?" != "0" ]; then
	show_usage
	exit 1
fi
set -- $args
for _switch ; do
	case $_switch in
	-y)
		yflag=1
		shift
		;;
	-f)
		fflag=1
		ffile="$2"
		shift; shift
		;;
	--)
		shift
		break
		;;
	esac
done

# Get user names from a file if the -f switch was used. Otherwise,
# get them from the commandline arguments. If we're getting it
# from a file, the file must be owned by and writable only by root.
#
if [ $fflag ]; then
	_insecure=`find $ffile ! -user 0 -or -perm +0022`
	if [ -n "$_insecure" ]; then
		err "file ($ffile) must be owned by and writeable only by root."
		exit 1
	fi
	if [ -r "$ffile" ]; then
		userlist=`cat $ffile | while read _user _junk ; do
			case $_user in
			\#*|'')
				;;
			*)
				echo -n "$userlist $_user"
				;;
			esac
		done`
	fi
else
	while [ $1 ] ; do
		userlist="$userlist $1"
		shift
	done
fi

# If the -y or -f switch has been used and the list of users to remove
# is empty it is a fatal error. Otherwise, prompt the user for a list
# of one or more user names.
#
if [ ! "$userlist" ]; then
	if [ $fflag ]; then
		err "($ffile) does not exist or does not contain any user names."
		exit 1
	elif [ $yflag ]; then
		show_usage
		exit 1
	else
		echo -n "Please enter one or more user name's: "
		read userlist
	fi
fi

_user=
_uid=
for _user in $userlist ; do
	# Make sure the name exists in the passwd database and that it
	# does not have a uid of 0
	#
	userrec=`pw 2>/dev/null usershow -n $_user`
	if [ "$?" != "0" ]; then
		err "user ($_user) does not exist in the password database."
		continue
	fi
	_uid=`echo $userrec | awk -F: '{print $3}'`
	if [ "$_uid" = "0" ]; then
		err "user ($_user) has uid 0. You may not remove this user."
		continue
	fi

	# If the -y switch was not used ask for confirmation to remove the
	# user and home directory.
	#
	if [ -z "$yflag" ]; then
		echo "Matching password entry:"
		echo
		echo $userrec
		echo
		if ! prompt_yesno "Is this the entry you wish to remove? " ; then
			continue
		fi
		_homedir=`echo $userrec | awk -F: '{print $9}'`
		if prompt_yesno "Remove user's home directory ($_homedir)? "; then
			pw_rswitch="-r"
		fi
	else
		pw_rswitch="-r"
	fi

	# Disable any further attempts to log into this account
	pw 2>/dev/null lock $_user

	# Remove crontab, mail spool, etc. Then obliterate the user from
	# the passwd and group database.
	rm_crontab $_user
	rm_at_jobs $_user
	kill_procs $_user
	rm_mail $_user
	rm_files $_user
	rm_user $_user
done
