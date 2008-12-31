#! /bin/sh
# ex:ts=8

# $FreeBSD: src/usr.bin/less/lesspipe.sh,v 1.4.6.1 2008/11/25 02:59:29 kensmith Exp $

case "$1" in
	*.Z)
		exec uncompress -c "$1"	2>/dev/null
		;;
	*.gz)
		exec gzip -d -c "$1"	2>/dev/null
		;;
	*.bz2)
		exec bzip2 -d -c "$1"	2>/dev/null
		;;
esac
