/* $Id: ncurses_dll.h,v 1.1 2000/12/10 00:05:35 tom Exp $ */


#if defined(__CYGWIN__)
#  if defined(NCURSES_DLL)
#    if defined(NCURSES_STATIC)
#      undef NCURSES_STATIC
#    endif
#  endif
#  undef NCURSES_IMPEXP
#  undef NCURSES_API
#  undef NCURSES_EXPORT(type)
#  undef NCURSES_EXPORT_VAR(type)
#  if defined(NCURSES_DLL)
/* building a DLL */
#    define NCURSES_IMPEXP __declspec(dllexport)
#  elif defined(NCURSES_STATIC)
/* building or linking to a static library */
#    define NCURSES_IMPEXP /* nothing */
#  else
/* linking to the DLL */
#    define NCURSES_IMPEXP __declspec(dllimport)
#  endif
#  define NCURSES_API __cdecl
#  define NCURSES_EXPORT(type) NCURSES_IMPEXP type NCURSES_API
#  define NCURSES_EXPORT_VAR(type) NCURSES_IMPEXP type
#endif

/* Take care of non-cygwin platforms */
#if !defined(NCURSES_IMPEXP)
#  define NCURSES_IMPEXP /* nothing */
#endif
#if !defined(NCURSES_API)
#  define NCURSES_API /* nothing */
#endif
#if !defined(NCURSES_EXPORT)
#  define NCURSES_EXPORT(type) NCURSES_IMPEXP type NCURSES_API
#endif
#if !defined(NCURSES_EXPORT_VAR)
#  define NCURSES_EXPORT_VAR(type) NCURSES_IMPEXP type
#endif
