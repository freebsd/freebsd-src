#ifndef CONFIG_H
#define CONFIG_H
@TOP@

/* Name of package.  */
#undef PACKAGE

/* Version of package.  */
#undef VERSION

/* Version string.  */
#undef VERSION_STRING

/* Define if the compiler understands prototypes.  */
#undef PROTOTYPES

/* Define to 1 if NLS is requested.  */
#undef ENABLE_NLS

/* Define as 1 if you have catgets and don't want to use GNU gettext.  */
#undef HAVE_CATGETS

/* Define as 1 if you have gettext and don't want to use GNU gettext.  */
#undef HAVE_GETTEXT

/* Define if your locale.h file contains LC_MESSAGES.  */
#undef HAVE_LC_MESSAGES

/* Define to 1 if you have the stpcpy function.  */
#undef HAVE_STPCPY

/* The location of the simple parser (bison.simple).  */
#undef XPFILE

/* The location of the semantic parser (bison.hairy).  */
#undef XPFILE1

/* The location of the local directory.  */
#undef LOCALEDIR

/* Define as 1 if realloc must be declared even if <stdlib.h> is
   included.  */
#undef NEED_DECLARATION_REALLOC

/* Define as 1 if calloc must be declared even if <stdlib.h> is
   included.  */
#undef NEED_DECLARATION_CALLOC
@BOTTOM@

#if defined(PROTOTYPES) || defined(__cplusplus)
# define PARAMS(p) p
#else
# define PARAMS(p) ()
#endif

#endif  /* CONFIG_H */
