/* 
 * Copyright 2010-2011 PathScale, Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS
 * IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

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
#include "atomic.h"


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
static atomic<new_handler> new_handl{nullptr};

namespace std
{
	/**
	 * Sets a function to be called when there is a failure in new.
	 */
	__attribute__((weak))
	new_handler set_new_handler(new_handler handler)
	{
		return new_handl.exchange(handler);
	}

	__attribute__((weak))
	new_handler get_new_handler(void)
	{
		return new_handl.load();
	}
}


#if __cplusplus < 201103L
#define BADALLOC throw(std::bad_alloc)
#else
#define BADALLOC
#endif

namespace
{
	/**
	 * Helper for forwarding from no-throw operators to versions that can
	 * return nullptr.  Catches any exception and converts it into a nullptr
	 * return.
	 */
	template<void*(New)(size_t)>
	void *noexcept_new(size_t size)
	{
#if !defined(_CXXRT_NO_EXCEPTIONS)
	try
	{
		return New(size);
	} catch (...)
	{
		// nothrow operator new should return NULL in case of
		// std::bad_alloc exception in new handler
		return nullptr;
	}
#else
	return New(size);
#endif
	}
}


__attribute__((weak))
void* operator new(size_t size) BADALLOC
{
	if (0 == size)
	{
		size = 1;
	}
	void * mem = malloc(size);
	while (0 == mem)
	{
		new_handler h = std::get_new_handler();
		if (0 != h)
		{
			h();
		}
		else
		{
#if !defined(_CXXRT_NO_EXCEPTIONS)
			throw std::bad_alloc();
#else
			break;
#endif
		}
		mem = malloc(size);
	}

	return mem;
}


__attribute__((weak))
void* operator new(size_t size, const std::nothrow_t &) _LIBCXXRT_NOEXCEPT
{
	return noexcept_new<(::operator new)>(size);
}


__attribute__((weak))
void operator delete(void * ptr) _LIBCXXRT_NOEXCEPT
{
	free(ptr);
}


__attribute__((weak))
void * operator new[](size_t size) BADALLOC
{
	return ::operator new(size);
}


__attribute__((weak))
void * operator new[](size_t size, const std::nothrow_t &) _LIBCXXRT_NOEXCEPT
{
	return noexcept_new<(::operator new[])>(size);
}


__attribute__((weak))
void operator delete[](void * ptr) _LIBCXXRT_NOEXCEPT
{
	::operator delete(ptr);
}

// C++14 additional delete operators

#if __cplusplus >= 201402L

__attribute__((weak))
void operator delete(void * ptr, size_t) _LIBCXXRT_NOEXCEPT
{
	::operator delete(ptr);
}


__attribute__((weak))
void operator delete[](void * ptr, size_t) _LIBCXXRT_NOEXCEPT
{
	::operator delete(ptr);
}

#endif
