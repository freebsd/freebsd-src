# $FreeBSD: src/tools/regression/usr.bin/uudecode/regress.sh,v 1.5.22.1.2.1 2009/10/25 01:10:29 kensmith Exp $

echo 1..2

REGRESSION_START($1)

REGRESSION_TEST_ONE(`uudecode -p < regress.traditional.in', `traditional')
REGRESSION_TEST_ONE(`uudecode -p < regress.base64.in', `base64')

REGRESSION_END()
