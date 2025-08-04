/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2025 Chelsio Communications, Inc.
 * Written by: John Baldwin <jhb@FreeBSD.org>
 */

#ifndef __LIBUTILPP_HH__
#define	__LIBUTILPP_HH__

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <memory>

namespace freebsd {
	/*
	 * FILE_up is a std::unique_ptr<> for FILE objects which uses
	 * fclose() to destroy the wrapped pointer.
	 */
	struct fclose_deleter {
		void operator() (std::FILE *fp) const
		{
			std::fclose(fp);
		}
	};

	using FILE_up = std::unique_ptr<std::FILE, fclose_deleter>;

	/*
	 * malloc_up<T> is a std::unique_ptr<> which uses free() to
	 * destroy the wrapped pointer.  This can be used to wrap
	 * pointers allocated implicitly by malloc() such as those
	 * returned by strdup().
	 */
	template <class T>
	struct free_deleter {
		void operator() (T *p) const
		{
			std::free(p);
		}
	};

	template <class T>
	using malloc_up = std::unique_ptr<T, free_deleter<T>>;

	/*
	 * Returns a std::string containing the same output as
	 * sprintf().  Throws std::bad_alloc if an error occurs.
	 */
	std::string stringf(const char *fmt, ...) __printflike(1, 2);
	std::string stringf(const char *fmt, std::va_list ap);
}

#endif /* !__LIBUTILPP_HH__ */
