int global;

extern void trap (int, int);
static void quit (int);
static int foo (int);

int
main ()
{
  if (foo (0) != 0 || global != 0)
    quit (1);
  if (foo (1) != 1 || global != 1)
    quit (1);
  if (foo (2) != 2 || global != 2)
    quit (1);
  if (foo (3) != 3 || global != 3)
    quit (1);
  if (foo (4) != 4 || global != 4)
    quit (1);
  if (foo (5) != 5 || global != 5)
    quit (1);
  if (foo (6) != 6 || global != 6)
    quit (1);
  if (foo (7) != 7 || global != 7)
    quit (1);
  if (foo (8) != 8 || global != 8)
    quit (1);
  quit (0);
}

void
__main ()
{
}

static void
quit (int status)
{
  trap (1, status);
}

int
bar (int i)
{
  global = i;
  return i;
}

int
bar0 (int i)
{
  global = 0;
  return i;
}

int
bar1 (int i)
{
  global = 1;
  return i;
}

int
bar2 (int i)
{
  global = 2;
  return i;
}

int
bar3 (int i)
{
  global = 3;
  return i;
}

int
bar4 (int i)
{
  global = 4;
  return i;
}

int
bar5 (int i)
{
  global = 5;
  return i;
}

int
bar6 (int i)
{
  global = 6;
  return i;
}

int
bar7 (int i)
{
  global = 7;
  return i;
}

int
foo (int i)
{
  switch (i)
    {
    case 0: bar0 (0); return 0;
    case 1: bar1 (1); return 1;
    case 2: bar2 (2); return 2;
    case 3: bar3 (3); return 3;
    case 4: bar4 (4); return 4;
    case 5: bar5 (5); return 5;
    case 6: bar6 (6); return 6;
    case 7: bar7 (7); return 7;
    default: return bar (i);
    }
}
