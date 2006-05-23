# $FreeBSD: src/tools/regression/usr.bin/xargs/regress.sh,v 1.5 2002/06/24 13:52:28 jmallett Exp $

REGRESSION_START($1)

REGRESSION_TEST(`normal', `xargs echo The < regress.in')
REGRESSION_TEST(`I', `xargs -I% echo The % % % %% % % < regress.in')
REGRESSION_TEST(`J', `xargs -J% echo The % again. < regress.in')
REGRESSION_TEST(`L', `xargs -L3 echo < regress.in')
REGRESSION_TEST(`R', `xargs -I% -R1 echo The % % % %% % % < regress.in')

REGRESSION_END()
