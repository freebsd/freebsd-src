#ifndef _LINUX_NLS_H
#define _LINUX_NLS_H

#include <linux/init.h>

/* unicode character */
typedef __u16 wchar_t;

struct nls_table {
	char *charset;
	int (*uni2char) (wchar_t uni, unsigned char *out, int boundlen);
	int (*char2uni) (const unsigned char *rawstring, int boundlen,
			 wchar_t *uni);
	unsigned char *charset2lower;
	unsigned char *charset2upper;
	struct module *owner;
	struct nls_table *next;
};

/* this value hold the maximum octet of charset */
#define NLS_MAX_CHARSET_SIZE 6 /* for UTF-8 */

/* nls.c */
extern int register_nls(struct nls_table *);
extern int unregister_nls(struct nls_table *);
extern struct nls_table *load_nls(char *);
extern void unload_nls(struct nls_table *);
extern struct nls_table *load_nls_default(void);

extern int utf8_mbtowc(wchar_t *, const __u8 *, int);
extern int utf8_mbstowcs(wchar_t *, const __u8 *, int);
extern int utf8_wctomb(__u8 *, wchar_t, int);
extern int utf8_wcstombs(__u8 *, const wchar_t *, int);

#endif /* _LINUX_NLS_H */

