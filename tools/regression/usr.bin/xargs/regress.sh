# $FreeBSD: src/tools/regression/usr.bin/xargs/regress.sh,v 1.6.16.1 2008/10/02 02:57:24 kensmith Exp $

echo 1..5

REGRESSION_START($1)

REGRESSION_TEST(`normal', `xargs echo The < regress.in')
REGRESSION_TEST(`I', `xargs -I% echo The % % % %% % % < regress.in')
REGRESSION_TEST(`J', `xargs -J% echo The % again. < regress.in')
REGRESSION_TEST(`L', `xargs -L3 echo < regress.in')
REGRESSION_TEST(`R', `xargs -I% -R1 echo The % % % %% % % < regress.in')

REGRESSION_END()
