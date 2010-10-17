// test program for Class Foo

#include "cdtest-foo.h"

static Foo static_foo( "static_foo"); 

Foo f()
{
    Foo x;
    return x;
}

void g()
{
    Foo other_foo1 = Foo( "other_foo1"), other_foo2 = Foo( "other_foo2");
    other_foo2 = other_foo1;
}
