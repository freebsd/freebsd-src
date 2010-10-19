// This file is compiled and linked into the S-record format.

#define FOO_MSG_LEN 80

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

static Foo static_foo( "static_foo");

int
main ()
{
  Foo automatic_foo( "automatic_foo");
  return 0;
}

void
terminate(void)
{
  /* This recursive call prevents a compiler warning that the noreturn
     function terminate actually does return.  */
  terminate ();
}

extern "C" {
void
__main ()
{
}

void
__builtin_delete ()
{
}

void
__builtin_new ()
{
}

void
__throw ()
{
}

void
__rethrow ()
{
}

void
__terminate ()
{
}

void *__eh_pc;

void ***
__get_dynamic_handler_chain ()
{
  return 0;
}

void *
__get_eh_context ()
{
  return 0;
}

}

int Foo::foos = 0;

void Foo::init_foo ()
{
  foos = 80;
}

Foo::Foo ()
{
  i = ++foos;
}

Foo::Foo (const char*)
{
  i = ++foos;
}

Foo::Foo (const Foo& foo)
{
  i = ++foos;
  for (int k = 0; k < FOO_MSG_LEN; k++)
    message[k] = foo.message[k];
}

Foo& Foo::operator= (const Foo& foo)
{
  for (int k = 0; k < FOO_MSG_LEN; k++)
    message[k] = foo.message[k];
  return *this;
}

Foo::~Foo ()
{
  foos--;
}

void *__dso_handle;

extern "C"
int
__cxa_atexit (void)
{
  return 0;
}
