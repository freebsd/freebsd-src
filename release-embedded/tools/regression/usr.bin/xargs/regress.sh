# $FreeBSD$

echo 1..13

REGRESSION_START($1)

REGRESSION_TEST(`normal', `xargs echo The < regress.in')
REGRESSION_TEST(`I', `xargs -I% echo The % % % %% % % < regress.in')
REGRESSION_TEST(`J', `xargs -J% echo The % again. < regress.in')
REGRESSION_TEST(`L', `xargs -L3 echo < regress.in')
REGRESSION_TEST(`R', `xargs -I% -R1 echo The % % % %% % % < regress.in')
REGRESSION_TEST(`n1', `xargs -n1 echo < regress.in')
REGRESSION_TEST(`n2', `xargs -n2 echo < regress.in')
REGRESSION_TEST(`n3', `xargs -n3 echo < regress.in')
REGRESSION_TEST(`0', `xargs -0 -n1 echo < regress.0.in')
REGRESSION_TEST(`0I', `xargs -0 -I% echo The % %% % < regress.0.in')
REGRESSION_TEST(`0J', `xargs -0 -J% echo The % again. < regress.0.in')
REGRESSION_TEST(`0L', `xargs -0 -L2 echo < regress.0.in')
REGRESSION_TEST(`quotes', `xargs -n1 echo < regress.quotes.in')

REGRESSION_END()
