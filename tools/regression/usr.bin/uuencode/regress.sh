# $FreeBSD$

REGRESSION_START($1)

# To make sure we end up with matching headers.
umask 022

REGRESSION_TEST(`traditional', `uuencode regress.in < regress.in')
REGRESSION_TEST(`base64', `uuencode -m regress.in < regress.in')

REGRESSION_END()
