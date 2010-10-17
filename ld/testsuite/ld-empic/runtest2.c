/* Second C source file for actual execution test.  */

int k;
extern int i;
extern int j;
extern char small_buf[];
extern char *small_pointer;

extern int chkstr ();

int
bar (n)
     int n;
{
  int r;

  if (i != 1
      || j != 0
      || ! chkstr (small_buf, 4)
      || ! chkstr (small_pointer, 4))
    return k + 1;

  r = k;
  k = n;
  return r;
}
