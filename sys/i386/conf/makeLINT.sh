#! /bin/sh
# $FreeBSD$

/usr/bin/sed -e 's/#.*//' -e 's/\//' | /usr/bin/awk '
/^[ \t]*$/	{ next }
/^hint\./	{ next }
/^(\
machine|\
ident|\
device|\
makeoptions|\
options|\
profile|\
cpu|\
option|\
maxusers\
)[ \t]/		{ print; next }
{ printf("unrecognized line: line %d: %s\n", NR, $0) > "/dev/stderr" }
'
