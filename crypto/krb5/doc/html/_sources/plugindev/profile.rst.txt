.. _profile_plugin:

Configuration interface (profile)
=================================

The profile interface allows a module to control how krb5
configuration information is obtained by the Kerberos library and
applications.  For a detailed description of the profile interface,
see the header file ``<profile.h>``.

.. note::

          The profile interface does not follow the normal conventions
          for MIT krb5 pluggable interfaces, because it is part of a
          lower-level component of the krb5 library.

As with other types of plugin modules, a profile module is a Unix
shared object or Windows DLL, built separately from the krb5 tree.
The krb5 library will dynamically load and use a profile plugin module
if it reads a ``module`` directive at the beginning of krb5.conf, as
described in :ref:`profile_plugin_config`.

A profile module exports a function named ``profile_module_init``
matching the signature of the profile_module_init_fn type.  This
function accepts a residual string, which may be used to help locate
the configuration source.  The function fills in a vtable and may also
create a per-profile state object.  If the module uses state objects,
it should implement the **copy** and **cleanup** methods to manage
them.

A basic read-only profile module need only implement the
**get_values** and **free_values** methods.  The **get_values** method
accepts a null-terminated list of C string names (e.g., an array
containing "libdefaults", "clockskew", and NULL for the **clockskew**
variable in the :ref:`libdefaults` section) and returns a
null-terminated list of values, which will be cleaned up with the
**free_values** method when the caller is done with them.

Iterable profile modules must also define the **iterator_create**,
**iterator**, **iterator_free**, and **free_string** methods.  The
core krb5 code does not require profiles to be iterable, but some
applications may iterate over the krb5 profile object in order to
present configuration interfaces.

Writable profile modules must also define the **writable**,
**modified**, **update_relation**, **rename_section**,
**add_relation**, and **flush** methods.  The core krb5 code does not
require profiles to be writable, but some applications may write to
the krb5 profile in order to present configuration interfaces.

The following is an example of a very basic read-only profile module
which returns a hardcoded value for the **default_realm** variable in
:ref:`libdefaults`, and provides no other configuration information.
(For conciseness, the example omits code for checking the return
values of malloc and strdup.) ::

    #include <stdlib.h>
    #include <string.h>
    #include <profile.h>

    static long
    get_values(void *cbdata, const char *const *names, char ***values)
    {
        if (names[0] != NULL && strcmp(names[0], "libdefaults") == 0 &&
            names[1] != NULL && strcmp(names[1], "default_realm") == 0) {
            *values = malloc(2 * sizeof(char *));
            (*values)[0] = strdup("ATHENA.MIT.EDU");
            (*values)[1] = NULL;
            return 0;
        }
        return PROF_NO_RELATION;
    }

    static void
    free_values(void *cbdata, char **values)
    {
        char **v;

        for (v = values; *v; v++)
            free(*v);
        free(values);
    }

    long
    profile_module_init(const char *residual, struct profile_vtable *vtable,
                        void **cb_ret);

    long
    profile_module_init(const char *residual, struct profile_vtable *vtable,
                        void **cb_ret)
    {
        *cb_ret = NULL;
        vtable->get_values = get_values;
        vtable->free_values = free_values;
        return 0;
    }
