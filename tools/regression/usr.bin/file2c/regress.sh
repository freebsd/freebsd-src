# $FreeBSD: src/tools/regression/usr.bin/file2c/regress.sh,v 1.3 2002/06/24 14:22:35 jmallett Exp $

REGRESSION_START($1)

REGRESSION_TEST_ONE(`file2c "const char data[] = {" ", 0};" < regress.in')

REGRESSION_END()
