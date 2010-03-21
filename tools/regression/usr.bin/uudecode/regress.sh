# $FreeBSD: src/tools/regression/usr.bin/uudecode/regress.sh,v 1.5.24.1 2010/02/10 00:26:20 kensmith Exp $

echo 1..2

REGRESSION_START($1)

REGRESSION_TEST_ONE(`uudecode -p < regress.traditional.in', `traditional')
REGRESSION_TEST_ONE(`uudecode -p < regress.base64.in', `base64')

REGRESSION_END()
