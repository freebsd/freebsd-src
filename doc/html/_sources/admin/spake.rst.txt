.. _spake:

SPAKE Preauthentication
=======================

SPAKE preauthentication (added in release 1.17) uses public key
cryptography techniques to protect against :ref:`password dictionary
attacks <dictionary>`.  Unlike :ref:`PKINIT <pkinit>`, it does not
require any additional infrastructure such as certificates; it simply
needs to be turned on.  Using SPAKE preauthentication may modestly
increase the CPU and network load on the KDC.

SPAKE preauthentication can use one of four elliptic curve groups for
its password-authenticated key exchange.  The recommended group is
``edwards25519``; three NIST curves (``P-256``, ``P-384``, and
``P-521``) are also supported.

By default, SPAKE with the ``edwards25519`` group is enabled on
clients, but the KDC does not offer SPAKE by default.  To turn it on,
set the **spake_preauth_groups** variable in :ref:`libdefaults` to a
list of allowed groups.  This variable affects both the client and the
KDC.  Simply setting it to ``edwards25519`` is recommended::

    [libdefaults]
        spake_preauth_groups = edwards25519

Set the **+requires_preauth** and **-allow_svr** flags on client
principal entries, as you would for any preauthentication mechanism::

    kadmin: modprinc +requires_preauth -allow_svr PRINCNAME

Clients which do not implement SPAKE preauthentication will fall back
to encrypted timestamp.

An active attacker can force a fallback to encrypted timestamp by
modifying the initial KDC response, defeating the protection against
dictionary attacks.  To prevent this fallback on clients which do
implement SPAKE preauthentication, set the
**disable_encrypted_timestamp** variable to ``true`` in the
:ref:`realms` subsection for realms whose KDCs offer SPAKE
preauthentication.

By default, SPAKE preauthentication requires an extra network round
trip to the KDC during initial authentication.  If most of the clients
in a realm support SPAKE, this extra round trip can be eliminated
using an optimistic challenge, by setting the
**spake_preauth_kdc_challenge** variable in :ref:`kdcdefaults` to a
single group name::

    [kdcdefaults]
        spake_preauth_kdc_challenge = edwards25519

Using optimistic challenge will cause the KDC to do extra work for
initial authentication requests that do not result in SPAKE
preauthentication, but will save work when SPAKE preauthentication is
used.
