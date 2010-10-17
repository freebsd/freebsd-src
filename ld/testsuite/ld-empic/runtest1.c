/* First C source file for actual execution test.  */

/* The main point of this test is to make sure that the code and data
   are truly position independent.  We statically initialize several
   global variables, and make sure that they are correctly adjusted at
   runtime.  */

int i = 1;
int j = 0;
extern int k;
int l;
char small_buf[] = "aaaa";
char *small_pointer = small_buf;
char big_buf[] = "aaaaaaaaaaaaaaaa";
char *big_pointer = big_buf;

extern int bar ();
int (*pbar) () = bar;

static int
foo2 (arg)
     int arg;
{
  l = arg;
  return i + j;
}

int (*pfoo2) () = foo2;

int
chkstr (z, c)
     char *z;
     int c;
{
  /* Switch statements need extra effort to be position independent,
     so we run one here, even though most of the cases will never be
     taken.  */
  switch (c)
    {
    case 1:
    case 2:
    case 3:
      return i - 1;
    case 4:
      break;
    case 5:
    case 6:
    case 7:
    case 8:
    case 9:
      return i * j;
    case 10:
    case 11:
    case 12:
    case 13:
    case 14:
    case 15:
      return j;
    case 16:
      break;
    default:
      return 0;
    }

  while (c-- != 0)
    if (*z++ != 'a')
      return 0;

  return *z == '\0';
}

/* This function is called by the assembler startup routine.  It tries
   to test that everything was correctly initialized.  It returns 0 on
   success, something else on failure.  */

int
foo ()
{
  if (i != 1)
    return 1;
  if (j != 0)
    return 2;
  if (! chkstr (small_buf, 4))
    return 3;
  if (! chkstr (small_pointer, 4))
    return 4;
  if (! chkstr (big_buf, 16))
    return 5;
  if (! chkstr (big_pointer, 16))
    return 6;

  if (l != 0)
    return 7;
  if (foo2 (1) != 1)
    return 8;
  if (l != 1)
    return 9;
  if ((*pfoo2) (2) != 1)
    return 10;
  if (l != 2)
    return 11;

  if (bar (1) != 0)
    return 12;
  if (bar (-1) != 1)
    return 13;
  if ((*pbar) (0xa5a5a5a5) != -1)
    return 14;
  if (k != 0xa5a5a5a5)
    return 15;
  if ((*pbar) (0) != 0xa5a5a5a5)
    return 16;
  if (k != 0)
    return 17;

  return 0;
}
