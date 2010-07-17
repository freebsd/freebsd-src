#! /bin/sh
# ex:ts=8

# $FreeBSD: src/usr.bin/less/lesspipe.sh,v 1.4.10.2.2.1 2010/06/14 02:09:06 kensmith Exp $

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
	*.xz)
		exec xz -d -c "$1"	2>/dev/null
		;;
	*.lzma)
		exec lzma -d -c "$1"	2>/dev/null
		;;
esac
