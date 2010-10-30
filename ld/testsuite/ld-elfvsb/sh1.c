#ifndef NULL
#define NULL ((void *) 0)
#endif

/* This is part of the shared library ld test.  This file becomes part
   of a shared library.  */

/* This variable is supplied by the main program.  */
#ifndef XCOFF_TEST
extern int mainvar;
#endif

/* This variable is defined in the shared library, and overridden by
   the main program.  */
#ifndef XCOFF_TEST
#ifdef SHARED
/* SHARED is defined if we are compiling with -fpic/-fPIC.  */
int overriddenvar = -1;
#else
/* Without -fpic, newer versions of gcc assume that we are not
   compiling for a shared library, and thus that overriddenvar is
   local.  */
extern int overriddenvar;
#endif
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

#ifdef SHARED
int
shlib_overriddencall2 ()
{
  return 7;
}
#endif
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

#ifdef HIDDEN_WEAK_TEST
#define HIDDEN_UNDEF_TEST
#define WEAK_TEST
#endif

#ifdef PROTECTED_WEAK_TEST
#define PROTECTED_UNDEF_TEST
#define WEAK_TEST
#endif

#if defined (HIDDEN_UNDEF_TEST) || defined (PROTECTED_UNDEF_TEST)
#ifdef WEAK_TEST
#pragma weak visibility
#endif
extern int visibility ();
#else
int
visibility ()
{
  return 2;
}
#endif

#ifdef HIDDEN_NORMAL_TEST
asm (".hidden visibility_normal");

int
visibility_normal ()
{
  return 2;
}
#endif

int
visibility_checkfunptr ()
{
#ifdef WEAK_TEST
  return 1;
#else
#ifdef HIDDEN_NORMAL_TEST
  int (*v) () = visibility_normal;
#else
  int (*v) () = visibility;
#endif
  return (*v) () == 2;
#endif
}

int
visibility_check ()
{
#ifdef WEAK_TEST
  if (&visibility)
    return visibility () == 1;
  else
    return 1;
#else
#ifdef HIDDEN_NORMAL_TEST
  return visibility_normal () == 2;
#else
  return visibility () == 2;
#endif
#endif
}

void *
visibility_funptr ()
{
#ifdef WEAK_TEST
  if (&visibility == NULL)
    return NULL;
  else
#endif
    return visibility;
}

#if defined (HIDDEN_UNDEF_TEST) || defined (PROTECTED_UNDEF_TEST)
#ifdef WEAK_TEST
#pragma weak visibility_var
#endif
extern int visibility_var;
#else
int visibility_var = 2;
#endif

#ifdef HIDDEN_NORMAL_TEST
asm (".hidden visibility_var_normal");

int visibility_var_normal = 2;
#endif

int
visibility_checkvarptr ()
{
#ifdef WEAK_TEST
  if (&visibility_var)
    return visibility_var == 1;
  else
    return 1;
#else
#ifdef HIDDEN_NORMAL_TEST
  int *v = &visibility_var_normal;
#else
  int *v = &visibility_var;
#endif
  return *v == 2;
#endif
}

int
visibility_checkvar ()
{
#ifdef WEAK_TEST
  return 1;
#else
#ifdef HIDDEN_NORMAL_TEST
  return visibility_var_normal == 2;
#else
  return visibility_var == 2;
#endif
#endif
}

void *
visibility_varptr ()
{
#ifdef WEAK_TEST
  if (&visibility_var == NULL)
    return NULL;
  else
#endif
    return &visibility_var;
}

int
visibility_varval ()
{
#ifdef WEAK_TEST
  if (&visibility_var == NULL)
    return 0;
  else
#endif
    return visibility_var;
}

#if defined (HIDDEN_TEST) || defined (HIDDEN_UNDEF_TEST)
asm (".hidden visibility");
asm (".hidden visibility_var");
#else
#if defined (PROTECTED_TEST) || defined (PROTECTED_UNDEF_TEST) || defined (PROTECTED_WEAK_TEST)
asm (".protected visibility");
asm (".protected visibility_var");
#endif
#endif

#ifdef HIDDEN_NORMAL_TEST
int shlib_visibility_com;
asm (".hidden shlib_visibility_com");

int
shlib_visibility_checkcom ()
{
  return shlib_visibility_com == 0;
}

int
shlib_visibility_checkweak ()
{
  return 1;
}
#elif defined (HIDDEN_WEAK_TEST)
#pragma weak shlib_visibility_undef_var_weak
extern int shlib_visibility_undef_var_weak;
asm (".hidden shlib_visibility_undef_var_weak");

#pragma weak shlib_visibility_undef_func_weak
extern int shlib_visibility_undef_func_weak ();
asm (".hidden shlib_visibility_undef_func_weak");

#pragma weak shlib_visibility_var_weak
extern int shlib_visibility_var_weak;
asm (".hidden shlib_visibility_var_weak");

#pragma weak shlib_visibility_func_weak
extern int shlib_visibility_func_weak ();
asm (".hidden shlib_visibility_func_weak");

int
shlib_visibility_checkcom ()
{
  return 1;
}

int
shlib_visibility_checkweak ()
{
  return &shlib_visibility_undef_var_weak == NULL
	 && &shlib_visibility_undef_func_weak == NULL
	 && &shlib_visibility_func_weak == NULL
	 && &shlib_visibility_var_weak == NULL;
}
#else
int
shlib_visibility_checkcom ()
{
  return 1;
}

int
shlib_visibility_checkweak ()
{
  return 1;
}
#endif

#ifdef PROTECTED_TEST
#ifdef SHARED
int shared_data = 100;
#else
extern int shared_data;
#endif
 
int *
shared_data_p ()
{
  return &shared_data;
}
 
int
shared_func ()
{
  return 100;
}
 
void *
shared_func_p ()
{
  return shared_func;
}
#endif
