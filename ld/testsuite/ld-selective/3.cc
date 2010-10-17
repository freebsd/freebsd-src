struct A
{
  virtual void foo();
  virtual void bar();
};

void A::foo() { }			// keep
void A::bar() { }			// lose

struct B : public A
{
  virtual void foo();
};

void B::foo() { }			// keep

void _start() __asm__("_start"); // keep
void start() __asm__("start"); // some toolchains use this name.

A a;					// keep
B b;
A *getme() { return &a; }		// keep

void _start()
{
  getme()->foo();
#ifdef __GNUC__
#if (__GNUC__ == 2 && __GNUC_MINOR__ < 96)
// gcc-2.95.2 gets this test wrong, and loses B::foo().
// Cheat.  After all, we aren't trying to test the compiler here.
  b.foo();
#endif
#endif
}

void start ()
{
  _start ();
}

// In addition, keep A's virtual table.

// We'll wind up keeping `b' and thus B's virtual table because
// `a' and `b' are both referenced from the constructor function.

extern "C" void __main() { }
