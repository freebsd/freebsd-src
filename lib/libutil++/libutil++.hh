/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2025 Chelsio Communications, Inc.
 * Written by: John Baldwin <jhb@FreeBSD.org>
 */

#ifndef __LIBUTILPP_HH__
#define	__LIBUTILPP_HH__

#include <sys/nv.h>
#include <libutil.h>
#include <netdb.h>
#include <unistd.h>

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
	 * addrinfo_up is a std::unique_ptr<> which uses
	 * freeaddrinfo() to destroy the wrapped pointer.  It is
	 * intended to wrap arrays allocated by getaddrinfo().
	 */
	struct freeaddrinfo_deleter {
		void operator() (struct addrinfo *ai) const
		{
			freeaddrinfo(ai);
		}
	};

	using addrinfo_up = std::unique_ptr<addrinfo, freeaddrinfo_deleter>;

	/*
	 * This class is intended to function similar to unique_ptr<>,
	 * but it contains a file descriptor rather than a pointer to
	 * an object.  On destruction the descriptor is closed via
	 * close(2).
	 *
	 * Similar to unique_ptr<>, release() returns ownership of the
	 * file descriptor to the caller.  reset() closes the current
	 * file descriptor and takes ownership of a new one.  A move
	 * constructor permits ownership to be transferred via
	 * std::move().  An integer file descriptor can be assigned
	 * directly which is equivalent to calling reset().
	 *
	 * An explicit bool conversion operator permits testing this
	 * class in logical expressions.  It returns true if it
	 * contains a valid descriptor.
	 *
	 * An implicit int conversion operator returns the underlying
	 * file descriptor allowing objects of this type to be passed
	 * directly to APIs such as connect(), listen(), etc.
	 */
	class fd_up {
	public:
		fd_up() : fd(-1) {}
		fd_up(int _fd) : fd(_fd) {}
		fd_up(fd_up &&other) : fd(other.release()) {}
		fd_up(fd_up const &) = delete;

		~fd_up() { reset(); }

		int get() const { return (fd); }

		int release()
		{
			int oldfd = fd;

			fd = -1;
			return (oldfd);
		}

		void reset(int newfd = -1)
		{
			if (fd >= 0)
				close(fd);
			fd = newfd;
		}

		fd_up &operator=(fd_up &&other) noexcept
		{
			if (this == &other)
				return *this;

			reset(other.release());
			return *this;
		}

		fd_up &operator=(fd_up const &) = delete;

		fd_up &operator=(int newfd)
		{
			reset(newfd);
			return *this;
		}

		explicit operator bool() const { return fd >= 0; }
		operator int() const { return fd; }
	private:
		int	fd;
	};

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
	 * nvlist_up is a std::unique_ptr<> for nvlist_t objects which
	 * uses nvlist_destroy() to destroy the wrapped pointer.
	 */
	struct nvlist_deleter {
		void operator() (nvlist_t *nvl) const
		{
			nvlist_destroy(nvl);
		}
	};

	using nvlist_up = std::unique_ptr<nvlist_t, nvlist_deleter>;

	/*
	 * A wrapper class for the pidfile_* API.  The destructor
	 * calls pidfile_remove() when an object is destroyed.  This
	 * class is similar to std::unique_ptr<> in that it retains
	 * exclusive ownership of the pidfh object.
	 *
	 * In addition to release() and reset methods(), write(),
	 * close(), and fileno() methods are provided as wrappers for
	 * pidfile_*.
	 */
	class pidfile {
	public:
		pidfile() = default;
		pidfile(struct pidfh *_pfh) : pfh(_pfh) {}
		pidfile(pidfile &&other) : pfh(other.release()) {}
		pidfile(pidfile const &) = delete;

		~pidfile() { reset(); }

		struct pidfh *release()
		{
			struct pidfh *oldpfh = pfh;

			pfh = nullptr;
			return (oldpfh);
		}

		void reset(struct pidfh *newpfh = nullptr)
		{
			if (pfh != nullptr)
				pidfile_remove(pfh);
			pfh = newpfh;
		}

		int write()
		{
			return (pidfile_write(pfh));
		}

		int close()
		{
			int rv = pidfile_close(pfh);
			if (rv == 0)
				pfh = nullptr;
			return (rv);
		}

		int fileno()
		{
			return (pidfile_fileno(pfh));
		}

		pidfile &operator=(pidfile &&other) noexcept
		{
			if (this == &other)
				return *this;
			reset(other.release());
			return *this;
		}

		pidfile &operator=(pidfile const &) = delete;

		pidfile &operator=(struct pidfh *newpfh)
		{
			reset(newpfh);
			return *this;
		}

		explicit operator bool() const { return pfh != nullptr; }
	private:
		struct pidfh *pfh = nullptr;
	};

	/*
	 * Returns a std::string containing the same output as
	 * sprintf().  Throws std::bad_alloc if an error occurs.
	 */
	std::string stringf(const char *fmt, ...) __printflike(1, 2);
	std::string stringf(const char *fmt, std::va_list ap);
}

#endif /* !__LIBUTILPP_HH__ */
