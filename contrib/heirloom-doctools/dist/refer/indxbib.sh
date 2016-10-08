#	Copyright 1988 Sun Microsystems, Inc. All rights reserved.
#	Use is subject to license terms.

#	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T
#	All Rights Reserved

#	Copyright (c) 1980 Regents of the University of California.
#	All rights reserved. The Berkeley software License Agreement
#	specifies the terms and conditions for redistribution.

# from OpenSolaris "indxbib.sh	1.5	05/06/03 SMI" 
#
# Portions Copyright (c) 2005 Gunnar Ritter, Freiburg i. Br., Germany
#
# Sccsid @(#)indxbib.sh	1.3 (gritter) 10/22/05
#
#	indxbib sh script
#
if test x"$1" != x
	then @REFDIR@/mkey "$@" | @REFDIR@/inv "_$1"
	mv "_$1.ia" "$1.ia"
	mv "_$1.ib" "$1.ib"
	mv "_$1.ic" "$1.ic"
else
	echo 'Usage:  indxbib database [ ... ]
	first argument is the basename for indexes
	indexes will be called database.{ia,ib,ic}'
fi
