#! /bin/sh
# ex:ts=8

# $FreeBSD$

case "$1" in
	*.Z)	uncompress -c $1	2>/dev/null
		;;
	*.gz)	gzip -d -c $1		2>/dev/null
		;;
	*.bz2)	bzip2 -d -c $1		2>/dev/null
		;;
esac
