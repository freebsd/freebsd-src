#ifndef __LOADFUNCS_PROFILE_H__
#define __LOADFUNCS_PROFILE_H__

#include "loadfuncs.h"
#include <profile.h>

#if defined(_WIN64)
#define PROFILE_DLL      "xpprof64.dll"
#else
#define PROFILE_DLL      "xpprof32.dll"
#endif

TYPEDEF_FUNC(
    long,
    KRB5_CALLCONV,
    profile_init,
    (const_profile_filespec_t *files, profile_t *ret_profile)
    );

TYPEDEF_FUNC(
    long,
    KRB5_CALLCONV,
    profile_init_path,
    (const_profile_filespec_list_t filelist, profile_t *ret_profile)
    );

TYPEDEF_FUNC(
    long,
    KRB5_CALLCONV,
    profile_flush,
    (profile_t profile)
    );

TYPEDEF_FUNC(
    void,
    KRB5_CALLCONV,
    profile_abandon,
    (profile_t profile)
    );

TYPEDEF_FUNC(
    void,
    KRB5_CALLCONV,
    profile_release,
    (profile_t profile)
    );

TYPEDEF_FUNC(
    long,
    KRB5_CALLCONV,
    profile_get_values,
    (profile_t profile, const char **names, char ***ret_values)
    );

TYPEDEF_FUNC(
    void,
    KRB5_CALLCONV,
    profile_free_list,
    (char **list)
    );

TYPEDEF_FUNC(
    long,
    KRB5_CALLCONV,
    profile_get_string,
    (profile_t profile, const char *name, const char *subname,
			const char *subsubname, const char *def_val,
			char **ret_string)
    );

TYPEDEF_FUNC(
    long,
    KRB5_CALLCONV,
    profile_get_integer,
    (profile_t profile, const char *name, const char *subname,
			const char *subsubname, int def_val,
			int *ret_default)
    );

TYPEDEF_FUNC(
    long,
    KRB5_CALLCONV,
    profile_get_relation_names,
    (profile_t profile, const char **names, char ***ret_names)
    );

TYPEDEF_FUNC(
    long,
    KRB5_CALLCONV,
    profile_get_subsection_names,
    (profile_t profile, const char **names, char ***ret_names)
    );

TYPEDEF_FUNC(
    long,
    KRB5_CALLCONV,
    profile_iterator_create,
    (profile_t profile, const char **names, int flags, void **ret_iter)
    );

TYPEDEF_FUNC(
    void,
    KRB5_CALLCONV,
    profile_iterator_free,
    (void **iter_p)
    );

TYPEDEF_FUNC(
    long,
    KRB5_CALLCONV,
    profile_iterator,
    (void **iter_p, char **ret_name, char **ret_value)
    );

TYPEDEF_FUNC(
    void,
    KRB5_CALLCONV,
    profile_release_string,
    (char *str)
    );

TYPEDEF_FUNC(
    long,
    KRB5_CALLCONV,
    profile_update_relation,
    (profile_t profile, const char **names, const char *old_value, const char *new_value)
    );

TYPEDEF_FUNC(
    long,
    KRB5_CALLCONV,
    profile_clear_relation,
    (profile_t profile, const char **names)
    );

TYPEDEF_FUNC(
    long,
    KRB5_CALLCONV,
    profile_rename_section,
    (profile_t profile, const char **names, const char *new_name)
    );

TYPEDEF_FUNC(
    long,
    KRB5_CALLCONV,
    profile_add_relation,
    (profile_t profile, const char **names, const char *new_value)
    );


#endif /* __LOADFUNCS_PROFILE_H__ */
