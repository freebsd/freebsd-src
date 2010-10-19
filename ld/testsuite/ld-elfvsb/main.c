#ifdef PROTECTED_CHECK
#include <features.h>
#include <stdio.h>

int
main (void)
{
#if defined (__GLIBC__) && (__GLIBC__ > 2 \
			    || (__GLIBC__ == 2 \
				&&  __GLIBC_MINOR__ >= 2))
  puts ("yes");
#else
  puts ("no");
#endif
  return 0;
}
#else
/* This is the main program for the shared library test.  */

#include <stdio.h>

int mainvar = 1;
int overriddenvar = 2;
extern int shlibvar1;

extern int shlib_mainvar ();
extern int shlib_overriddenvar ();
extern int shlib_shlibvar1 ();
extern int shlib_shlibvar2 ();
extern int shlib_shlibcall ();
extern int shlib_maincall ();
extern int shlib_checkfunptr1 ();
extern int shlib_checkfunptr2 ();
extern int (*shlib_getfunptr1 ()) ();
extern int (*shlib_getfunptr2 ()) ();
extern int shlib_check ();
extern int shlib_shlibcall2 ();
extern int visibility_check ();
extern int visibility_checkfunptr ();
extern void *visibility_funptr ();
extern int visibility_checkvar ();
extern int visibility_checkvarptr ();
extern int visibility_varval ();
extern void *visibility_varptr ();
extern int shlib_visibility_checkcom ();
extern int shlib_visibility_checkweak ();

int shlib_visibility_com = 1;

int shlib_visibility_var_weak = 1;

int
shlib_visibility_func_weak ()
{
  return 1;
}

#ifdef HIDDEN_WEAK_TEST
#define WEAK_TEST
#endif

#ifdef PROTECTED_WEAK_TEST
#define WEAK_TEST
#endif

#ifdef PROTECTED_UNDEF_TEST
#define PROTECTED_TEST
#endif

#ifndef WEAK_TEST
extern int visibility ();
extern int visibility_var;
#endif

#if !defined (HIDDEN_TEST) && defined (PROTECTED_TEST)
int
visibility (void)
{
  return 1;
}

static int
main_visibility_check (void)
{
  return ((int (*) (void)) visibility_funptr ()) != visibility;
}

int visibility_var = 1;

static int
main_visibility_checkvar (void)
{
  return visibility_varval () != visibility_var
	 && visibility_varptr () != &visibility_var;
}

#ifndef PROTECTED_UNDEF_TEST
int shared_data = 1;
asm (".protected shared_data");

int
shared_func (void)
{
  return 1;
}

asm (".protected shared_func");

extern int * shared_data_p ();
typedef int (*func) ();
extern func shared_func_p ();
#endif
#else
static int
main_visibility_check (void)
{
#ifdef WEAK_TEST
  return visibility_funptr () == NULL;
#else
  return ((int (*) (void)) visibility_funptr ()) == visibility;
#endif
}

static int
main_visibility_checkvar (void)
{
#ifdef WEAK_TEST
  return visibility_varval () == 0
	 && visibility_varptr () == NULL;
#else
  return visibility_varval () == visibility_var
	 && visibility_varptr () == &visibility_var;
#endif
}
#endif

/* This function is called by the shared library.  */

int
main_called (void)
{
  return 6;
}

/* This function overrides a function in the shared library.  */

int
shlib_overriddencall2 (void)
{
  return 8;
}

#ifdef HIDDEN_NORMAL_TEST
int visibility_com;
asm (".hidden visibility_com");

int
main_visibility_checkcom (void)
{
  return visibility_com == 0;
}

int
main_visibility_checkweak (void)
{
  return 1;
}
#elif defined (HIDDEN_WEAK_TEST)
int
main_visibility_checkcom (void)
{
  return 1;
}

#pragma weak visibility_undef_var_weak
extern int visibility_undef_var_weak;
asm (".hidden visibility_undef_var_weak");

#pragma weak visibility_undef_func_weak
extern int visibility_undef_func_weak ();
asm (".hidden visibility_undef_func_weak");

#pragma weak visibility_var_weak
extern int visibility_var_weak;
asm (".hidden visibility_var_weak");

#pragma weak visibility_func_weak
extern int visibility_func_weak ();
asm (".hidden visibility_func_weak");

int
main_visibility_checkweak ()
{
  return &visibility_undef_var_weak == NULL
	 && &visibility_undef_func_weak == NULL
	 && &visibility_func_weak == NULL
	 && &visibility_var_weak == NULL;
}
#elif defined (HIDDEN_UNDEF_TEST)
extern int visibility_def;
asm (".hidden visibility_def");
extern int visibility_func ();
asm (".hidden visibility_func");

int
main_visibility_checkcom (void)
{
  return & visibility_def != NULL && visibility_def == 2;
}

int
main_visibility_checkweak (void)
{
  return & visibility_func != NULL && visibility_func () == 2;
}
#else
int
main_visibility_checkcom (void)
{
  return 1;
}

int
main_visibility_checkweak (void)
{
  return 1;
}
#endif

int
main (void)
{
  int (*p) ();
  int ret = 0;

  printf ("mainvar == %d\n", mainvar);
  printf ("overriddenvar == %d\n", overriddenvar);
  printf ("shlibvar1 == %d\n", shlibvar1);
#ifndef XCOFF_TEST
  printf ("shlib_mainvar () == %d\n", shlib_mainvar ());
  printf ("shlib_overriddenvar () == %d\n", shlib_overriddenvar ());
#endif
  printf ("shlib_shlibvar1 () == %d\n", shlib_shlibvar1 ());
  printf ("shlib_shlibvar2 () == %d\n", shlib_shlibvar2 ());
  printf ("shlib_shlibcall () == %d\n", shlib_shlibcall ());
#ifndef XCOFF_TEST
  printf ("shlib_shlibcall2 () == %d\n", shlib_shlibcall2 ());
  printf ("shlib_maincall () == %d\n", shlib_maincall ());
#endif
  printf ("main_called () == %d\n", main_called ());
  printf ("shlib_checkfunptr1 (shlib_shlibvar1) == %d\n",
	  shlib_checkfunptr1 (shlib_shlibvar1));
#ifndef XCOFF_TEST
  printf ("shlib_checkfunptr2 (main_called) == %d\n",
	  shlib_checkfunptr2 (main_called));
#endif
  p = shlib_getfunptr1 ();
  printf ("shlib_getfunptr1 () ");
  if (p == shlib_shlibvar1)
    printf ("==");
  else
    printf ("!=");
  printf (" shlib_shlibvar1\n");
#ifndef XCOFF_TEST
  p = shlib_getfunptr2 ();
  printf ("shlib_getfunptr2 () ");
  if (p == main_called)
    printf ("==");
  else
    printf ("!=");
  printf (" main_called\n");
#endif
  printf ("shlib_check () == %d\n", shlib_check ());
  printf ("visibility_check () == %d\n", visibility_check ());
  printf ("visibility_checkfunptr () == %d\n",
	  visibility_checkfunptr ());
  printf ("main_visibility_check () == %d\n", main_visibility_check ());
  printf ("visibility_checkvar () == %d\n", visibility_checkvar ());
  printf ("visibility_checkvarptr () == %d\n",
	  visibility_checkvarptr ());
  printf ("main_visibility_checkvar () == %d\n",
	  main_visibility_checkvar ());
  printf ("main_visibility_checkcom () == %d\n",
	  main_visibility_checkcom ());
  printf ("shlib_visibility_checkcom () == %d\n",
	  shlib_visibility_checkcom ());
  printf ("main_visibility_checkweak () == %d\n",
	  main_visibility_checkweak ());
  printf ("shlib_visibility_checkweak () == %d\n",
	  shlib_visibility_checkweak ());

#if !defined (PROTECTED_UNDEF_TEST) && defined (PROTECTED_TEST)
  if (&shared_data != shared_data_p ())
    ret = 1;
  p = shared_func_p ();
  if (shared_func != p)
    ret = 1;
  if (shared_data != *shared_data_p ())
    ret = 1;
  if (shared_func () != (*p) () )
    ret = 1;
#endif

  return ret;
}
#endif
