/* Normally we should loose foo and keep _start and _init. 
   With -u foo, we should keep that as well.  */

void _start() __asm__("_start");
void _start()
{
}

void __attribute__((section(".init")))
_init()
{
}

int foo() __asm__("foo");
int foo()
{
  static int x = 1;
  return x++;
}
