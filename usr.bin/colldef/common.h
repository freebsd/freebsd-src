/*
 * $FreeBSD: src/usr.bin/colldef/common.h,v 1.2.40.1 2010/02/10 00:26:20 kensmith Exp $
 */

#define CHARMAP_SYMBOL_LEN 64
#define BUFSIZE 80

extern int line_no;

extern u_char charmap_table[UCHAR_MAX + 1][CHARMAP_SYMBOL_LEN];
extern char map_name[FILENAME_MAX];
