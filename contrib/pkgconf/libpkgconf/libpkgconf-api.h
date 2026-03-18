#ifndef LIBPKGCONF_LIBPKGCONF_API_H
#define LIBPKGCONF_LIBPKGCONF_API_H

/* Makefile.am specifies visibility using the libtool option -export-symbols-regex '^pkgconf_'
 * Unfortunately, that is not available when building with meson, so use attributes instead.
 */
#if defined(PKGCONFIG_IS_STATIC)
# define PKGCONF_API
#elif defined(_WIN32) || defined(_WIN64)
# if defined(LIBPKGCONF_EXPORT) || defined(DLL_EXPORT)
#  define PKGCONF_API __declspec(dllexport)
# else
#  define PKGCONF_API __declspec(dllimport)
# endif
#else
# define PKGCONF_API __attribute__((visibility("default")))
#endif

#endif
