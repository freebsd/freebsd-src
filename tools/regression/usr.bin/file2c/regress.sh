# $FreeBSD$

echo 1..1

REGRESSION_START($1)

REGRESSION_TEST_ONE(`file2c "const char data[] = {" ", 0};" < regress.in')

REGRESSION_END()
