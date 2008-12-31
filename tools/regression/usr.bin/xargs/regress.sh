# $FreeBSD: src/tools/regression/usr.bin/xargs/regress.sh,v 1.6.18.1 2008/11/25 02:59:29 kensmith Exp $

echo 1..5

REGRESSION_START($1)

REGRESSION_TEST(`normal', `xargs echo The < regress.in')
REGRESSION_TEST(`I', `xargs -I% echo The % % % %% % % < regress.in')
REGRESSION_TEST(`J', `xargs -J% echo The % again. < regress.in')
REGRESSION_TEST(`L', `xargs -L3 echo < regress.in')
REGRESSION_TEST(`R', `xargs -I% -R1 echo The % % % %% % % < regress.in')

REGRESSION_END()
