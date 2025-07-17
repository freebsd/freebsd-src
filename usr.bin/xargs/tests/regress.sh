
echo 1..21

REGRESSION_START($1)

REGRESSION_TEST(`normal', `xargs echo The <${SRCDIR}/regress.in')
REGRESSION_TEST(`I', `xargs -I% echo The % % % %% % % <${SRCDIR}/regress.in')
REGRESSION_TEST(`J', `xargs -J% echo The % again. <${SRCDIR}/regress.in')
REGRESSION_TEST(`L', `xargs -L3 echo <${SRCDIR}/regress.in')
REGRESSION_TEST(`P1', `xargs -P1 echo <${SRCDIR}/regress.in')
REGRESSION_TEST(`R', `xargs -I% -R1 echo The % % % %% % % <${SRCDIR}/regress.in')
REGRESSION_TEST(`R-1', `xargs -I% -R-1 echo The % % % %% % % <${SRCDIR}/regress.in')
REGRESSION_TEST(`n1', `xargs -n1 echo <${SRCDIR}/regress.in')
REGRESSION_TEST(`n2', `xargs -n2 echo <${SRCDIR}/regress.in')
# This test may consume a large amount of memory, making it unsuited to CI
# environments.  Disable it for now.
#REGRESSION_TEST(`n2147483647', `xargs -n2147483647 <${SRCDIR}/regress.in')
REGRESSION_TEST(`n2P0',`xargs -n2 -P0 echo <${SRCDIR}/regress.in | sort')
REGRESSION_TEST(`n3', `xargs -n3 echo <${SRCDIR}/regress.in')
REGRESSION_TEST(`0', `xargs -0 -n1 echo <${SRCDIR}/regress.0.in')
REGRESSION_TEST(`0I', `xargs -0 -I% echo The % %% % <${SRCDIR}/regress.0.in')
REGRESSION_TEST(`0J', `xargs -0 -J% echo The % again. <${SRCDIR}/regress.0.in')
REGRESSION_TEST(`0L', `xargs -0 -L2 echo <${SRCDIR}/regress.0.in')
REGRESSION_TEST(`0P1', `xargs -0 -P1 echo <${SRCDIR}/regress.0.in')
REGRESSION_TEST(`quotes', `xargs -n1 echo <${SRCDIR}/regress.quotes.in')

REGRESSION_TEST_FREEFORM(`parallel1', `echo /var/empty       /var/empty       | xargs -n1 -P2 test -d; [ $? = 0 ]')
REGRESSION_TEST_FREEFORM(`parallel2', `echo /var/empty       /var/empty/nodir | xargs -n1 -P2 test -d; [ $? = 1 ]')
REGRESSION_TEST_FREEFORM(`parallel3', `echo /var/empty/nodir /var/empty       | xargs -n1 -P2 test -d; [ $? = 1 ]')
REGRESSION_TEST_FREEFORM(`parallel4', `echo /var/empty/nodir /var/empty/nodir | xargs -n1 -P2 test -d; [ $? = 1 ]')

REGRESSION_END()
