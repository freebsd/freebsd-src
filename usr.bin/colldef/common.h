/*
 * $FreeBSD$
 */

#define CHARMAP_SYMBOL_LEN 64

extern int line_no;

extern u_char charmap_table[UCHAR_MAX + 1][CHARMAP_SYMBOL_LEN];
extern char map_name[FILENAME_MAX];
