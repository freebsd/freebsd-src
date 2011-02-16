# $FreeBSD: src/tools/regression/usr.bin/uudecode/regress.sh,v 1.5.22.1.6.1 2010/12/21 17:09:25 kensmith Exp $

echo 1..2

REGRESSION_START($1)

REGRESSION_TEST_ONE(`uudecode -p < regress.traditional.in', `traditional')
REGRESSION_TEST_ONE(`uudecode -p < regress.base64.in', `base64')

REGRESSION_END()
