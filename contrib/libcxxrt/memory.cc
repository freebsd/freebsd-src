/**
 * memory.cc - Contains stub definition of C++ new/delete operators.
 *
 * These definitions are intended to be used for testing and are weak symbols
 * to allow them to be replaced by definitions from a STL implementation.
 * These versions simply wrap malloc() and free(), they do not provide a
 * C++-specific allocator.
 */

#include <stddef.h>
#include <stdlib.h>
#include "stdexcept.h"

namespace std
{
	struct nothrow_t {};
}


/// The type of the function called when allocation fails.
typedef void (*new_handler)();
/**
 * The function to call when allocation fails.  By default, there is no
 * handler and a bad allocation exception is thrown if an allocation fails.
 */
static new_handler new_handl;

namespace std
{
	/**
	 * Sets a function to be called when there is a failure in new.
	 */
	__attribute__((weak))
	new_handler set_new_handler(new_handler handler)
	{
		return __sync_lock_test_and_set(&new_handl, handler);
	}
}


__attribute__((weak))
void* operator new(size_t size)
{
	void * mem = malloc(size);
	while (0 == mem)
	{
		if (0 != new_handl)
		{
			new_handl();
		}
		else
		{
			throw std::bad_alloc();
		}
		mem = malloc(size);
	}

	return mem;
}

__attribute__((weak))
void* operator new(size_t size, const std::nothrow_t &) throw()
{
	void *mem = malloc(size);
	while (0 == mem)
	{
		if (0 != new_handl)
		{
			try
			{
				new_handl();
			}
			catch (...)
			{
				// nothrow operator new should return NULL in case of
				// std::bad_alloc exception in new handler
				return NULL;
			}
		}
		else
		{
			return NULL;
		}
		mem = malloc(size);
	}

	return mem;
}


__attribute__((weak))
void operator delete(void * ptr)
{
	free(ptr);
}


__attribute__((weak))
void * operator new[](size_t size)
{
	return ::operator new(size);
}


__attribute__((weak))
void operator delete[](void * ptr)
{
	::operator delete(ptr);
}


