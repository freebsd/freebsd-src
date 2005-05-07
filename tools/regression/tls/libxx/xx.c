/* $FreeBSD: src/tools/regression/tls/libxx/xx.c,v 1.1 2004/08/03 09:04:00 dfr Exp $ */

extern int __thread yy1;
int __thread xx1 = 1;
int __thread xx2 = 2;
int __thread xxa[10] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };

int
xxyy()
{
	return yy1;
}
