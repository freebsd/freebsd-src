/*-
 * Copyright (c) 2021 Dag-Erling Smørgrav
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
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
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
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <sys/types.h>

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>

#include <cryb/test.h>

#include <security/pam_appl.h>
#include "openpam_impl.h"

static int
t_straddch_empty(char **desc CRYB_UNUSED, void *arg CRYB_UNUSED)
{
	char *str;
	size_t size, len;
	int ret;

	str = NULL;
	size = len = SIZE_MAX;
	ret = t_is_zero_i(openpam_straddch(&str, &size, &len, '\0'));
	ret &= t_is_not_null(str);
	ret &= t_is_not_zero_sz(size);
	ret &= t_is_zero_sz(len);
	free(str);
	return ret;
}

static int
t_straddch_alloc_fail(char **desc CRYB_UNUSED, void *arg CRYB_UNUSED)
{
	char *str;
	size_t size, len;
	int ret;

	str = NULL;
	size = len = SIZE_MAX;
	errno = 0;
	t_malloc_fail = 1;
	ret = t_compare_i(-1, openpam_straddch(&str, &size, &len, '\0'));
	t_malloc_fail = 0;
	ret &= t_compare_i(ENOMEM, errno);
	ret &= t_is_null(str);
	ret &= t_compare_sz(SIZE_MAX, size);
	ret &= t_compare_sz(SIZE_MAX, len);
	free(str);
	return ret;
}

static int
t_straddch_realloc_fail(char **desc CRYB_UNUSED, void *arg CRYB_UNUSED)
{
	char *str, *_str;
	size_t size, _size, len, _len;
	int i, ret;

	// start with an empty string
	str = NULL;
	size = len = SIZE_MAX;
	ret = t_is_zero_i(openpam_straddch(&str, &size, &len, '\0'));
	ret &= t_is_not_null(str);
	ret &= t_is_not_zero_sz(size);
	ret &= t_is_zero_sz(len);
	if (!ret)
		goto end;
	// repeatedly append to it until allocation fails
	errno = 0;
	_str = str;
	_size = size;
	_len = len;
	t_malloc_fail = 1;
	for (i = 0; i < 4096; i++) {
		if ((ret = openpam_straddch(&str, &size, &len, 'x')) != 0)
			break;
		_size = size;
		_len = len;
	}
	t_malloc_fail = 0;
	ret = t_compare_i(-1, ret);
	ret &= t_compare_i(ENOMEM, errno);
	ret &= t_compare_ptr(_str, str);
	ret &= t_compare_sz(_size, size);
	ret &= t_compare_sz(_len, len);
end:
	free(str);
	return ret;
}

static int
t_straddch_realloc_ok(char **desc CRYB_UNUSED, void *arg CRYB_UNUSED)
{
	char *str;
	size_t size, _size, len, _len;
	int i, ret;

	// start with an empty string
	str = NULL;
	size = len = SIZE_MAX;
	ret = t_is_zero_i(openpam_straddch(&str, &size, &len, '\0'));
	ret &= t_is_not_null(str);
	ret &= t_is_not_zero_sz(size);
	ret &= t_is_zero_sz(len);
	if (!ret)
		goto end;
	// repeatedly append to it until size changes
	_size = size;
	_len = len;
	for (i = ' '; i <= '~'; i++) { // assume ascii
		if ((ret = openpam_straddch(&str, &size, &len, i)) != 0)
			break;
		if (size != _size)
			break;
		if (len != _len + 1)
			break;
		_len = len;
	}
	ret = t_is_zero_i(ret);
	if (!ret)
		goto end;
	ret &= t_compare_sz(_len + 1, len);
	ret &= t_compare_sz(_size * 2, size);
	ret &= t_compare_i(i, str[_len]);
	ret &= t_is_zero_i(str[len]);
end:
	free(str);
	return ret;
}


/***************************************************************************
 * Boilerplate
 */

static int
t_prepare(int argc CRYB_UNUSED, char *argv[] CRYB_UNUSED)
{

	t_add_test(t_straddch_empty, NULL, "empty string");
	t_add_test(t_straddch_alloc_fail, NULL, "allocation failure");
	t_add_test(t_straddch_realloc_fail, NULL, "reallocation failure");
	t_add_test(t_straddch_realloc_ok, NULL, "reallocation success");
	return (0);
}

int
main(int argc, char *argv[])
{

	t_main(t_prepare, NULL, argc, argv);
}
