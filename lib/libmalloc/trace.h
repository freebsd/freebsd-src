#ifndef __TRACE_H__
#define __TRACE_H__
extern void __m_install_record proto((univptr_t, const char *));
extern void __m_delete_record proto((univptr_t));

#define RECORD_FILE_AND_LINE(addr, fname, linenum) \
	if (_malloc_leaktrace) { \
		(void) sprintf(_malloc_statsbuf, "%s:%d:", fname, linenum); \
		__m_install_record(addr, _malloc_statsbuf); \
	} else \
		_malloc_leaktrace += 0

#define DELETE_RECORD(addr) \
	if (_malloc_leaktrace) \
		__m_delete_record(addr); \
	else \
		_malloc_leaktrace += 0

#endif /* __TRACE_H__ */ /* Do not add anything after this line */
