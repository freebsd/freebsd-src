#!/bin/sh
#
# $FreeBSD$
#
# Our current make(1)-based approach to dependency tracking cannot cope with
# certain source tree changes, including:
# - removing source files
# - replacing generated files with files committed to the tree
# - changing file extensions (e.g. a C source file rewritten in C++)
#
# We handle those cases here in an ad-hoc fashion by looking for the known-
# bad case in the main .depend file, and if found deleting all of the related
# .depend files (including for example the lib32 version).

OBJTOP=$1
if [ ! -d "$OBJTOP" ]; then
	echo "usage: $(basename $0) objtop" >&2
	exit 1
fi

# $1 directory
# $2 source filename w/o extension
# $3 source extension
clean_dep()
{
	if [ -e "$OBJTOP"/$1/.depend.$2.pico ] && \
	    egrep -qw "$2\.$3" "$OBJTOP"/$1/.depend.$2.pico; then \
		echo "Removing stale dependencies for $2.$3"; \
		rm -f "$OBJTOP"/$1/.depend.$2.* \
		    "$OBJTOP"/obj-lib32/$1/.depend.$2.*
	fi
}

# Date      Rev      Description
# 20200310  r358851  rename of openmp's ittnotify_static.c to .cpp
clean_dep lib/libomp ittnotify_static c
