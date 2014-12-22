/*-
 * Copyright (c) 2010 Kai Wang
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $Id: driver.h 3074 2014-06-23 03:08:53Z kaiwang27 $
 */

#ifndef	_DRIVER_H_
#define	_DRIVER_H_

struct dwarf_tp {
	const char *tp_name;
	void (*tp_func)(void);
};

#define	TS_DWARF_INIT(D,FD,DE) do {					\
	(D) = NULL;							\
	if (((FD) = open(_cur_file, O_RDONLY)) < 0) {			\
		tet_printf("open %s failed; %s", _cur_file,		\
		    strerror(errno));					\
		result = TET_FAIL;					\
		goto done;						\
	}								\
	if (dwarf_init((FD), DW_DLC_READ, NULL, NULL, &(D), &(DE)) !=	\
	    DW_DLV_OK) {						\
		tet_printf("dwarf_init failed: %s", dwarf_errmsg((DE)));\
		result = TET_FAIL;					\
		goto done;						\
	}								\
	} while (0)

#define	TS_DWARF_FINISH(D,DE) do {					\
	if (dwarf_finish((D), &(DE)) != DW_DLV_OK) {			\
		tet_printf("dwarf_finish failed: %s",			\
		    dwarf_errmsg((DE)));				\
		result = TET_FAIL;					\
	}								\
	} while (0)

#define	TS_DWARF_CU_FOREACH(D,N,DE)					\
	while (dwarf_next_cu_header((D), NULL, NULL, NULL, NULL, &(N),	\
	    &(DE)) == DW_DLV_OK)

#define	TS_DWARF_CU_FOREACH2(D,I,N,DE)					\
	while (dwarf_next_cu_header_c((D), (I), NULL, NULL, NULL, NULL,	\
	    NULL, NULL, NULL, NULL, &(N), &(DE)) == DW_DLV_OK)

#define	TS_DWARF_DIE_TRAVERSE(D,CB)					\
	_die_traverse((D), (CB))

#define	TS_DWARF_DIE_TRAVERSE2(D,I,CB)					\
	_die_traverse2((D), (I), (CB))

#ifndef	TCGEN

#define	_TS_CHECK_VAR(X,S) do {						\
	struct _drv_vc *_next_vc;					\
	int skip = 0;							\
	if (strcmp(_cur_vc->var, S)) {					\
		tet_printf("VC var(%s) does not match %s, possibly"	\
		    " caused by the skip of previous VCs, try finding"	\
		    " the next var with maching name", _cur_vc->var,	\
		    S);							\
		_next_vc = _cur_vc;					\
		do {							\
			_next_vc = STAILQ_NEXT(_next_vc, next);		\
			skip++;						\
			if (!strcmp(_next_vc->var, S))			\
				break;					\
		} while (_next_vc != NULL);				\
		if (_next_vc != NULL) {					\
			tet_printf("skipped %d VC(s)\n", skip);		\
			_cur_vc = _next_vc;				\
		}							\
	}								\
	} while (0)

#define	TS_CHECK_INT(X)	do {						\
	assert(_cur_vc != NULL);					\
	_TS_CHECK_VAR(X,#X);						\
	if (X != _cur_vc->v.i64) {					\
		tet_printf("assertion %s(%jd) == %jd failed",		\
		    _cur_vc->var, (intmax_t) (X),			\
		    (intmax_t) _cur_vc->v.i64);				\
		result = TET_FAIL;					\
	}								\
	_cur_vc = STAILQ_NEXT(_cur_vc, next);				\
	} while (0)

#define	TS_CHECK_UINT(X) do {						\
	assert(_cur_vc != NULL);					\
	_TS_CHECK_VAR(X,#X);						\
	if (X != _cur_vc->v.u64) {					\
		tet_printf("assertion %s(%ju) == %ju failed",		\
		    _cur_vc->var, (uintmax_t) (X),			\
		    (uintmax_t) _cur_vc->v.u64);			\
		result = TET_FAIL;					\
	}								\
	_cur_vc = STAILQ_NEXT(_cur_vc, next);				\
	} while (0)

#define	TS_CHECK_STRING(X) do {						\
	assert(_cur_vc != NULL);					\
	_TS_CHECK_VAR(X,#X);						\
	if (strcmp(X, _cur_vc->v.str)) {				\
		tet_printf("assertion %s('%s') == '%s' failed",		\
		    _cur_vc->var, (X), _cur_vc->v.str);			\
		result = TET_FAIL;					\
	}								\
	_cur_vc = STAILQ_NEXT(_cur_vc, next);				\
	} while (0)

#define	TS_CHECK_BLOCK(B,S) do {					\
	assert(_cur_vc != NULL);					\
	_TS_CHECK_VAR(B,#B);						\
	if ((S) != _cur_vc->v.b.len ||					\
	    memcmp((B), _cur_vc->v.b.data, _cur_vc->v.b.len)) {		\
		tet_printf("assertion block %s failed\n", _cur_vc->var);\
		result = TET_FAIL;					\
	}								\
	_cur_vc = STAILQ_NEXT(_cur_vc, next);				\
	} while (0)

#define	TS_RESULT(X) tet_result(X)

#else  /* !TCGEN */

#define	TS_CHECK_INT(X) do {						\
	fprintf(_cur_fp, "    <vc var='%s' type='int'>%jd</vc>\n", #X,	\
	    (intmax_t) (X));						\
	} while (0)

#define	TS_CHECK_UINT(X) do {						\
	fprintf(_cur_fp, "    <vc var='%s' type='uint'>%ju</vc>\n", #X,	\
	    (uintmax_t)(X));						\
	} while (0)

#define	TS_CHECK_STRING(X) do {						\
	fprintf(_cur_fp, "    <vc var='%s' type='str'>%s</vc>\n", #X,	\
	    driver_string_encode(X));					\
	} while (0)

#define	TS_CHECK_BLOCK(B,S) do {					\
	char *code;							\
	int codesize;							\
	size_t wsize;							\
	fprintf(_cur_fp, "    <vc var='%s' type='block'>", #B);		\
	driver_base64_encode((char *) (B), (S), &code, &codesize);	\
	wsize = fwrite(code, 1, (size_t) codesize, _cur_fp);		\
	assert(wsize == (size_t) codesize);				\
	fprintf(_cur_fp, "</vc>\n");					\
	free(code);							\
	} while (0)

#define	TS_RESULT(X)

#endif	/* !TCGEN */
#endif	/* !_DRIVER_H_ */
