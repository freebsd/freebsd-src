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
