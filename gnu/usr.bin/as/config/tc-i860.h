/*
 * This file is tc-i860.h.
 */

#define TC_I860 1

#define NO_LISTING

#ifdef OLD_GAS
#define REVERSE_SORT_RELOCS
#endif /* OLD_GAS */

#define tc_headers_hook(a)		{;} /* not used */
#define tc_crawl_symbol_chain(a)	{;} /* not used */
#define tc_aout_pre_write_hook(x)	{;} /* not used */

/*
 * Local Variables:
 * comment-column: 0
 * fill-column: 131
 * End:
 */

/* end of tc-i860.h */
