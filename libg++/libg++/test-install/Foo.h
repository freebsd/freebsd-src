// Class Foo

#pragma interface

#define FOOLISH_NUMBER -4711

#ifndef FOO_MSG_LEN
#define FOO_MSG_LEN 80
#endif
 
class Foo {
    static int foos;
    int i;
    const len = FOO_MSG_LEN;
    char message[len];
public: 
    static void init_foo ();
    static int nb_foos() { return foos; }
    Foo();
    Foo( char* message);
    Foo(const Foo&);
    Foo & operator= (const Foo&);
    ~Foo ();
}; 
