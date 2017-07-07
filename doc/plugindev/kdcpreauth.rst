KDC preauthentication interface (kdcpreauth)
============================================

The kdcpreauth interface allows the addition of KDC support for
preauthentication mechanisms beyond those included in the core MIT
krb5 code base.  For a detailed description of the kdcpreauth
interface, see the header file ``<krb5/kdcpreauth_plugin.h>`` (or
``<krb5/preauth_plugin.h>`` before release 1.12).

A kdcpreauth module is generally responsible for:

* Supplying a list of preauth type numbers used by the module in the
  **pa_type_list** field of the vtable structure.

* Indicating what kind of preauthentication mechanism it implements,
  with the **flags** method.  If the mechanism computes a new reply
  key, it must specify the ``PA_REPLACES_KEY`` flag.  If the mechanism
  is generally only used with hardware tokens, the ``PA_HARDWARE``
  flag allows the mechanism to work with principals which have the
  **requires_hwauth** flag set.

* Producing a padata value to be sent with a preauth_required error,
  with the **edata** method.

* Examining a padata value sent by a client and verifying that it
  proves knowledge of the appropriate client credential information.
  This is done with the **verify** method.

* Producing a padata response value for the client, and possibly
  computing a reply key.  This is done with the **return_padata**
  method.

A module can create and destroy per-KDC state objects by implementing
the **init** and **fini** methods.  Per-KDC state objects have the
type krb5_kdcpreauth_moddata, which is an abstract pointer types.  A
module should typically cast this to an internal type for the state
object.

A module can create a per-request state object by returning one in the
**verify** method, receiving it in the **return_padata** method, and
destroying it in the **free_modreq** method.  Note that these state
objects only apply to the processing of a single AS request packet,
not to an entire authentication exchange (since an authentication
exchange may remain unfinished by the client or may involve multiple
different KDC hosts).  Per-request state objects have the type
krb5_kdcpreauth_modreq, which is an abstract pointer type.

The **edata**, **verify**, and **return_padata** methods have access
to a callback function and handle (called a "rock") which can be used
to get additional information about the current request, including the
maximum allowable clock skew, the client's long-term keys, the
DER-encoded request body, the FAST armor key, string attributes on the
client's database entry, and the client's database entry itself.  The
**verify** method can assert one or more authentication indicators to
be included in the issued ticket using the ``add_auth_indicator``
callback (new in release 1.14).

A module can generate state information to be included with the next
client request using the ``set_cookie`` callback (new in release
1.14).  On the next request, the module can read this state
information using the ``get_cookie`` callback.  Cookie information is
encrypted, timestamped, and transmitted to the client in a
``PA-FX-COOKIE`` pa-data item.  Older clients may not support cookies
and therefore may not transmit the cookie in the next request; in this
case, ``get_cookie`` will not yield the saved information.

If a module implements a mechanism which requires multiple round
trips, its **verify** method can respond with the code
``KRB5KDC_ERR_MORE_PREAUTH_DATA_REQUIRED`` and a list of pa-data in
the *e_data* parameter to be processed by the client.

The **edata** and **verify** methods can be implemented
asynchronously.  Because of this, they do not return values directly
to the caller, but must instead invoke responder functions with their
results.  A synchronous implementation can invoke the responder
function immediately.  An asynchronous implementation can use the
callback to get an event context for use with the libverto_ API.

.. _libverto: https://fedorahosted.org/libverto/
