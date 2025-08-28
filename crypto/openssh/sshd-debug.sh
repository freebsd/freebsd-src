#!/bin/sh

# ssh-debug

# A wrapper script around sshd to invoke when debugging to debug the
#  work-in-progress versions of sshd-auth and sshd-session, instead
# of debugging the installed ones that probably don't have the change
# you are working on.
#
#	Placed in the Public Domain.

unset DIR SSHD SSHD_AUTH SSHD_SESSION

fatal() {
	echo >&2 $@
	exit 1
}

case "$0" in
/*)			DIR="`dirname $0`"	;;
./sshd-debug.sh)	DIR="`pwd`"		;;
*)			echo "Need full path or working directory."; exit 1 ;;
esac

for i in sshd/obj/sshd sshd/sshd sshd; do
	if [ -f "${DIR}/$i" ] && [ -x "${DIR}/$i" ]; then
		SSHD="${DIR}/$i"
	fi
done
[ -z "${SSHD}" ] && fatal "Could not find sshd"

for i in sshd-auth/obj/sshd-auth sshd-auth/sshd-auth sshd-auth; do
	if [ -f "${DIR}/$i" ] && [ -x "${DIR}/$i" ]; then
		SSHD_AUTH="${DIR}/$i"
	fi
done
[ -z "${SSHD_AUTH}" ] && fatal "Could not find sshd-auth"

for i in sshd-session/obj/sshd-session sshd-session/sshd-session sshd-session; do
	if [ -f "${DIR}/$i" ] && [ -x "${DIR}/$i" ]; then
		SSHD_SESSION="${DIR}/$i"
	fi
done
[ -z "${SSHD_SESSION}" ] && fatal "Could not find sshd-session"

echo >&2 Debugging ${SSHD} auth ${SSHD_AUTH} session ${SSHD_SESSION}

# Append SshdSessionPath and SshdAuthPath pointing to the build directory.
# If you explicitly specify these in the command line, the first-match
# keyword semantics will override these.
exec "${SSHD}" $@ \
    -oSshdAuthPath="${SSHD_AUTH}" -oSshdSessionPath="${SSHD_SESSION}"
