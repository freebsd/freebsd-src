# $FreeBSD$

echo 1..2

REGRESSION_START($1)

REGRESSION_TEST(`traditional', `uuencode regress.in <${SRCDIR}/regress.in')
REGRESSION_TEST(`base64', `uuencode -m regress.in <${SRCDIR}/regress.in')

# was uudecode: stdin: /dev/null: character out of range: [33-96]
REGRESSION_TEST(`153276', `uudecode -o /dev/null < regress.153276.in 2>&1')

REGRESSION_END()
