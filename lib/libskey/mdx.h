#ifdef MD5
/* S/Key can use MD5 now, if defined... */
#include <md5.h>

#define	MDXFinal	MD5Final
#define	MDXInit		MD5Init
#define	MDXUpdate	MD5Update
#define	MDX_CTX		MD5_CTX
#else

/* By default, use MD4 for compatibility */
#include <md4.h>

#define	MDXFinal	MD4Final
#define	MDXInit		MD4Init
#define	MDXUpdate	MD4Update
#define	MDX_CTX		MD4_CTX

#endif
