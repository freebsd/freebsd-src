# $FreeBSD: src/tools/regression/usr.bin/uuencode/regress.sh,v 1.6 2002/06/24 13:52:28 jmallett Exp $

REGRESSION_START($1)

# To make sure we end up with matching headers.
umask 022

REGRESSION_TEST(`traditional', `uuencode regress.in < regress.in')
REGRESSION_TEST(`base64', `uuencode -m regress.in < regress.in')

REGRESSION_END()
