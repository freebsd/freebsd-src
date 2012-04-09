# $FreeBSD: src/tools/regression/usr.bin/uudecode/regress.sh,v 1.5.22.1.8.1 2012/03/03 06:15:13 kensmith Exp $

echo 1..2

REGRESSION_START($1)

REGRESSION_TEST_ONE(`uudecode -p < regress.traditional.in', `traditional')
REGRESSION_TEST_ONE(`uudecode -p < regress.base64.in', `base64')

REGRESSION_END()
