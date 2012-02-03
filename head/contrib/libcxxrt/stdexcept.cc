/**
 * stdexcept.cc - provides stub implementations of the exceptions required by the runtime.
 */
#include "stdexcept.h"

namespace std {

exception::exception() throw() {}
exception::~exception() {}
exception::exception(const exception&) throw() {}
exception& exception::operator=(const exception&) throw()
{
	return *this;
}
const char* exception::what() const throw()
{
	return "std::exception";
}

bad_alloc::bad_alloc() throw() {}
bad_alloc::~bad_alloc() {}
bad_alloc::bad_alloc(const bad_alloc&) throw() {}
bad_alloc& bad_alloc::operator=(const bad_alloc&) throw()
{
	return *this;
}
const char* bad_alloc::what() const throw()
{
	return "cxxrt::bad_alloc";
}



bad_cast::bad_cast() throw() {}
bad_cast::~bad_cast() {}
bad_cast::bad_cast(const bad_cast&) throw() {}
bad_cast& bad_cast::operator=(const bad_cast&) throw()
{
	return *this;
}
const char* bad_cast::what() const throw()
{
	return "std::bad_cast";
}

bad_typeid::bad_typeid() throw() {}
bad_typeid::~bad_typeid() {}
bad_typeid::bad_typeid(const bad_typeid &__rhs) throw() {}
bad_typeid& bad_typeid::operator=(const bad_typeid &__rhs) throw()
{
	return *this;
}

const char* bad_typeid::what() const throw()
{
	return "std::bad_typeid";
}

} // namespace std

