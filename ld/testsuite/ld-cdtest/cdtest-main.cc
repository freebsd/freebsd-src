// main program for Class Foo

extern "C" {
// Some <assert.h> implementations (e.g. SUNOS 4.1) are broken,
// in that they require <stdio.h>.  But, if gcc/g++ is installed
// correctly, you should get gcc's assert.h.
// If the compile fails, it means the wrong include files are in use!
#include <assert.h>
};
#include "cdtest-foo.h"

extern "C" void __init_start();

extern Foo f(void);
extern void g(void);

/* This function should *not* be called by the environment.  There is
   no way in C++ to ``run something after the initializers but before main()''.
   The library that depends on this (NIHCL) is broken.  -- John Gilmore 
   We leave this here to test that future changes to the compiler
   do not re-introduce this losing ``feature''.  */
void 
__init_start()
{
    Foo::init_foo();
}

static Foo static_foo( "static_foo"); 

int main() 
{ 
    assert (Foo::nb_foos() == 2);
    Foo automatic_foo( "automatic_foo");
    Foo bla_foo = f();
    assert (Foo::nb_foos() == 4);
    g();
    assert (Foo::nb_foos() == 4);
    // `automatic_foo' and `bla_foo' are destructed here 
} 
 
