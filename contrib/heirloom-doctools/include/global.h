#ifdef HAVE_STRLCPY
# define n_strcpy(dst, src, dstsize) strlcpy(dst, src, dstsize)
#else
# define n_strcpy(dst, src, dstsize) strcpy(dst, src)
#endif

#ifdef HAVE_STRLCAT
# define n_strcat(dst, src, dstsize) strlcat(dst, src, dstsize)
#else
# define n_strcat(dst, src, dstsize) strcat(dst, src)
#endif

#ifdef HAVE_WCSLCPY
# define n_wcscpy(dst, src, size   ) wcslcpy(dst, src, size   )
#else
# define n_wcscpy(dst, src, size   ) wcscpy(dst, src)
#endif

#define min(x,y) ((x) < (y) ? (x) : (y))
#define max(x,y) ((x) > (y) ? (x) : (y))
#define prefix(str, pfx) (strncmp(pfx, str, strlen(pfx)) == 0)

#undef __unused
#define __unused __attribute__((unused))

#ifdef	__GLIBC__
#ifdef	_IO_getc_unlocked
#undef	getc
#define	getc(f)		_IO_getc_unlocked(f)
#endif
#ifdef	_IO_putc_unlocked
#undef	putc
#undef	putchar
#define	putc(c, f)	_IO_putc_unlocked(c, f)
#define	putchar(c)	_IO_putc_unlocked(c, stdout)
#endif
#endif
