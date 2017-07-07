.. _otp_preauth:

OTP Preauthentication
=====================

OTP is a preauthentication mechanism for Kerberos 5 which uses One
Time Passwords (OTP) to authenticate the client to the KDC.  The OTP
is passed to the KDC over an encrypted FAST channel in clear-text.
The KDC uses the password along with per-user configuration to proxy
the request to a third-party RADIUS system.  This enables
out-of-the-box compatibility with a large number of already widely
deployed proprietary systems.

Additionally, our implementation of the OTP system allows for the
passing of RADIUS requests over a UNIX domain stream socket.  This
permits the use of a local companion daemon which can handle the
details of authentication.


Defining token types
--------------------

Token types are defined in either :ref:`krb5.conf(5)` or
:ref:`kdc.conf(5)` according to the following format::

    [otp]
        <name> = {
            server = <host:port or filename> (default: see below)
            secret = <filename>
            timeout = <integer> (default: 5 [seconds])
            retries = <integer> (default: 3)
            strip_realm = <boolean> (default: true)
            indicator = <string> (default: none)
        }

If the server field begins with '/', it will be interpreted as a UNIX
socket.  Otherwise, it is assumed to be in the format host:port.  When
a UNIX domain socket is specified, the secret field is optional and an
empty secret is used by default.  If the server field is not
specified, it defaults to |kdcrundir|\ ``/<name>.socket``.

When forwarding the request over RADIUS, by default the principal is
used in the User-Name attribute of the RADIUS packet.  The strip_realm
parameter controls whether the principal is forwarded with or without
the realm portion.

If an indicator field is present, tickets issued using this token type
will be annotated with the specified authentication indicator (see
:ref:`auth_indicator`).  This key may be specified multiple times to
add multiple indicators.


The default token type
----------------------

A default token type is used internally when no token type is specified for a
given user.  It is defined as follows::

    [otp]
        DEFAULT = {
            strip_realm = false
        }

The administrator may override the internal ``DEFAULT`` token type
simply by defining a configuration with the same name.


Token instance configuration
----------------------------

To enable OTP for a client principal, the administrator must define
the **otp** string attribute for that principal.  (See
:ref:`set_string`.)  The **otp** user string is a JSON string of the
format:

.. code-block:: xml

    [{
        "type": <string>,
        "username": <string>,
        "indicators": [<string>, ...]
     }, ...]

This is an array of token objects.  Both fields of token objects are
optional.  The **type** field names the token type of this token; if
not specified, it defaults to ``DEFAULT``.  The **username** field
specifies the value to be sent in the User-Name RADIUS attribute.  If
not specified, the principal name is sent, with or without realm as
defined in the token type.  The **indicators** field specifies a list
of authentication indicators to annotate tickets with, overriding any
indicators specified in the token type.

For ease of configuration, an empty array (``[]``) is treated as
equivalent to one DEFAULT token (``[{}]``).


Other considerations
--------------------

#. FAST is required for OTP to work.
