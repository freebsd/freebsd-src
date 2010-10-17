#include <stdio.h>

extern int var;
extern void (*func_ptr)(void);
extern void print_var (void);
extern void print_foo (void);
extern int foo;
extern int var2[2];

typedef struct
{
  int *   var;
  void (* func_ptr)(void);
}
TEST;

TEST xyz = { &var, print_var };

int
main (void)
{
  print_var ();

  printf ("We see var = %d\n", var);
  printf ("Setting var = 456\n");

  var = 456;

  print_var ();
  printf ("We see var = %d\n\n", var);

  var = 90;
  print_var ();
  printf ("We see var = %d\n\n", var);

  print_foo ();
  printf ("We see foo = %d\n", foo);
  printf ("Setting foo = 19\n");
  foo = 19;
  print_foo ();
  printf ("We see foo = %d\n\n", foo);
  fflush (stdout);

  printf ("Calling dllimported function pointer\n");
  func_ptr ();

  printf ("Calling functions using global structure\n"); 
  xyz.func_ptr ();
  * xyz.var = 40;
  xyz.func_ptr ();

  printf ("We see var2[0] = %d\n\n", var2[0]);

  return 0;
}
