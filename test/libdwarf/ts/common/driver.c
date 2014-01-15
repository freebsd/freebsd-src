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
 * $Id: driver.c 2121 2011-11-09 08:43:56Z jkoshy $
 */

#include <sys/types.h>
#include <sys/queue.h>
#include <dirent.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <err.h>
#include <unistd.h>
#ifdef	__FreeBSD__
#include <bsdxml.h>
#else
#include <expat.h>
#endif

#include "driver.h"
#include "tet_api.h"

#ifndef	TCGEN
#define	_XML_BUFSIZE	8192
#define	_XML_DATABUFSZ	65536
struct _drv_vc {
	const char *var;
	union {
		uint64_t u64;
		int64_t i64;
		char *str;
		struct {
			char *data;
			int len;
		} b;
	} v;
	enum {
		_VTYPE_NONE,
		_VTYPE_INT,
		_VTYPE_UINT,
		_VTYPE_STRING,
		_VTYPE_BLOCK,
	} vt;
	enum {
		_OP_EQ,
		_OP_NE,
	} op;
	enum {
		_FAIL_CONTINUE,
		_FAIL_ABORT,
	} fail;
	STAILQ_ENTRY(_drv_vc) next;
};
struct _drv_tp {
	struct dwarf_tp *dtp;
	int testnum;
	STAILQ_HEAD(, _drv_vc) vclist;
	STAILQ_ENTRY(_drv_tp) next;
};
struct _drv_ic {
	const char *file;
	int tpcnt;
	STAILQ_HEAD(, _drv_tp) tplist;
	STAILQ_ENTRY(_drv_ic) next;
};
extern int ic_count;
static STAILQ_HEAD(, _drv_ic) _iclist;
static struct _drv_ic *_cur_ic = NULL;
static struct _drv_tp *_cur_tp = NULL;
static struct _drv_vc *_cur_vc = NULL;
static char _xml_buf[_XML_BUFSIZE];
static char _xml_data[_XML_DATABUFSZ];
static int _xml_data_pos = 0;
static int _test_cnt = 0;
#else
FILE *_cur_fp = NULL;
#endif	/* !TCGEN */

/* The name of the file currently being processed. */
const char *_cur_file = NULL;

static void driver_startup(void);
static void driver_cleanup(void);
static  __attribute__ ((unused)) char * driver_string_encode(const char *str);
#ifndef	TCGEN
static void driver_base64_decode(const char *code, int codesize, char **plain,
    int *plainsize);
#else
static __attribute__ ((unused)) void driver_base64_encode(const char *plain,
    int plainsize, char **code, int *codesize);
#endif	/* !TCGEN */

void (*tet_startup)(void) = driver_startup;
void (*tet_cleanup)(void) = driver_cleanup;

/*
 * Functions used by TCM for supporting a dynamic test case.
 */

#ifndef	TCGEN
static struct _drv_ic *
_find_ic(int icnum)
{
	struct _drv_ic *ic;
	int i;

	for (i = 1, ic = STAILQ_FIRST(&_iclist);
	     i < icnum && ic != NULL;
	     i++, ic = STAILQ_NEXT(ic, next))
		;

	return (ic);
}

static struct _drv_tp *
_find_tp(int icnum, int tpnum)
{
	struct _drv_ic *ic;
	struct _drv_tp *tp;
	int i;

	ic = _find_ic(icnum);
	assert(ic != NULL);
	for (i = 1, tp = STAILQ_FIRST(&ic->tplist);
	     i < tpnum && tp != NULL;
	     i++, tp = STAILQ_NEXT(tp, next))
		;

	return (tp);
}
#endif	/* !TCGEN */

int
tet_getminic(void)
{

	return (1);		/* IC start with 1. */
}

int
tet_getmaxic(void)
{

#ifdef	TCGEN
	return (1);
#else
	return (ic_count);
#endif	/* TCGEN */
}

int
tet_isdefic(int icnum)
{

#ifdef	TCGEN
	assert(icnum == 1);
	return (1);
#else
	if (icnum >= 1 && icnum <= ic_count)
		return (1);

	return (0);
#endif	/* TCGEN */
}

int
tet_gettpcount(int icnum)
{
#ifdef	TCGEN
	assert(icnum == 1);
	return (1);
#else
	struct _drv_ic *ic;

	ic = _find_ic(icnum);
	assert(ic != NULL);

	return (ic->tpcnt);
#endif	/* TCGEN */
}

int
tet_gettestnum(int icnum, int tpnum)
{
#ifdef	TCGEN
	assert(icnum == 1 && tpnum == 1);
	return (1);
#else
	struct _drv_tp *tp;

	tp = _find_tp(icnum, tpnum);
	assert(tp != NULL);

	return (tp->testnum);
#endif	/* TCGEN */
}

int
tet_invoketp(int icnum, int tpnum)
{
#ifdef	TCGEN
	assert(icnum == 1 && tpnum == 1);
	return (0);
#else
	struct _drv_ic *ic;
	struct _drv_tp *tp;

	ic = _find_ic(icnum);
	assert(ic != NULL);
	_cur_ic = ic;
	_cur_file = _cur_ic->file;
	tp = _find_tp(icnum, tpnum);
	assert(tp != NULL && tp->dtp != NULL);
	tet_printf("Start Test Purpose <%s> on <%s>\n", tp->dtp->tp_name,
	    _cur_ic->file);
	_cur_vc = STAILQ_FIRST(&tp->vclist);
	tp->dtp->tp_func();
	
	return (0);
#endif	/* TCGEN */
}

#ifndef	TCGEN
static void
_xml_start_cb(void *data, const char *el, const char **attr)
{
	XML_Parser p;
	int i, j;

	p = data;

	if (!strcmp(el, "ic")) {
		if (_cur_ic != NULL)
			errx(EXIT_FAILURE, "Nested IC at line %jd",
			    (intmax_t) XML_GetCurrentLineNumber(p));
		_cur_ic = calloc(1, sizeof(*_cur_ic));
		STAILQ_INIT(&_cur_ic->tplist);
		if (_cur_ic == NULL)
			err(EXIT_FAILURE, "calloc");
		for (i = 0; attr[i]; i += 2) {
			if (!strcmp(attr[i], "file")) {
				_cur_ic->file = strdup(attr[i + 1]);
				if (_cur_ic->file == NULL)
					err(EXIT_FAILURE, "strdup");
				break;
			}
		}
		if (_cur_ic->file == NULL)
			errx(EXIT_FAILURE, "IC without 'file' attribute "
			    "at line %jd",
			    (intmax_t) XML_GetCurrentLineNumber(p));

	} else if (!strcmp(el, "tp")) {
		if (_cur_ic == NULL)
			errx(EXIT_FAILURE, "TP without containing IC at "
			    "line %jd",
			    (intmax_t) XML_GetCurrentLineNumber(p));
		if (_cur_tp != NULL)
			errx(EXIT_FAILURE, "Nested TP at line %jd",
			    (intmax_t) XML_GetCurrentLineNumber(p));
		_cur_tp = calloc(1, sizeof(*_cur_tp));
		STAILQ_INIT(&_cur_tp->vclist);
		if (_cur_tp == NULL)
			err(EXIT_FAILURE, "calloc");
		for (i = 0; attr[i]; i += 2) {
			if (!strcmp(attr[i], "func")) {
				for (j = 0; dwarf_tp_array[j].tp_name != NULL;
				     j++)
					if (!strcmp(attr[i + 1],
					    dwarf_tp_array[j].tp_name)) {
						_cur_tp->dtp =
						    &dwarf_tp_array[j];
						break;
					}
				if (_cur_tp->dtp == NULL)
					errx(EXIT_FAILURE,
					    "TP function '%s' not found",
					    attr[i]);
				break;
			}
		}
		if (_cur_tp->dtp == NULL)
			errx(EXIT_FAILURE,
			    "TP without 'func' attribute at line %jd",
			    (intmax_t) XML_GetCurrentLineNumber(p));

	} else if (!strcmp(el, "vc")) {
		if (_cur_tp == NULL)
			errx(EXIT_FAILURE,
			    "VC without containing IC at line %jd",
			    (intmax_t) XML_GetCurrentLineNumber(p));
		if (_cur_vc != NULL)
			errx(EXIT_FAILURE, "Nested VC at line %jd",
			    (intmax_t) XML_GetCurrentLineNumber(p));
		_cur_vc = calloc(1, sizeof(*_cur_vc));
		
		_cur_vc->op = _OP_EQ;
		_cur_vc->fail = _FAIL_CONTINUE;
		if (_cur_vc == NULL)
			err(EXIT_FAILURE, "calloc");
		for (i = 0; attr[i]; i += 2) {
			if (!strcmp(attr[i], "var")) {
				_cur_vc->var = strdup(attr[i + 1]);
				if (_cur_vc->var == NULL)
					err(EXIT_FAILURE, "strdup");
			} else if (!strcmp(attr[i], "type")) {
				if (!strcmp(attr[i + 1], "int"))
					_cur_vc->vt = _VTYPE_INT;
				else if (!strcmp(attr[i + 1], "uint"))
					_cur_vc->vt = _VTYPE_UINT;
				else if (!strcmp(attr[i + 1], "str"))
					_cur_vc->vt = _VTYPE_STRING;
				else if (!strcmp(attr[i + 1], "block"))
					_cur_vc->vt = _VTYPE_BLOCK;
				else
					errx(EXIT_FAILURE,
					    "Unknown value type %s at "
					    "line %jd", attr[i + 1],
					    (intmax_t) XML_GetCurrentLineNumber(p));
			} else if (!strcmp(attr[i], "op")) {
				if (!strcmp(attr[i + 1], "ne"))
					_cur_vc->op = _OP_NE;
			} else if (!strcmp(attr[i], "fail")) {
				if (!strcmp(attr[i + 1], "abort"))
					_cur_vc->fail = _FAIL_ABORT;
			} else
				errx(EXIT_FAILURE,
				    "Unknown attr %s at line %jd",
				    attr[i],
				    (intmax_t) XML_GetCurrentLineNumber(p));
		}
		if (_cur_vc->var == NULL || _cur_vc->vt == _VTYPE_NONE)
			errx(EXIT_FAILURE,
			    "VC without 'var' or 'type' attribute at line %jd",
			    (intmax_t) XML_GetCurrentLineNumber(p));
	} else
		errx(EXIT_FAILURE, "Unknown element %s at line %jd", el,
		    (intmax_t) XML_GetCurrentLineNumber(p));
}

static void
_xml_end_cb(void *data, const char *el)
{
	XML_Parser p;

	p = data;

	if (!strcmp(el, "ic")) {
		if (_cur_ic == NULL)
			errx(EXIT_FAILURE, "bogus IC end tag at line %jd",
			    (intmax_t) XML_GetCurrentLineNumber(p));
		STAILQ_INSERT_TAIL(&_iclist, _cur_ic, next);
		_cur_ic = NULL;
	} else if (!strcmp(el, "tp")) {
		if (_cur_tp == NULL)
			errx(EXIT_FAILURE, "bogus TP end tag at line %jd",
			    (intmax_t) XML_GetCurrentLineNumber(p));
		assert(_cur_ic != NULL);
		_test_cnt++;
		_cur_tp->testnum = _test_cnt;
		STAILQ_INSERT_TAIL(&_cur_ic->tplist, _cur_tp, next);
		_cur_ic->tpcnt++;
		_cur_tp = NULL;
	} else if (!strcmp(el, "vc")) {
		if (_cur_vc == NULL)
			errx(EXIT_FAILURE, "bogus VC end tag at line %jd",
			    (intmax_t) XML_GetCurrentLineNumber(p));
		if (_xml_data_pos == 0 && _cur_vc->vt != _VTYPE_STRING)
			errx(EXIT_FAILURE,
			    "VC element without value defined at line %jd",
			    (intmax_t) XML_GetCurrentLineNumber(p));
		_xml_data[_xml_data_pos] = '\0';
		switch (_cur_vc->vt) {
		case _VTYPE_INT:
			_cur_vc->v.i64 = strtoimax(_xml_data, NULL, 0);
			break;
		case _VTYPE_UINT:
			_cur_vc->v.u64 = strtoumax(_xml_data, NULL, 0);
			break;
		case _VTYPE_STRING:
			_cur_vc->v.str = strdup(_xml_data);
			if (_cur_vc->v.str == NULL)
				err(EXIT_FAILURE, "strdup");
			break;
		case _VTYPE_BLOCK:
			driver_base64_decode(_xml_data, _xml_data_pos,
			    &_cur_vc->v.b.data, &_cur_vc->v.b.len);
			break;
		default:
			assert(0);
			break;
		}
		_xml_data_pos = 0;

		assert(_cur_tp != NULL);
		STAILQ_INSERT_TAIL(&_cur_tp->vclist, _cur_vc, next);
		_cur_vc = NULL;
	}
}

#define	_VALUE_BUFSIZE	1024

static void
_xml_data_cb(void *data, const char *s, int len)
{

	(void) data;

	if (_cur_vc != NULL) {
		if (_xml_data_pos + len >= _XML_DATABUFSZ) {
			warnx("_xml_data overflowed, data(%d) discarded", len);
			return;
		}
		memcpy(&_xml_data[_xml_data_pos], s, len);
		_xml_data_pos += len;
	}
}

#define	_CMD_SIZE	256

static void
driver_parse_ic_desc(const char *fname)
{
	XML_Parser p;
	ssize_t bytes;
	int fd, final;
	char *xml_name, *ext, *fname0;
	char cmd[_CMD_SIZE];

	if ((fname0 = strdup(fname)) == NULL)
		err(EXIT_FAILURE, "strdup");
	fname0[strlen(fname) - 3] = '\0';
	snprintf(cmd, _CMD_SIZE, "gunzip -f -c %s > %s", fname, fname0);
	if (system(cmd) < 0)
		err(EXIT_FAILURE, "system");

	if ((xml_name = strdup(fname)) == NULL)
		err(EXIT_FAILURE, "strdup");
	ext = strrchr(xml_name, '.');
	assert(ext != NULL);
	*ext = '\0';

	if ((p = XML_ParserCreate(NULL)) == NULL)
		errx(EXIT_FAILURE, "XML_ParserCreate failed");
	XML_SetUserData(p, p);
	XML_SetElementHandler(p, _xml_start_cb, _xml_end_cb);
	XML_SetCharacterDataHandler(p, _xml_data_cb);

	if ((fd = open(xml_name, O_RDONLY)) < 0)
		err(EXIT_FAILURE, "open %s failed", xml_name);

	final = 0;
	for (;;) {
		bytes = read(fd, _xml_buf, _XML_BUFSIZE);
		if (bytes < 0)
			err(EXIT_FAILURE, "read %s failed", xml_name);
		if (bytes == 0)
			final = 1;
		if (!XML_Parse(p, _xml_buf, (int) bytes, final))
			errx(EXIT_FAILURE, "XML_Parse error at line %jd: %s\n",
			    (intmax_t) XML_GetCurrentLineNumber(p),
			    XML_ErrorString(XML_GetErrorCode(p)));
		if (final)
			break;
	}

	free(xml_name);
}

static void
driver_parse_ic(void)
{
	struct dirent *dp;
	DIR *dirp;

	if ((dirp = opendir(".")) == NULL)
		err(EXIT_FAILURE, "opendir");
	while ((dp = readdir(dirp)) != NULL) {
		if (strlen(dp->d_name) <= 7)
			continue;
		if (!strcmp(&dp->d_name[strlen(dp->d_name) - 7], ".xml.gz"))
			driver_parse_ic_desc(dp->d_name);
	}
	(void) closedir(dirp);
}

#else  /* !TCGEN */

static void
driver_gen_tp(FILE *fp, const char *file)
{
	int i;

	assert(fp != NULL);
	for (i = 0; dwarf_tp_array[i].tp_name != NULL; i++) {
		fprintf(fp, "  <tp func='%s'>\n", dwarf_tp_array[i].tp_name);
		_cur_file = file;
		_cur_fp = fp;
		dwarf_tp_array[i].tp_func();
		fprintf(fp, "  </tp>\n");
	}
}

#define	_FILENAME_BUFSIZE	1024
#define	_CMD_SIZE		256

static void
driver_gen_ic(void)
{
	char *flist, *token;
	FILE *fp;
	char nbuf[_FILENAME_BUFSIZE], cmd[_CMD_SIZE];

	flist = getenv("ICLIST");
	if (flist == NULL)
		errx(EXIT_FAILURE,
		    "Driver in TCGEN mode but ICLIST env is not defined");
	if ((flist = strdup(flist)) == NULL)
		err(EXIT_FAILURE, "strdup");
	while ((token = strsep(&flist, ":")) != NULL) {
		snprintf(nbuf, sizeof(nbuf), "%s.xml", token);
		if ((fp = fopen(nbuf, "w")) == NULL)
			err(EXIT_FAILURE, "fopen %s failed", nbuf);
		fprintf(fp, "<ic file='%s'>\n", token);
		driver_gen_tp(fp, token);
		fprintf(fp, "</ic>\n");
		fclose(fp);
		snprintf(cmd, _CMD_SIZE, "gzip -f %s", nbuf);
		if (system(cmd) < 0)
			err(EXIT_FAILURE, "system");
	}
	free(flist);
}

#endif	/* !TCGEN */

#define	_MAX_STRING_SIZE	65535

static char *
driver_string_encode(const char *str)
{
	static char enc[_MAX_STRING_SIZE];
	size_t len;
	int pos;

#define	_ENCODE_STRING(S)	do {			\
	len = strlen(S);				\
	if (pos + len < _MAX_STRING_SIZE) {		\
		strncpy(enc + pos, S, len);		\
		pos += len;				\
	} else {					\
		assert(0);				\
		return (NULL);				\
	}						\
	} while(0)

	pos = 0;
	for (; *str != '\0'; str++) {
		switch (*str) {
		case '"':
			_ENCODE_STRING("&quot;");
			break;
		case '\'':
			_ENCODE_STRING("&apos;");
			break;
		case '<':
			_ENCODE_STRING("&lt;");
			break;
		case '>':
			_ENCODE_STRING("&gt;");
			break;
		case '&':
			_ENCODE_STRING("&amp;");
			break;
		default:
			/* Normal chars. */
			if (pos < _MAX_STRING_SIZE - 1)
				enc[pos++] = *str;
			else {
				enc[pos] = '\0';
				assert(0);
				return (NULL);
			}
			break;
		}
	}
	enc[pos] = '\0';

	return (enc);
#undef _ENCODE_STRING
}

static void
driver_startup(void)
{

#ifdef	TCGEN
	driver_gen_ic();
#else
	STAILQ_INIT(&_iclist);
	driver_parse_ic();
#endif
}

static void
driver_cleanup(void)
{

}

/*
 * Base64 encode/decode utility modified from libb64 project. It's been
 * placed in the public domain. Note that this modified version doesn't
 * emit newline during encoding.
 */

#ifdef	TCGEN

typedef enum
{
	step_A, step_B, step_C
} base64_encodestep;

typedef struct
{
	base64_encodestep step;
	char result;
} base64_encodestate;

static void
base64_init_encodestate(base64_encodestate* state_in)
{
	state_in->step = step_A;
	state_in->result = 0;
}

static char
base64_encode_value(char value_in)
{
	static const char* encoding = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
		"abcdefghijklmnopqrstuvwxyz0123456789+/";

	if (value_in > 63)
		return '=';

	return encoding[(int)value_in];
}

static int
base64_encode_block(const char* plaintext_in, int length_in, char* code_out,
    base64_encodestate* state_in)
{
	const char* plainchar = plaintext_in;
	const char* const plaintextend = plaintext_in + length_in;
	char* codechar = code_out;
	char res;
	char fragment;

	res = state_in->result;

	switch (state_in->step)
	{
		while (1)
		{
	case step_A:
			if (plainchar == plaintextend)
			{
				state_in->result = res;
				state_in->step = step_A;
				return codechar - code_out;
			}
			fragment = *plainchar++;
			res = (fragment & 0x0fc) >> 2;
			*codechar++ = base64_encode_value(res);
			res = (fragment & 0x003) << 4;
	case step_B:
			if (plainchar == plaintextend)
			{
				state_in->result = res;
				state_in->step = step_B;
				return codechar - code_out;
			}
			fragment = *plainchar++;
			res |= (fragment & 0x0f0) >> 4;
			*codechar++ = base64_encode_value(res);
			res = (fragment & 0x00f) << 2;
	case step_C:
			if (plainchar == plaintextend)
			{
				state_in->result = res;
				state_in->step = step_C;
				return codechar - code_out;
			}
			fragment = *plainchar++;
			res |= (fragment & 0x0c0) >> 6;
			*codechar++ = base64_encode_value(res);
			res  = (fragment & 0x03f) >> 0;
			*codechar++ = base64_encode_value(res);
		}
	}
	/* control should not reach here */
	return codechar - code_out;
}

static int
base64_encode_blockend(char* code_out, base64_encodestate* state_in)
{
	char* codechar = code_out;
	
	switch (state_in->step)
	{
	case step_B:
		*codechar++ = base64_encode_value(state_in->result);
		*codechar++ = '=';
		*codechar++ = '=';
		break;
	case step_C:
		*codechar++ = base64_encode_value(state_in->result);
		*codechar++ = '=';
		break;
	case step_A:
		break;
	}
	
	return codechar - code_out;
}

static void
driver_base64_encode(const char *plain, int plainsize, char **code,
    int *codesize)
{
	base64_encodestate state;

	assert(plain != NULL && plainsize > 0);

	*code = malloc(sizeof(char) * plainsize * 2);
	if (*code == NULL)
		err(EXIT_FAILURE, "malloc");

	base64_init_encodestate(&state);

	*codesize = base64_encode_block(plain, plainsize, *code, &state);
	*codesize += base64_encode_blockend(*code + *codesize, &state);
}

#else  /* TCGEN */

typedef enum
{
	step_a, step_b, step_c, step_d
} base64_decodestep;

typedef struct
{
	base64_decodestep step;
	char plainchar;
} base64_decodestate;

static int
base64_decode_value(int value_in)
{
	static const char decoding[] = { 62,-1,-1,-1,63,52,53,54,55,56,57,58,
					 59,60,61,-1,-1,-1,-2,-1,-1,-1,0,1,2,
					 3,4,5,6,7,8,9,10,11,12,13,14,15,16,
					 17,18,19,20,21,22,23,24,25,-1,-1,-1,
					 -1,-1,-1,26,27,28,29,30,31,32,33,34,
					 35,36,37,38,39,40,41,42,43,44,45,46,
					 47,48,49,50,51 };
	static const int decoding_size = sizeof(decoding);

	value_in -= 43;
	if (value_in < 0 || value_in > decoding_size)
		return -1;

	return decoding[value_in];
}

static void
base64_init_decodestate(base64_decodestate* state_in)
{
	state_in->step = step_a;
	state_in->plainchar = 0;
}

static int
base64_decode_block(const char* code_in, const int length_in,
    char* plaintext_out, base64_decodestate* state_in)
{
	const char* codechar = code_in;
	char* plainchar = plaintext_out;
	char fragment;

	*plainchar = state_in->plainchar;
	
	switch (state_in->step)
	{
		while (1) {
	case step_a:
			do {
				if (codechar == code_in+length_in)
				{
					state_in->step = step_a;
					state_in->plainchar = *plainchar;
					return plainchar - plaintext_out;
				}
				fragment = (char)base64_decode_value(*codechar++);
			} while (fragment < 0);
			*plainchar    = (fragment & 0x03f) << 2;
	case step_b:
			do {
				if (codechar == code_in+length_in)
				{
					state_in->step = step_b;
					state_in->plainchar = *plainchar;
					return plainchar - plaintext_out;
				}
				fragment = (char)base64_decode_value(*codechar++);
			} while (fragment < 0);
			*plainchar++ |= (fragment & 0x030) >> 4;
			*plainchar    = (fragment & 0x00f) << 4;
	case step_c:
			do {
				if (codechar == code_in+length_in)
				{
					state_in->step = step_c;
					state_in->plainchar = *plainchar;
					return plainchar - plaintext_out;
				}
				fragment = (char)base64_decode_value(*codechar++);
			} while (fragment < 0);
			*plainchar++ |= (fragment & 0x03c) >> 2;
			*plainchar    = (fragment & 0x003) << 6;
	case step_d:
			do {
				if (codechar == code_in+length_in)
				{
					state_in->step = step_d;
					state_in->plainchar = *plainchar;
					return plainchar - plaintext_out;
				}
				fragment = (char)base64_decode_value(*codechar++);
			} while (fragment < 0);
			*plainchar++   |= (fragment & 0x03f);
		}
	}
	/* control should not reach here */
	return plainchar - plaintext_out;
}

static void
driver_base64_decode(const char *code, int codesize, char **plain, int *plainsize)
{
	base64_decodestate state;

	assert(code != NULL && codesize > 0);

	*plain = malloc(sizeof(char) * codesize);
	if (*plain == NULL)
		err(EXIT_FAILURE, "malloc");

	base64_init_decodestate(&state);

	*plainsize = base64_decode_block(code, codesize, *plain, &state);
}
#endif	/* TCGEN */
