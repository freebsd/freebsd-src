/* Copyright 2004 Bob Proulx <bob@proulx.com>
Distributed under the two-clause BSD licence;
see the COPYING file for details. */

# include <stdio.h>
# include <stdlib.h>

# ifndef BAR
int bar() { return 0; }
# else
#  error BAR defined
# endif

int main()
{
  bar();
}
