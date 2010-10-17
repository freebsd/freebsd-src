/* This file is compiled and linked into the S-record format.  */

extern int e1;
extern int e2;
int i;
int j = 1;
static int k;
static int l = 1;
static char ab[] = "This is a string constant";

extern int fn1 ();
extern int fn2 ();

int
main ()
{
  fn1 (ab);
  fn2 ("static string constant");
  return e1 + e2 + i + j + k + l;
}

void
__main ()
{
}
