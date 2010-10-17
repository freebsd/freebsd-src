/* This is part of the shared library ld test.  This file becomes part
   of a shared library.  */

/* This variable is supplied by the main program.  */
#ifndef XCOFF_TEST
extern int mainvar;
#endif

/* This variable is defined in the shared library, and overridden by
   the main program.  */
#ifndef XCOFF_TEST
int overriddenvar = -1;
#endif

/* This variable is defined in the shared library.  */
int shlibvar1 = 3;

/* This variable is defined by another object in the shared library.  */
extern int shlibvar2;

/* These functions return the values of the above variables as seen in
   the shared library.  */

#ifndef XCOFF_TEST
int
shlib_mainvar ()
{
  return mainvar;
}
#endif

#ifndef XCOFF_TEST
int
shlib_overriddenvar ()
{
  return overriddenvar;
}
#endif

int
shlib_shlibvar1 ()
{
  return shlibvar1;
}

int
shlib_shlibvar2 ()
{
  return shlibvar2;
}

/* This function calls a function defined by another object in the
   shared library.  */

extern int shlib_shlibcalled ();

int
shlib_shlibcall ()
{
  return shlib_shlibcalled ();
}

#ifndef XCOFF_TEST
/* This function calls a function defined in this object in the shared
   library.  The main program will override the called function.  */

extern int shlib_overriddencall2 ();

int
shlib_shlibcall2 ()
{
  return shlib_overriddencall2 ();
}

int
shlib_overriddencall2 ()
{
  return 7;
}
#endif

/* This function calls a function defined by the main program.  */

#ifndef XCOFF_TEST
extern int main_called ();

int
shlib_maincall ()
{
  return main_called ();
}
#endif

/* This function is passed a function pointer to shlib_mainvar.  It
   confirms that the pointer compares equally.  */

int 
shlib_checkfunptr1 (p)
     int (*p) ();
{
  return p == shlib_shlibvar1;
}

/* This function is passed a function pointer to main_called.  It
   confirms that the pointer compares equally.  */

#ifndef XCOFF_TEST
int
shlib_checkfunptr2 (p)
     int (*p) ();
{
  return p == main_called;
}
#endif

/* This function returns a pointer to shlib_mainvar.  */

int
(*shlib_getfunptr1 ()) ()
{
  return shlib_shlibvar1;
}

/* This function returns a pointer to main_called.  */

#ifndef XCOFF_TEST
int
(*shlib_getfunptr2 ()) ()
{
  return main_called;
}
#endif

/* This function makes sure that constant data and local functions
   work.  */

#ifndef __STDC__
#define const
#endif

static int i = 6;
static const char *str = "Hello, world\n";

int
shlib_check ()
{
  const char *s1, *s2;

  if (i != 6)
    return 0;

  /* To isolate the test, don't rely on any external functions, such
     as strcmp.  */
  s1 = "Hello, world\n";
  s2 = str;
  while (*s1 != '\0')
    if (*s1++ != *s2++)
      return 0;
  if (*s2 != '\0')
    return 0;

  if (shlib_shlibvar1 () != 3)
    return 0;

  return 1;
}
