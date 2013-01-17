# $FreeBSD$

LC_ALL=C; export LC_ALL

echo 1..15

REGRESSION_START($1)

REGRESSION_TEST(`00', `/usr/obj/usr/src/usr.bin/yacc/yacc -b regress -o /dev/stdout undefined.y')
REGRESSION_TEST(`01', `/usr/obj/usr/src/usr.bin/yacc/yacc -b regress -o /dev/stdout calc.y')
REGRESSION_TEST(`02', `/usr/obj/usr/src/usr.bin/yacc/yacc -b regress -o /dev/stdout calc1.y')
REGRESSION_TEST(`03', `/usr/obj/usr/src/usr.bin/yacc/yacc -b regress -o /dev/stdout calc3.y')
REGRESSION_TEST(`04', `/usr/obj/usr/src/usr.bin/yacc/yacc -b regress -o /dev/stdout code_calc.y')
REGRESSION_TEST(`05', `/usr/obj/usr/src/usr.bin/yacc/yacc -b regress -o /dev/stdout code_error.y')
REGRESSION_TEST(`06', `/usr/obj/usr/src/usr.bin/yacc/yacc -b regress -o /dev/stdout error.y')
REGRESSION_TEST(`07', `/usr/obj/usr/src/usr.bin/yacc/yacc -b regress -o /dev/stdout ftp.y')
REGRESSION_TEST(`08', `/usr/obj/usr/src/usr.bin/yacc/yacc -b regress -o /dev/stdout grammar.y')
REGRESSION_TEST(`09', `/usr/obj/usr/src/usr.bin/yacc/yacc -b regress -o /dev/stdout pure_calc.y')
REGRESSION_TEST(`10', `/usr/obj/usr/src/usr.bin/yacc/yacc -b regress -o /dev/stdout pure_error.y')
REGRESSION_TEST(`11', `/usr/obj/usr/src/usr.bin/yacc/yacc -b regress -o /dev/stdout quote_calc.y')
REGRESSION_TEST(`12', `/usr/obj/usr/src/usr.bin/yacc/yacc -b regress -o /dev/stdout quote_calc2.y')
REGRESSION_TEST(`13', `/usr/obj/usr/src/usr.bin/yacc/yacc -b regress -o /dev/stdout quote_calc3.y')
REGRESSION_TEST(`14', `/usr/obj/usr/src/usr.bin/yacc/yacc -b regress -o /dev/stdout quote_calc4.y')

REGRESSION_END()

