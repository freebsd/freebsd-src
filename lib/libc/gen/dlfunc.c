/*
 * This source file is in the public domain.
 * Garrett A. Wollman, 2002-05-28.
 *
 * $FreeBSD$
 */

#include <dlfcn.h>

/*
 * Implement the dlfunc() interface, which behaves exactly the same as
 * dlsym() except that it returns a function pointer instead of a data
 * pointer.  This can be used by applications to avoid compiler warnings
 * about undefined behavior, and is intended as prior art for future
 * POSIX standardization.  This function requires that all pointer types
 * have the same representation, which is true on all platforms FreeBSD
 * runs on, but is not guaranteed by the C standard.
 */
dlfunc_t
dlfunc(void * __restrict handle, const char * __restrict symbol)
{
	union {
		void *d;
		dlfunc_t f;
	} rv;

	rv.d = dlsym(handle, symbol);
	return (rv.f);
}

