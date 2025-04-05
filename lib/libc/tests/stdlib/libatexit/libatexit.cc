/*
 * Copyright (C) 2025 Kyle Evans <kevans@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 */

#include <unistd.h>

static int exit_code = -1;
static bool fatal_atexit;

extern "C" {
	void set_fatal_atexit(bool);
	void set_exit_code(int);
}

void
set_fatal_atexit(bool fexit)
{
	fatal_atexit = fexit;
}

void
set_exit_code(int code)
{
	exit_code = code;
}

struct other_object {
	~other_object() {

		/*
		 * In previous versions of our __cxa_atexit handling, we would
		 * never actually execute this handler because it's added during
		 * ~object() below; __cxa_finalize would never revisit it.  We
		 * will allow the caller to configure us to exit with a certain
		 * exit code so that it can run us twice: once to ensure we
		 * don't crash at the end, and again to make sure the handler
		 * actually ran.
		 */
		if (exit_code != -1)
			_exit(exit_code);
	}
};

void
create_staticobj()
{
	static other_object obj;
}

struct object {
	~object() {
		/*
		 * If we're doing the fatal_atexit behavior (i.e., create an
		 * object that will add its own dtor for __cxa_finalize), then
		 * we don't exit here.
		 */
		if (fatal_atexit)
			create_staticobj();
		else if (exit_code != -1)
			_exit(exit_code);
	}
};

static object obj;
