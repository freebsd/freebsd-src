#! /bin/sh
# ex:ts=8

# $FreeBSD: src/usr.bin/less/lesspipe.sh,v 1.1.8.1 2005/06/20 07:11:24 des Exp $

case "$1" in
	*.Z)
		exec uncompress -c $1	2>/dev/null
		;;
	*.gz)
		exec gzip -d -c $1	2>/dev/null
		;;
	*.bz2)
		exec bzip2 -d -c $1	2>/dev/null
		;;
	*)
		exec cat $1		2>/dev/null
		;;
esac
