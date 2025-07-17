/*-
 * Copyright (c) 2023 Dag-Erling Smørgrav
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

#include <err.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <cryb/test.h>

#include <security/pam_appl.h>
#include <security/openpam.h>

#include "openpam_impl.h"

#define T_FUNC(n, d)							\
	static const char *t_ ## n ## _desc = d;			\
	static int t_ ## n ## _func(OPENPAM_UNUSED(char **desc),	\
	    OPENPAM_UNUSED(void *arg))

#define T(n)								\
	t_add_test(&t_ ## n ## _func, NULL, "%s", t_ ## n ## _desc)

const char *pam_return_so;

T_FUNC(final_percent, "template ends with %")
{
	char template[] = "test%\0deadbeef";
	char buf[] = "Squeamish Ossifrage";
	size_t bufsize = sizeof(buf);
	int pam_err, ret;

	pam_err = openpam_subst(NULL, buf, &bufsize, template);
	ret = (pam_err == PAM_SUCCESS);
	ret &= t_compare_sz(sizeof("test%"), bufsize);
	ret &= t_compare_str("test%", buf);
	return (ret);
}


/***************************************************************************
 * Boilerplate
 */

static int
t_prepare(int argc, char *argv[])
{

	(void)argc;
	(void)argv;

	if ((pam_return_so = getenv("PAM_RETURN_SO")) == NULL) {
		t_printv("define PAM_RETURN_SO before running these tests\n");
		return (0);
	}

	openpam_set_feature(OPENPAM_RESTRICT_MODULE_NAME, 0);
	openpam_set_feature(OPENPAM_VERIFY_MODULE_FILE, 0);
	openpam_set_feature(OPENPAM_RESTRICT_SERVICE_NAME, 0);
	openpam_set_feature(OPENPAM_VERIFY_POLICY_FILE, 0);
	openpam_set_feature(OPENPAM_FALLBACK_TO_OTHER, 0);

	T(final_percent);

	return (0);
}

int
main(int argc, char *argv[])
{

	t_main(t_prepare, NULL, argc, argv);
}
