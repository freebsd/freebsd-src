# $FreeBSD$

LC_ALL=C; export LC_ALL

echo 1..15

REGRESSION_START($1)

REGRESSION_TEST(`00', `yacc -b regress -o /dev/stdout undefined.y')
REGRESSION_TEST(`01', `yacc -b regress -o /dev/stdout calc.y')
REGRESSION_TEST(`02', `yacc -b regress -o /dev/stdout calc1.y')
REGRESSION_TEST(`03', `yacc -b regress -o /dev/stdout calc3.y')
REGRESSION_TEST(`04', `yacc -b regress -o /dev/stdout code_calc.y')
REGRESSION_TEST(`05', `yacc -b regress -o /dev/stdout code_error.y')
REGRESSION_TEST(`06', `yacc -b regress -o /dev/stdout error.y')
REGRESSION_TEST(`07', `yacc -b regress -o /dev/stdout ftp.y')
REGRESSION_TEST(`08', `yacc -b regress -o /dev/stdout grammar.y')
REGRESSION_TEST(`09', `yacc -b regress -o /dev/stdout pure_calc.y')
REGRESSION_TEST(`10', `yacc -b regress -o /dev/stdout pure_error.y')
REGRESSION_TEST(`11', `yacc -b regress -o /dev/stdout quote_calc.y')
REGRESSION_TEST(`12', `yacc -b regress -o /dev/stdout quote_calc2.y')
REGRESSION_TEST(`13', `yacc -b regress -o /dev/stdout quote_calc3.y')
REGRESSION_TEST(`14', `yacc -b regress -o /dev/stdout quote_calc4.y')

REGRESSION_END()

