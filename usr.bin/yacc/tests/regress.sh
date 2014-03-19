# $FreeBSD$

echo 1..15

test_yacc() {
	yacc "${@}" | sed -e "s,${SRCDIR}/,,g"
}

REGRESSION_START($1)

REGRESSION_TEST(`00', `test_yacc -b regress -o /dev/stdout ${SRCDIR}/undefined.y')
REGRESSION_TEST(`01', `test_yacc -b regress -o /dev/stdout ${SRCDIR}/calc.y')
REGRESSION_TEST(`02', `test_yacc -b regress -o /dev/stdout ${SRCDIR}/calc1.y')
REGRESSION_TEST(`03', `test_yacc -b regress -o /dev/stdout ${SRCDIR}/calc3.y')
REGRESSION_TEST(`04', `test_yacc -b regress -o /dev/stdout ${SRCDIR}/code_calc.y')
REGRESSION_TEST(`05', `test_yacc -b regress -o /dev/stdout ${SRCDIR}/code_error.y')
REGRESSION_TEST(`06', `test_yacc -b regress -o /dev/stdout ${SRCDIR}/error.y')
REGRESSION_TEST(`07', `test_yacc -b regress -o /dev/stdout ${SRCDIR}/ftp.y')
REGRESSION_TEST(`08', `test_yacc -b regress -o /dev/stdout ${SRCDIR}/grammar.y')
REGRESSION_TEST(`09', `test_yacc -b regress -o /dev/stdout ${SRCDIR}/pure_calc.y')
REGRESSION_TEST(`10', `test_yacc -b regress -o /dev/stdout ${SRCDIR}/pure_error.y')
REGRESSION_TEST(`11', `test_yacc -b regress -o /dev/stdout ${SRCDIR}/quote_calc.y')
REGRESSION_TEST(`12', `test_yacc -b regress -o /dev/stdout ${SRCDIR}/quote_calc2.y')
REGRESSION_TEST(`13', `test_yacc -b regress -o /dev/stdout ${SRCDIR}/quote_calc3.y')
REGRESSION_TEST(`14', `test_yacc -b regress -o /dev/stdout ${SRCDIR}/quote_calc4.y')

REGRESSION_END()

