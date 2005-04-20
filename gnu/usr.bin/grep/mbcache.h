/* $FreeBSD$ */
#ifndef MB_CACHE_DEFINED
#define MB_CACHE_DEFINED
struct mb_cache
{
  size_t len;
  const char *orig_buf; /* not the only reference; do not free */
  wchar_t *wcs_buf;
  unsigned char *mblen_buf;
};
#endif
