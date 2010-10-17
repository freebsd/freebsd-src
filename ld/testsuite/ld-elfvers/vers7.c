/*
 * Test program that goes with test7.so
 */

extern int hide_a();
extern int show_b();

int
main()
{
  return hide_a(1) + show_b(1);
  return 0;
}
