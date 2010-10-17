struct A
{
  virtual void foo();
  virtual void bar();
};

void A::foo() { }			// lose
void A::bar() { }			// keep

struct B : public A
{
  virtual void foo();
};

void B::foo() { }			// lose

void _start() __asm__("_start");	// keep
void start() __asm__("start"); // some toolchains use this name.

A a;					// keep
B b;
A *getme() { return &a; }		// keep

extern B* dropme2();
void dropme1() { dropme2()->foo(); }	// lose
B *dropme2() { return &b; }		// lose

void _start()
{
  getme()->bar();
}

void start ()
{
  _start ();
}

extern "C" void __main() { }
