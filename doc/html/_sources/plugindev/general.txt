General plugin concepts
=======================

A krb5 dynamic plugin module is a Unix shared object or Windows DLL.
Typically, the source code for a dynamic plugin module should live in
its own project with a build system using automake_ and libtool_, or
tools with similar functionality.

A plugin module must define a specific symbol name, which depends on
the pluggable interface and module name.  For most pluggable
interfaces, the exported symbol is a function named
``INTERFACE_MODULE_initvt``, where *INTERFACE* is the name of the
pluggable interface and *MODULE* is the name of the module.  For these
interfaces, it is possible for one shared object or DLL to implement
multiple plugin modules, either for the same pluggable interface or
for different ones.  For example, a shared object could implement both
KDC and client preauthentication mechanisms, by exporting functions
named ``kdcpreauth_mymech_initvt`` and ``clpreauth_mymech_initvt``.

.. note: The profile, locate, and GSSAPI mechglue pluggable interfaces
         follow different conventions.  See the documentation for
         those interfaces for details.  The remainder of this section
         applies to pluggable interfaces which use the standard
         conventions.

A plugin module implementation should include the header file
``<krb5/INTERFACE_plugin.h>``, where *INTERFACE* is the name of the
pluggable interface.  For instance, a ccselect plugin module
implementation should use ``#include <krb5/ccselect_plugin.h>``.

.. note: clpreauth and kdcpreauth module implementations should
         include <krb5/preauth_plugin.h>.

initvt functions have the following prototype::

    krb5_error_code interface_modname_initvt(krb5_context context,
                                             int maj_ver, int min_ver,
                                             krb5_plugin_vtable vtable);

and should do the following:

1. Check that the supplied maj_ver argument is supported by the
   module.  If it is not supported, the function should return
   KRB5_PLUGIN_VER_NOTSUPP.

2. Cast the supplied vtable pointer to the structure type
   corresponding to the major version, as documented in the pluggable
   interface header file.

3. Fill in the structure fields with pointers to method functions and
   static data, stopping at the field indicated by the supplied minor
   version.  Fields for unimplemented optional methods can be left
   alone; it is not necessary to initialize them to NULL.

In most cases, the context argument will not be used.  The initvt
function should not allocate memory; think of it as a glorified
structure initializer.  Each pluggable interface defines methods for
allocating and freeing module state if doing so is necessary for the
interface.

Pluggable interfaces typically include a **name** field in the vtable
structure, which should be filled in with a pointer to a string
literal containing the module name.

Here is an example of what an initvt function might look like for a
fictional pluggable interface named fences, for a module named
"wicker"::

    krb5_error_code
    fences_wicker_initvt(krb5_context context, int maj_ver,
                         int min_ver, krb5_plugin_vtable vtable)
    {
        krb5_ccselect_vtable vt;

        if (maj_ver == 1) {
            krb5_fences_vtable vt = (krb5_fences_vtable)vtable;
            vt->name = "wicker";
            vt->slats = wicker_slats;
            vt->braces = wicker_braces;
        } else if (maj_ver == 2) {
            krb5_fences_vtable_v2 vt = (krb5_fences_vtable_v2)vtable;
            vt->name = "wicker";
            vt->material = wicker_material;
            vt->construction = wicker_construction;
            if (min_ver < 2)
                return 0;
            vt->footing = wicker_footing;
            if (min_ver < 3)
                return 0;
            vt->appearance = wicker_appearance;
        } else {
            return KRB5_PLUGIN_VER_NOTSUPP;
        }
        return 0;
    }

.. _automake: http://www.gnu.org/software/automake/
.. _libtool: http://www.gnu.org/software/libtool/
