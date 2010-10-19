// Class Foo
#pragma implementation


// We don't use header files, since we only want to see, whether the
// compiler is installed properly.
//
#if (__GNUG__ == 2)
typedef __SIZE_TYPE__ size_t;
#else
typedef unsigned int size_t;
#endif

extern "C" {
    char *strncpy (char* dest, const char* dest, size_t len);
    int printf (const char*, ...);
};

#include "cdtest-foo.h"

int Foo::foos = 0;

void Foo::init_foo ()
{
    printf ("BROKENLY calling Foo::init_foo from __init_start; size_of(Foo) = %ld\n", (long) sizeof(Foo));
    foos = FOOLISH_NUMBER;
}


Foo::Foo ()
{
    i = ++foos;
    strncpy (message, "default-foo", len);
#ifdef WITH_ADDR
    printf ("Constructing Foo(%d) \"default-foo\" at %08x\n", i, this);
#else
    printf ("Constructing Foo(%d) \"default-foo\"\n", i);
#endif
}

Foo::Foo (const char* msg)
{
    i = ++foos;
    strncpy( message, msg, len);
#ifdef WITH_ADDR
    printf ( "Constructing Foo(%d) \"%s\" at %08x\n", i, message, this);
#else
    printf ( "Constructing Foo(%d) \"%s\"\n", i, message);
#endif
}


Foo::Foo (const Foo& foo)
{
    i = ++foos;
#ifdef WITH_ADDR
    printf ("Initializing Foo(%d) \"%s\" at %08x with Foo(%d) %08x\n", 
	    i, foo.message, this, foo.i, &foo);
#else
   printf ("Initializing Foo(%d) \"%s\" with Foo(%d)\n",i, foo.message, foo.i);
#endif
    for ( int k = 0; k < FOO_MSG_LEN; k++) message[k] = foo.message[k];
}


Foo& Foo::operator= (const Foo& foo)
{
#ifdef WITH_ADDR
    printf ("Copying Foo(%d) \"%s\" at %08x to Foo(%d) %08x\n", 
	    foo.i, foo.message, &foo, i, this);
#else
   printf ("Copying Foo(%d) \"%s\" to Foo(%d)\n", foo.i, foo.message, i);
#endif
    for ( int k = 0; k < FOO_MSG_LEN; k++) message[k] = foo.message[k];
    return *this;
}


Foo::~Foo ()
{
    foos--;
#ifdef WITH_ADDR
    printf ("Destructing Foo(%d) \"%s\" at %08x (remaining foos: %d)\n",
	    i, message, this, foos);
#else
    printf ("Destructing Foo(%d) \"%s\" (remaining foos: %d)\n",
	    i, message, foos);
#endif
}
