/*
 * $FreeBSD: src/usr.bin/colldef/common.h,v 1.2.38.1.2.1 2009/10/25 01:10:29 kensmith Exp $
 */

#define CHARMAP_SYMBOL_LEN 64
#define BUFSIZE 80

extern int line_no;

extern u_char charmap_table[UCHAR_MAX + 1][CHARMAP_SYMBOL_LEN];
extern char map_name[FILENAME_MAX];
