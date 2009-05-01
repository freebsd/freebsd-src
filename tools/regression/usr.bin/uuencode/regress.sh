# $FreeBSD: src/tools/regression/usr.bin/uuencode/regress.sh,v 1.7.20.1 2009/04/15 03:14:26 kensmith Exp $

echo 1..2

REGRESSION_START($1)

# To make sure we end up with matching headers.
umask 022

REGRESSION_TEST(`traditional', `uuencode regress.in < regress.in')
REGRESSION_TEST(`base64', `uuencode -m regress.in < regress.in')

REGRESSION_END()
