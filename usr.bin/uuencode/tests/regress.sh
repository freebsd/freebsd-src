# $FreeBSD$

echo 1..5

REGRESSION_START($1)

REGRESSION_TEST(`traditional', `uuencode regress.in <${SRCDIR}/regress.in')
REGRESSION_TEST(`base64', `uuencode -m regress.in <${SRCDIR}/regress.in')
REGRESSION_TEST_ONE(`uudecode -p <${SRCDIR}/regress.traditional.in', `traditional')
REGRESSION_TEST_ONE(`uudecode -p <${SRCDIR}/regress.base64.in', `base64')

REGRESSION_END()
