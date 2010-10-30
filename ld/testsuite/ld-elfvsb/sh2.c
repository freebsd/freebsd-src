/* This is part of the shared library ld test.  This file becomes part
   of a shared library.  */

/* This variable is defined here, and referenced by another file in
   the shared library.  */
int shlibvar2 = 4;

/* This variable is defined here, and shouldn't be used to resolve a
   reference with non-default visibility in another shared library.  */
int visibility_com = 2;

/* This function is called by another file in the shared library.  */

int
shlib_shlibcalled ()
{
  return 5;
}

#ifdef DSO_DEFINE_TEST
int
visibility ()
{
  return 2;
}

int visibility_var = 2;

int visibility_def = 2;

int
visibility_func ()
{
  return 2;
}
#endif

#ifdef HIDDEN_WEAK_TEST
int visibility_var_weak = 2;

int
visibility_func_weak ()
{
  return 2;
}
#endif

#ifndef SHARED
# ifndef XCOFF_TEST
int overriddenvar = -1;

int
shlib_overriddencall2 ()
{
  return 7;
}
# endif
# ifdef PROTECTED_TEST
int shared_data = 100;
# endif
#endif
