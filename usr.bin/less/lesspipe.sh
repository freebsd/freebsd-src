#! /bin/sh
# ex:ts=8

# $FreeBSD: src/usr.bin/less/lesspipe.sh,v 1.4.8.1 2009/04/15 03:14:26 kensmith Exp $

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
