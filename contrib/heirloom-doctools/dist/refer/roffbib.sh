#	Copyright 1988 Sun Microsystems, Inc. All Rights Reserved.
#	Use is subject to license terms.

#	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T
#	All Rights Reserved

#	Copyright (c) 1980 Regents of the University of California.
#	All rights reserved. The Berkeley software License Agreement
#	specifies the terms and conditions for redistribution.

# from OpenSolaris "roffbib.sh	1.6	05/06/03 SMI" 
#
# Portions Copyright (c) 2005 Gunnar Ritter, Freiburg i. Br., Germany
#
# Sccsid @(#)roffbib.sh	1.3 (gritter) 10/22/05

#
#	roffbib sh script
#
flags=
abstr=
headr=BIBLIOGRAPHY
xroff=@BINDIR@/nroff
macro=-mbib

for i
do case $1 in
	-[onsrT]*|-[qeh])
		flags="$flags $1"
		shift ;;
	-x)
		abstr="X.ig ]-"
		shift ;;
	-m)
		shift
		macro="-m$1"
		shift ;;
	-Q)
		xroff="@BINDIR@/troff"
		shift ;;
	-H)
		shift
		headr="$1"
		shift ;;
	-*)
		echo "roffbib: unknown flag: $1"
		shift
	esac
done
if test "x$1" != x
then
	(echo .ds TL $headr; @BINDIR@/refer -a1 -B"$abstr" "$@") | \
	    $xroff $flags $macro
else
	(echo .ds TL $headr; @BINDIR@/refer -a1 -B"$abstr") | $xroff $flags $macro
fi
