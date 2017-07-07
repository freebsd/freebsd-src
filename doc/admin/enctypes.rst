.. _enctypes:

Encryption types
================

Kerberos can use a variety of cipher algorithms to protect data.  A
Kerberos **encryption type** (also known as an **enctype**) is a
specific combination of a cipher algorithm with an integrity algorithm
to provide both confidentiality and integrity to data.


Enctypes in requests
--------------------

Clients make two types of requests (KDC-REQ) to the KDC: AS-REQs and
TGS-REQs.  The client uses the AS-REQ to obtain initial tickets
(typically a Ticket-Granting Ticket (TGT)), and uses the TGS-REQ to
obtain service tickets.

The KDC uses three different keys when issuing a ticket to a client:

* The long-term key of the service: the KDC uses this to encrypt the
  actual service ticket.  The KDC only uses the first long-term key in
  the most recent kvno for this purpose.

* The session key: the KDC randomly chooses this key and places one
  copy inside the ticket and the other copy inside the encrypted part
  of the reply.

* The reply-encrypting key: the KDC uses this to encrypt the reply it
  sends to the client.  For AS replies, this is a long-term key of the
  client principal.  For TGS replies, this is either the session key of the
  authenticating ticket, or a subsession key.

Each of these keys is of a specific enctype.

Each request type allows the client to submit a list of enctypes that
it is willing to accept.  For the AS-REQ, this list affects both the
session key selection and the reply-encrypting key selection.  For the
TGS-REQ, this list only affects the session key selection.


.. _session_key_selection:

Session key selection
---------------------

The KDC chooses the session key enctype by taking the intersection of
its **permitted_enctypes** list, the list of long-term keys for the
most recent kvno of the service, and the client's requested list of
enctypes.  If **allow_weak_crypto** is true, all services are assumed
to support des-cbc-crc.

Starting in krb5-1.11, **des_crc_session_supported** in
:ref:`kdc.conf(5)` allows additional control over whether the KDC
issues des-cbc-crc session keys.

Also starting in krb5-1.11, it is possible to set a string attribute
on a service principal to control what session key enctypes the KDC
may issue for service tickets for that principal.  See
:ref:`set_string` in :ref:`kadmin(1)` for details.


Choosing enctypes for a service
-------------------------------

Generally, a service should have a key of the strongest
enctype that both it and the KDC support.  If the KDC is running a
release earlier than krb5-1.11, it is also useful to generate an
additional key for each enctype that the service can support.  The KDC
will only use the first key in the list of long-term keys for encrypting
the service ticket, but the additional long-term keys indicate the
other enctypes that the service supports.

As noted above, starting with release krb5-1.11, there are additional
configuration settings that control session key enctype selection
independently of the set of long-term keys that the KDC has stored for
a service principal.


Configuration variables
-----------------------

The following ``[libdefaults]`` settings in :ref:`krb5.conf(5)` will
affect how enctypes are chosen.

**allow_weak_crypto**
    defaults to *false* starting with krb5-1.8.  When *false*, removes
    single-DES enctypes (and other weak enctypes) from
    **permitted_enctypes**, **default_tkt_enctypes**, and
    **default_tgs_enctypes**.  Do not set this to *true* unless the
    use of weak enctypes is an acceptable risk for your environment
    and the weak enctypes are required for backward compatibility.

**permitted_enctypes**
    controls the set of enctypes that a service will accept as session
    keys.

**default_tkt_enctypes**
    controls the default set of enctypes that the Kerberos client
    library requests when making an AS-REQ.  Do not set this unless
    required for specific backward compatibility purposes; stale
    values of this setting can prevent clients from taking advantage
    of new stronger enctypes when the libraries are upgraded.

**default_tgs_enctypes**
    controls the default set of enctypes that the Kerberos client
    library requests when making a TGS-REQ.  Do not set this unless
    required for specific backward compatibility purposes; stale
    values of this setting can prevent clients from taking advantage
    of new stronger enctypes when the libraries are upgraded.

The following per-realm setting in :ref:`kdc.conf(5)` affects the
generation of long-term keys.

**supported_enctypes**
    controls the default set of enctype-salttype pairs that :ref:`kadmind(8)`
    will use for generating long-term keys, either randomly or from
    passwords


Enctype compatibility
---------------------

See :ref:`Encryption_types` for additional information about enctypes.

========================== ===== ======== =======
enctype                    weak? krb5     Windows
========================== ===== ======== =======
des-cbc-crc                weak  all      >=2000
des-cbc-md4                weak  all      ?
des-cbc-md5                weak  all      >=2000
des3-cbc-sha1                    >=1.1    none
arcfour-hmac                     >=1.3    >=2000
arcfour-hmac-exp           weak  >=1.3    >=2000
aes128-cts-hmac-sha1-96          >=1.3    >=Vista
aes256-cts-hmac-sha1-96          >=1.3    >=Vista
aes128-cts-hmac-sha256-128       >=1.15   none
aes256-cts-hmac-sha384-192       >=1.15   none
camellia128-cts-cmac             >=1.9    none
camellia256-cts-cmac             >=1.9    none
========================== ===== ======== =======

krb5 releases 1.8 and later disable the single-DES enctypes by
default.  Microsoft Windows releases Windows 7 and later disable
single-DES enctypes by default.
