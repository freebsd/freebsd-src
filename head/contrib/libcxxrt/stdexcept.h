/**
 * stdexcept.h - provides a stub version of <stdexcept>, which defines enough
 * of the exceptions for the runtime to use.  
 */

namespace std
{

	class exception
	{
	public:
		exception() throw();
		exception(const exception&) throw();
		exception& operator=(const exception&) throw();
		virtual ~exception();
		virtual const char* what() const throw();
	};


	/**
	 * Bad allocation exception.  Thrown by ::operator new() if it fails.
	 */
	class bad_alloc: public exception
	{
	public:
		bad_alloc() throw();
		bad_alloc(const bad_alloc&) throw();
		bad_alloc& operator=(const bad_alloc&) throw();
		~bad_alloc();
		virtual const char* what() const throw();
	};

	/**
	 * Bad cast exception.  Thrown by the __cxa_bad_cast() helper function.
	 */
	class bad_cast: public exception {
	public:
		bad_cast() throw();
		bad_cast(const bad_cast&) throw();
		bad_cast& operator=(const bad_cast&) throw();
		virtual ~bad_cast();
		virtual const char* what() const throw();
	};

	/**
	 * Bad typeidexception.  Thrown by the __cxa_bad_typeid() helper function.
	 */
	class bad_typeid: public exception
	{
	public:
		bad_typeid() throw();
		bad_typeid(const bad_typeid &__rhs) throw();
		virtual ~bad_typeid();
		bad_typeid& operator=(const bad_typeid &__rhs) throw();
		virtual const char* what() const throw();
	};



} // namespace std

