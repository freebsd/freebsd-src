.. _certauth_plugin:

PKINIT certificate authorization interface (certauth)
=====================================================

The certauth interface was first introduced in release 1.16.  It
allows customization of the X.509 certificate attribute requirements
placed on certificates used by PKINIT enabled clients.  For a detailed
description of the certauth interface, see the header file
``<krb5/certauth_plugin.h>``

A certauth module implements the **authorize** method to determine
whether a client's certificate is authorized to authenticate a client
principal.  **authorize** receives the DER-encoded certificate, the
requested client principal, and a pointer to the client's
krb5_db_entry (for modules that link against libkdb5).  The method
must decode the certificate and inspect its attributes to determine if
it should authorize PKINIT authentication.  It returns the
authorization status and optionally outputs a list of authentication
indicator strings to be added to the ticket.

Beginning in release 1.19, the authorize method can request that the
hardware authentication bit be set in the ticket by returning
**KRB5_CERTAUTH_HWAUTH**.  Beginning in release 1.20, the authorize
method can return **KRB5_CERTAUTH_HWAUTH_PASS** to request that the
hardware authentication bit be set in the ticket but otherwise defer
authorization to another certauth module.  A module must use its own
internal or library-provided ASN.1 certificate decoder.

A module can optionally create and destroy module data with the
**init** and **fini** methods.  Module data objects last for the
lifetime of the KDC process.

If a module allocates and returns a list of authentication indicators
from **authorize**, it must also implement the **free_ind** method
to free the list.
