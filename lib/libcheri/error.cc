#include <sys/types.h>
#include <cheri/cheri.h>

#include "sandbox.h"

cheri::sandbox_invoke_failure::~sandbox_invoke_failure() throw() {}
const char *cheri::sandbox_invoke_failure::what() const throw()
{
	return "CHERI sandbox invocation error.";
}

extern "C" void __cxa_cheri_sandbox_invoke_failure(int e)
{
	cheri::sandbox_invoke_failure ex(e);
	throw ex;
}
