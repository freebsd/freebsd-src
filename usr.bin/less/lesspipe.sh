#! /bin/sh
# ex:ts=8

# $FreeBSD: src/usr.bin/less/lesspipe.sh,v 1.2.2.1 2006/09/03 16:09:50 delphij Exp $

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
esac
