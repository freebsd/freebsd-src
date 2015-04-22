/*
 * include/ssl_applink.c -- common NTP code for openssl/applink.c
 *
 * Each program which uses OpenSSL should include this file in _one_
 * of its source files and call ssl_applink() before any OpenSSL
 * functions.
 */

#if defined(OPENSSL) && defined(SYS_WINNT)
# ifdef _MSC_VER
#  pragma warning(push)
#  pragma warning(disable: 4152)
# endif
# include <openssl/applink.c>
# ifdef _MSC_VER
#  pragma warning(pop)
# endif
#endif

#if defined(OPENSSL) && defined(_MSC_VER) && defined(_DEBUG)
#define WRAP_DBG_MALLOC
#endif

#ifdef WRAP_DBG_MALLOC
void *wrap_dbg_malloc(size_t s, const char *f, int l);
void *wrap_dbg_realloc(void *p, size_t s, const char *f, int l);
void wrap_dbg_free(void *p);
#endif


#if defined(OPENSSL) && defined(SYS_WINNT)
void ssl_applink(void);

void
ssl_applink(void)
{
#ifdef WRAP_DBG_MALLOC
	CRYPTO_set_mem_ex_functions(wrap_dbg_malloc, wrap_dbg_realloc, wrap_dbg_free);
#else
	CRYPTO_malloc_init();
#endif
}
#else	/* !OPENSSL || !SYS_WINNT */
#define ssl_applink()	do {} while (0)
#endif


#ifdef WRAP_DBG_MALLOC
/*
 * OpenSSL malloc overriding uses different parameters
 * for DEBUG malloc/realloc/free (lacking block type).
 * Simple wrappers convert.
 */
void *wrap_dbg_malloc(size_t s, const char *f, int l)
{
	void *ret;

	ret = _malloc_dbg(s, _NORMAL_BLOCK, f, l);
	return ret;
}

void *wrap_dbg_realloc(void *p, size_t s, const char *f, int l)
{
	void *ret;

	ret = _realloc_dbg(p, s, _NORMAL_BLOCK, f, l);
	return ret;
}

void wrap_dbg_free(void *p)
{
	_free_dbg(p, _NORMAL_BLOCK);
}
#endif	/* WRAP_DBG_MALLOC */
