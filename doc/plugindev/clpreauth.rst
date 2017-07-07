Client preauthentication interface (clpreauth)
==============================================

During an initial ticket request, a KDC may ask a client to prove its
knowledge of the password before issuing an encrypted ticket, or to
use credentials other than a password.  This process is called
preauthentication, and is described in :rfc:`4120` and :rfc:`6113`.
The clpreauth interface allows the addition of client support for
preauthentication mechanisms beyond those included in the core MIT
krb5 code base.  For a detailed description of the clpreauth
interface, see the header file ``<krb5/clpreauth_plugin.h>`` (or
``<krb5/preauth_plugin.h>`` before release 1.12).

A clpreauth module is generally responsible for:

* Supplying a list of preauth type numbers used by the module in the
  **pa_type_list** field of the vtable structure.

* Indicating what kind of preauthentication mechanism it implements,
  with the **flags** method.  In the most common case, this method
  just returns ``PA_REAL``, indicating that it implements a normal
  preauthentication type.

* Examining the padata information included in a PREAUTH_REQUIRED or
  MORE_PREAUTH_DATA_REQUIRED error and producing padata values for the
  next AS request.  This is done with the **process** method.

* Examining the padata information included in a successful ticket
  reply, possibly verifying the KDC identity and computing a reply
  key.  This is also done with the **process** method.

* For preauthentication types which support it, recovering from errors
  by examining the error data from the KDC and producing a padata
  value for another AS request.  This is done with the **tryagain**
  method.

* Receiving option information (supplied by ``kinit -X`` or by an
  application), with the **gic_opts** method.

A clpreauth module can create and destroy per-library-context and
per-request state objects by implementing the **init**, **fini**,
**request_init**, and **request_fini** methods.  Per-context state
objects have the type krb5_clpreauth_moddata, and per-request state
objects have the type krb5_clpreauth_modreq.  These are abstract
pointer types; a module should typically cast these to internal
types for the state objects.

The **process** and **tryagain** methods have access to a callback
function and handle (called a "rock") which can be used to get
additional information about the current request, including the
expected enctype of the AS reply, the FAST armor key, and the client
long-term key (prompting for the user password if necessary).  A
callback can also be used to replace the AS reply key if the
preauthentication mechanism computes one.
