# $FreeBSD: src/tools/regression/usr.bin/comm/regress.sh,v 1.1.2.2.4.1 2012/03/03 06:15:13 kensmith Exp $

LC_ALL=C; export LC_ALL

echo 1..3

REGRESSION_START($1)

REGRESSION_TEST(`00', `comm -12 regress.00a.in regress.00b.in')
REGRESSION_TEST(`01', `comm -12 regress.01a.in regress.01b.in')
REGRESSION_TEST(`02', `comm regress.02a.in regress.02b.in')

REGRESSION_END()
