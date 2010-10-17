extern int xfunc (int y);

static int sfunc (int y)
{
  xfunc (y);
}

int gfunc (int y)
{
  sfunc (y);
}

int branches (int y)
{
  int z;

  for (z = y; z < y + 10; z++)
    {
      if (z & 0x1)
	{
	  gfunc (z);
	}
      else
	{
	  xfunc (z);
	}
    }
}
