// gold.h -- general definitions for gold   -*- C++ -*-

#ifndef GOLD_GOLD_H

#include "config.h"
#include "ansidecl.h"

#ifdef ENABLE_NLS
# include <libintl.h>
# define _(String) gettext (String)
# ifdef gettext_noop
#  define N_(String) gettext_noop (String)
# else
#  define N_(String) (String)
# endif
#else
# define gettext(Msgid) (Msgid)
# define dgettext(Domainname, Msgid) (Msgid)
# define dcgettext(Domainname, Msgid, Category) (Msgid)
# define textdomain(Domainname) while (0) /* nothing */
# define bindtextdomain(Domainname, Dirname) while (0) /* nothing */
# define _(String) (String)
# define N_(String) (String)
#endif

// Figure out how to get a hash set and a hash map.

#if defined(HAVE_TR1_UNORDERED_SET) && defined(HAVE_TR1_UNORDERED_MAP)

#include <tr1/unordered_set>
#include <tr1/unordered_map>

// We need a template typedef here.

#define Unordered_set std::tr1::unordered_set
#define Unordered_map std::tr1::unordered_map

#elif defined(HAVE_EXT_HASH_MAP) && defined(HAVE_EXT_HASH_SET)

#include <ext/hash_map>
#include <ext/hash_set>
#include <string>

#define Unordered_set __gnu_cxx::hash_set
#define Unordered_map __gnu_cxx::hash_map

namespace __gnu_cxx
{

template<>
struct hash<std::string>
{
  size_t
  operator()(std::string s) const
  { return __stl_hash_string(s.c_str()); }
};

template<typename T>
struct hash<T*>
{
  size_t
  operator()(T* p) const
  { return reinterpret_cast<size_t>(p); }
};

}

#else

// The fallback is to just use set and map.

#include <set>
#include <map>

#define Unordered_set std::set
#define Unordered_map std::map

#endif

namespace gold
{
// This is a hack to work around a problem with older versions of g++.
// The problem is that they don't support calling a member template by
// specifying the template parameters.  It works to pass in an
// argument for argument dependent lookup.

// To use this, the member template method declaration should put
// ACCEPT_SIZE or ACCEPT_SIZE_ENDIAN after the last parameter.  If the
// method takes no parameters, use ACCEPT_SIZE_ONLY or
// ACCEPT_SIZE_ENDIAN_ONLY.

// When calling the method, instead of using fn<size>, use fn
// SELECT_SIZE_NAME or SELECT_SIZE_ENDIAN_NAME.  And after the last
// argument, put SELECT_SIZE(size) or SELECT_SIZE_ENDIAN(size,
// big_endian).  If there is only one argment, use the _ONLY variants.

#ifdef HAVE_MEMBER_TEMPLATE_SPECIFICATIONS

#define SELECT_SIZE_NAME(size) <size>
#define SELECT_SIZE(size)
#define SELECT_SIZE_ONLY(size)
#define ACCEPT_SIZE
#define ACCEPT_SIZE_ONLY
#define ACCEPT_SIZE_EXPLICIT(size)

#define SELECT_SIZE_ENDIAN_NAME(size, big_endian) <size, big_endian>
#define SELECT_SIZE_ENDIAN(size, big_endian)
#define SELECT_SIZE_ENDIAN_ONLY(size, big_endian)
#define ACCEPT_SIZE_ENDIAN
#define ACCEPT_SIZE_ENDIAN_ONLY
#define ACCEPT_SIZE_ENDIAN_EXPLICIT(size, big_endian)

#else // !defined(HAVE_MEMBER_TEMPLATE_SPECIFICATIONS)

template<int size>
class Select_size { };
template<int size, bool big_endian>
class Select_size_endian { };

#define SELECT_SIZE_NAME(size)
#define SELECT_SIZE(size) , Select_size<size>()
#define SELECT_SIZE_ONLY(size) Select_size<size>()
#define ACCEPT_SIZE , Select_size<size>
#define ACCEPT_SIZE_ONLY Select_size<size>
#define ACCEPT_SIZE_EXPLICIT(size) , Select_size<size>

#define SELECT_SIZE_ENDIAN_NAME(size, big_endian)
#define SELECT_SIZE_ENDIAN(size, big_endian) \
  , Select_size_endian<size, big_endian>()
#define SELECT_SIZE_ENDIAN_ONLY(size, big_endian) \
  Select_size_endian<size, big_endian>()
#define ACCEPT_SIZE_ENDIAN , Select_size_endian<size, big_endian>
#define ACCEPT_SIZE_ENDIAN_ONLY Select_size_endian<size, big_endian>
#define ACCEPT_SIZE_ENDIAN_EXPLICIT(size, big_endian) \
  , Select_size_endian<size, big_endian>

#endif // !defined(HAVE_MEMBER_TEMPLATE_SPECIFICATIONS)

} // End namespace gold.

namespace gold
{

class General_options;
class Command_line;
class Input_argument_list;
class Dirsearch;
class Input_objects;
class Symbol_table;
class Layout;
class Workqueue;
class Output_file;

// The name of the program as used in error messages.
extern const char* program_name;

// This function is called to exit the program.  Status is true to
// exit success (0) and false to exit failure (1).
extern void
gold_exit(bool status) ATTRIBUTE_NORETURN;

// This function is called to emit an unexpected error message and a
// newline, and then exit with failure.  If PERRNO is true, it reports
// the error in errno.
extern void
gold_fatal(const char* msg, bool perrno) ATTRIBUTE_NORETURN;

// This is function is called in some cases if we run out of memory.
extern void
gold_nomem() ATTRIBUTE_NORETURN;

// This macro and function are used in cases which can not arise if
// the code is written correctly.

#define gold_unreachable() \
  (gold::do_gold_unreachable(__FILE__, __LINE__, __FUNCTION__))

extern void do_gold_unreachable(const char*, int, const char*)
  ATTRIBUTE_NORETURN;

// Assertion check.

#define gold_assert(expr) ((void)(!(expr) ? gold_unreachable(), 0 : 0))

// Queue up the first set of tasks.
extern void
queue_initial_tasks(const General_options&,
		    const Dirsearch&,
		    const Command_line&,
		    Workqueue*,
		    Input_objects*,
		    Symbol_table*,
		    Layout*);

// Queue up the middle set of tasks.
extern void
queue_middle_tasks(const General_options&,
		   const Input_objects*,
		   Symbol_table*,
		   Layout*,
		   Workqueue*);

// Queue up the final set of tasks.
extern void
queue_final_tasks(const General_options&,
		  const Input_objects*,
		  const Symbol_table*,
		  const Layout*,
		  Workqueue*,
		  Output_file* of);

} // End namespace gold.

#endif // !defined(GOLD_GOLD_H)
