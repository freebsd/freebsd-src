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

void _start()
{
  getme()->bar();
}

void start ()
{
  _start ();
}

extern "C" void __main() { }
