/*
 * $FreeBSD: src/usr.bin/colldef/common.h,v 1.2.32.1 2008/10/02 02:57:24 kensmith Exp $
 */

#define CHARMAP_SYMBOL_LEN 64
#define BUFSIZE 80

extern int line_no;

extern u_char charmap_table[UCHAR_MAX + 1][CHARMAP_SYMBOL_LEN];
extern char map_name[FILENAME_MAX];
