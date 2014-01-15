/*-
 * Copyright (c) 2008 Hyogeol Lee <hyogeollee@gmail.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $Id: test_nm.c 2378 2012-01-03 08:59:56Z jkoshy $
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <tet_api.h>

static int	exec_cmd(const char *, const char *);
static void	startup();
static void	test_bsd();
static void	test_dynamic();
static void	test_external();
static void	test_hexa();
static bool	test_nm_out(const char *, const char *);
static void	test_no_sort();
static void	test_num_sort();
static void	test_octal();
static void	test_posix();
static void	test_print_filename();
static void	test_print_size();
static void	test_reverse_sort();
static void	test_size_sort();
static void	test_sysv();
static void	test_undef();

struct tet_testlist tet_testlist[] = {
	{ test_dynamic, 1},
	{ test_external, 2},
	{ test_num_sort, 3},
	{ test_no_sort, 4},
	{ test_posix, 5},
	{ test_print_size, 6},
	{ test_undef, 7},
	{ test_size_sort, 8},
	{ test_sysv, 9},
	{ test_bsd, 10},
	{ test_print_filename, 11},
	{ test_octal, 12},
	{ test_hexa, 13},
	{ test_reverse_sort, 14},
	{ NULL, 0}
};

#define	NM_CMD		NM " %s " TESTFILE " > test.out"
#define DIFF_CMD	"diff test.out " TC_DIR "/" TESTFILE "%s.txt > /dev/null"

void (*tet_startup)() = startup;
void (*tet_cleanup)() = NULL;

static int
exec_cmd(const char *cmd, const char *op)
{
	char *this_cmd;
	int rtn;
	size_t cmd_len;

	if (cmd == NULL || op == NULL)
		return (-1);

	cmd_len = strlen(cmd) + strlen(op);

	if ((this_cmd = malloc(sizeof(char) * cmd_len)) == NULL) {
		tet_infoline("cannot allocate memory");

		return (-1);
	}

	snprintf(this_cmd, cmd_len, cmd, op);

	rtn = system(this_cmd);

	free(this_cmd);

	return (rtn);
}

static void
startup()
{

	if (system("cp " TC_DIR "/" TESTFILE " .") < 0) {
		tet_infoline("cannot cp object");
		
		exit(EXIT_FAILURE);
	}
}

static void
test_bsd()
{
	bool rtn = true;

	tet_infoline("OPTION -B, --format=bsd");

	rtn |= test_nm_out("-B", "-B");
	rtn |= test_nm_out("--format=bsd", "-B");

	tet_result(rtn == true ? TET_PASS : TET_FAIL);
}

static void
test_dynamic()
{
	bool rtn = true;

	tet_infoline("OPTION -D, --dynamic");

	rtn |= test_nm_out("-D", "-D");
	rtn |= test_nm_out("--dynamic", "-D");

	tet_result(rtn == true ? TET_PASS : TET_FAIL);
}

static void
test_external()
{

	tet_infoline("OPTION -g");

	if (test_nm_out("-g", "-g") == true)
		tet_result(TET_PASS);
	else
		tet_result(TET_FAIL);
}

static void
test_hexa()
{
	bool rtn = true;

	tet_infoline("OPTION -x, -t x");

	rtn |= test_nm_out("-x", "-x");
	rtn |= test_nm_out("-t x", "-x");
	rtn |= test_nm_out("--radix=x", "-x");

	tet_result(rtn == true ? TET_PASS : TET_FAIL);
}

static bool
test_nm_out(const char *op, const char *d_op)
{
	int rtn;

	if (op == NULL) {
		tet_result(TET_FAIL);

		return (false);
	}

	if ((rtn = exec_cmd(NM_CMD, op)) < 0) {
		tet_infoline("system function failed");

		return (false);
	} else if (rtn == 127) {
		tet_infoline("execution shell failed");

		return (false);
	}

	if ((rtn = exec_cmd(DIFF_CMD, d_op)) < 0)
		tet_infoline("system function failed");
	else {
		switch (rtn) {
		case 127:
			tet_infoline("execution shell failed");

			break;
		case 2:
			tet_infoline("diff has trouble");

			break;
		case 1:
			tet_infoline("output is different");

			break;
		case 0:
			return (true);
		}
	}

	return (false);
}

static void
test_no_sort()
{
	bool rtn = true;

	tet_infoline("OPTION -p");

	rtn |= test_nm_out("-p", "-p");
	rtn |= test_nm_out("--no-sort", "-p");

	tet_result(rtn == true ? TET_PASS : TET_FAIL);
}

static void
test_num_sort()
{
	bool rtn = true;

	tet_infoline("OPTION -n, --numeric-sort");

	rtn |= test_nm_out("-n", "-n");
	rtn |= test_nm_out("--numeric-sort", "-n");

	tet_result(rtn == true ? TET_PASS : TET_FAIL);
}

static void
test_octal()
{
	bool rtn = true;

	tet_infoline("OPTION --radix=o, -t o");

	rtn |= test_nm_out("-t o", "-o");
	rtn |= test_nm_out("--radix=o", "-o");

	tet_result(rtn == true ? TET_PASS : TET_FAIL);
}

static void
test_posix()
{
	bool rtn = true;

	tet_infoline("OPTION -P, --format=posix");

	rtn |= test_nm_out("-P", "-P");
	rtn |= test_nm_out("--format=posix", "-P");

	tet_result(rtn == true ? TET_PASS : TET_FAIL);
}

static void
test_print_filename()
{
	bool rtn = true;

	tet_infoline("OPTION -A, --print-file-name");

	rtn |= test_nm_out("-A", "-A");
	rtn |= test_nm_out("--print-file-name", "-A");

	tet_result(rtn == true ? TET_PASS : TET_FAIL);
}

static void
test_print_size()
{
	bool rtn = true;

	tet_infoline("OPTION -S, --print-size");

	rtn |= test_nm_out("-S", "-S");
	rtn |= test_nm_out("--print-size", "-S");

	tet_result(rtn == true ? TET_PASS : TET_FAIL);
}

static void
test_reverse_sort()
{
	bool rtn = true;

	tet_infoline("OPTION -r, --reverse-sort");

	rtn |= test_nm_out("-r", "-r");
	rtn |= test_nm_out("--reverse-sort", "-r");

	rtn |= test_nm_out("-r -n", "-r-n");
	rtn |= test_nm_out("-r -p", "-r-p");
	rtn |= test_nm_out("-r --size-sort", "-r-size-sort");

	tet_result(rtn == true ? TET_PASS : TET_FAIL);
}

static void
test_size_sort()
{

	tet_infoline("OPTION --size-sort");

	if (test_nm_out("--size-sort", "-size-sort") == true)
		tet_result(TET_PASS);
	else
		tet_result(TET_FAIL);
}

static void
test_sysv()
{

	tet_infoline("OPTION --format=sysv");

	if (test_nm_out("--format=sysv", "-sysv") == true)
		tet_result(TET_PASS);
	else
		tet_result(TET_FAIL);
}

static void
test_undef()
{
	bool rtn = true;

	tet_infoline("OPTION -u, --undefined-only");

	rtn |= test_nm_out("-u", "-u");
	rtn |= test_nm_out("--undefined-only", "-u");

	tet_result(rtn == true ? TET_PASS : TET_FAIL);
}
