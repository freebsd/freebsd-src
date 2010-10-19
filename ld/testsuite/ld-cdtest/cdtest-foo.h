// Class Foo

#pragma interface

#define FOOLISH_NUMBER -4711

#ifndef FOO_MSG_LEN
#define FOO_MSG_LEN 80
#endif
 
class Foo {
    static int foos;
    int i;
    static const int len = FOO_MSG_LEN;
    char message[len];
public: 
    static void init_foo ();
    static int nb_foos() { return foos; }
    Foo();
    Foo(const char* message);
    Foo(const Foo&);
    Foo & operator= (const Foo&);
    ~Foo ();
}; 
