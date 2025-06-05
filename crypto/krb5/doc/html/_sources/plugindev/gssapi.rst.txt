GSSAPI mechanism interface
==========================

The GSSAPI library in MIT krb5 can load mechanism modules to augment
the set of built-in mechanisms.

.. note: The GSSAPI loadable mechanism interface does not follow the
         normal conventions for MIT krb5 pluggable interfaces.

A mechanism module is a Unix shared object or Windows DLL, built
separately from the krb5 tree.  Modules are loaded according to the
GSS mechanism config files described in :ref:`gssapi_plugin_config`.

For the most part, a GSSAPI mechanism module exports the same
functions as would a GSSAPI implementation itself, with the same
function signatures.  The mechanism selection layer within the GSSAPI
library (called the "mechglue") will dispatch calls from the
application to the module if the module's mechanism is requested.  If
a module does not wish to implement a GSSAPI extension, it can simply
refrain from exporting it, and the mechglue will fail gracefully if
the application calls that function.

The mechglue does not invoke a module's **gss_add_cred**,
**gss_add_cred_from**, **gss_add_cred_impersonate_name**, or
**gss_add_cred_with_password** function.  A mechanism only needs to
implement the "acquire" variants of those functions.

A module does not need to coordinate its minor status codes with those
of other mechanisms.  If the mechglue detects conflicts, it will map
the mechanism's status codes onto unique values, and then map them
back again when **gss_display_status** is called.


NegoEx modules
--------------

Some Windows GSSAPI mechanisms can only be negotiated via a Microsoft
extension to SPNEGO called NegoEx.  Beginning with release 1.18,
mechanism modules can support NegoEx as follows:

* Implement the gssspi_query_meta_data(), gssspi_exchange_meta_data(),
  and gssspi_query_mechanism_info() SPIs declared in
  ``<gssapi/gssapi_ext.h>``.

* Implement gss_inquire_sec_context_by_oid() and answer the
  **GSS_C_INQ_NEGOEX_KEY** and **GSS_C_INQ_NEGOEX_VERIFY_KEY** OIDs
  to provide the checksum keys for outgoing and incoming checksums,
  respectively.  The answer must be in two buffers: the first buffer
  contains the key contents, and the second buffer contains the key
  encryption type as a four-byte little-endian integer.

By default, NegoEx mechanisms will not be directly negotiated via
SPNEGO.  If direct SPNEGO negotiation is required for
interoperability, implement gss_inquire_attrs_for_mech() and assert
the GSS_C_MA_NEGOEX_AND_SPNEGO attribute (along with any applicable
RFC 5587 attributes).


Interposer modules
------------------

The mechglue also supports a kind of loadable module, called an
interposer module, which intercepts calls to existing mechanisms
rather than implementing a new mechanism.

An interposer module must export the symbol **gss_mech_interposer**
with the following signature::

    gss_OID_set gss_mech_interposer(gss_OID mech_type);

This function is invoked with the OID of the interposer mechanism as
specified in the mechanism config file, and returns a set of mechanism
OIDs to be interposed.  The returned OID set must have been created
using the mechglue's gss_create_empty_oid_set and
gss_add_oid_set_member functions.

An interposer module must use the prefix ``gssi_`` for the GSSAPI
functions it exports, instead of the prefix ``gss_``.  In most cases,
unexported ``gssi_`` functions will result in failure from their
corresponding ``gss_`` calls.

An interposer module can link against the GSSAPI library in order to
make calls to the original mechanism.  To do so, it must specify a
special mechanism OID which is the concatention of the interposer's
own OID byte string and the original mechanism's OID byte string.

Functions that do not accept a mechanism argument directly require no
special handling, with the following exceptions:

Since **gss_accept_sec_context** does not accept a mechanism argument,
an interposer mechanism must, in order to invoke the original
mechanism's function, acquire a credential for the concatenated OID
and pass that as the *verifier_cred_handle* parameter.

Since **gss_import_name**, **gss_import_cred**, and
**gss_import_sec_context** do not accept mechanism parameters, the SPI
has been extended to include variants which do.  This allows the
interposer module to know which mechanism should be used to interpret
the token.  These functions have the following signatures::

    OM_uint32 gssi_import_sec_context_by_mech(OM_uint32 *minor_status,
        gss_OID desired_mech, gss_buffer_t interprocess_token,
        gss_ctx_id_t *context_handle);

    OM_uint32 gssi_import_name_by_mech(OM_uint32 *minor_status,
        gss_OID mech_type, gss_buffer_t input_name_buffer,
        gss_OID input_name_type, gss_name_t output_name);

    OM_uint32 gssi_import_cred_by_mech(OM_uint32 *minor_status,
        gss_OID mech_type, gss_buffer_t token,
        gss_cred_id_t *cred_handle);

To re-enter the original mechanism when importing tokens for the above
functions, the interposer module must wrap the mechanism token in the
mechglue's format, using the concatenated OID (except in
**gss_import_name**).  The mechglue token formats are:

* For **gss_import_sec_context**, a four-byte OID length in big-endian
  order, followed by the concatenated OID, followed by the mechanism
  token.

* For **gss_import_name**, the bytes 04 01, followed by a two-byte OID
  length in big-endian order, followed by the mechanism OID, followed
  by a four-byte token length in big-endian order, followed by the
  mechanism token.  Unlike most uses of OIDs in the API, the mechanism
  OID encoding must include the DER tag and length for an object
  identifier (06 followed by the DER length of the OID byte string),
  and this prefix must be included in the two-byte OID length.
  input_name_type must also be set to GSS_C_NT_EXPORT_NAME.

* For **gss_import_cred**, a four-byte OID length in big-endian order,
  followed by the concatenated OID, followed by a four-byte token
  length in big-endian order, followed by the mechanism token.  This
  sequence may be repeated multiple times.
