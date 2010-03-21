# $FreeBSD: src/tools/regression/usr.bin/join/regress.sh,v 1.4.24.1 2010/02/10 00:26:20 kensmith Exp $

echo 1..1

REGRESSION_START($1)

REGRESSION_TEST_ONE(`join -t , -a1 -a2 -e "(unknown)" -o 0,1.2,2.2 regress.1.in regress.2.in')

REGRESSION_END()
