/* This file is compiled and linked into the S-record format.  */

int e1;
int e2 = 1;

int
fn1 (s)
     char *s;
{
  return s[e1];
}

int
fn2 (s)
     char *s;
{
  return s[e2];
}
