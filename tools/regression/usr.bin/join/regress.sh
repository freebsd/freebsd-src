# $FreeBSD: src/tools/regression/usr.bin/join/regress.sh,v 1.3 2002/06/24 14:22:38 jmallett Exp $

REGRESSION_START($1)

REGRESSION_TEST_ONE(`join -t , -a1 -a2 -e "(unknown)" -o 0,1.2,2.2 regress.1.in regress.2.in')

REGRESSION_END()
