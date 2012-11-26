#include <stdlib.h>

namespace std
{
	/**
	 * Stub implementation of std::terminate.  Used when the STL implementation
	 * doesn't provide one.
	 */
	__attribute__((weak))
	void terminate()
	{
		abort();
	}
}
