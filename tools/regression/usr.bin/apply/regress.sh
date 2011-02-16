# $FreeBSD: src/tools/regression/usr.bin/apply/regress.sh,v 1.1.2.2.2.1 2010/12/21 17:09:25 kensmith Exp $

echo 1..2

REGRESSION_START($1)

REGRESSION_TEST(`00', `apply "echo %1 %1 %1 %1" $(cat regress.00.in)')
REGRESSION_TEST(`01', `sh regress.01.sh')

REGRESSION_END()
