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
enctypes.  Starting in krb5-1.21, all services are assumed to support
aes256-cts-hmac-sha1-96; also, des3-cbc-sha1 and arcfour-hmac session
keys will not be issued by default.

Starting in krb5-1.11, it is possible to set a string attribute on a
service principal to control what session key enctypes the KDC may
issue for service tickets for that principal, overriding the service's
long-term keys and the assumption of aes256-cts-hmac-sha1-96 support.
See :ref:`set_string` in :ref:`kadmin(1)` for details.


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
    weak enctypes from **permitted_enctypes**,
    **default_tkt_enctypes**, and **default_tgs_enctypes**.  Do not
    set this to *true* unless the use of weak enctypes is an
    acceptable risk for your environment and the weak enctypes are
    required for backward compatibility.

**allow_des3**
    was added in release 1.21 and defaults to *false*.  Unless this
    flag is set to *true*, the KDC will not issue tickets with
    des3-cbc-sha1 session keys.  In a future release, this flag will
    control whether des3-cbc-sha1 is permitted in similar fashion to
    weak enctypes.

**allow_rc4**
    was added in release 1.21 and defaults to *false*.  Unless this
    flag is set to *true*, the KDC will not issue tickets with
    arcfour-hmac session keys.  In a future release, this flag will
    control whether arcfour-hmac is permitted in similar fashion to
    weak enctypes.

**permitted_enctypes**
    controls the set of enctypes that a service will permit for
    session keys and for ticket and authenticator encryption.  The KDC
    and other programs that access the Kerberos database will ignore
    keys of non-permitted enctypes.  Starting in release 1.18, this
    setting also acts as the default for **default_tkt_enctypes** and
    **default_tgs_enctypes**.

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

========================== ========== ======== =======
enctype                    weak?      krb5     Windows
========================== ========== ======== =======
des-cbc-crc                weak       <1.18    >=2000
des-cbc-md4                weak       <1.18    ?
des-cbc-md5                weak       <1.18    >=2000
des3-cbc-sha1              deprecated >=1.1    none
arcfour-hmac               deprecated >=1.3    >=2000
arcfour-hmac-exp           weak       >=1.3    >=2000
aes128-cts-hmac-sha1-96               >=1.3    >=Vista
aes256-cts-hmac-sha1-96               >=1.3    >=Vista
aes128-cts-hmac-sha256-128            >=1.15   none
aes256-cts-hmac-sha384-192            >=1.15   none
camellia128-cts-cmac                  >=1.9    none
camellia256-cts-cmac                  >=1.9    none
========================== ========== ======== =======

krb5 releases 1.18 and later do not support single-DES.  krb5 releases
1.8 and later disable the single-DES enctypes by default.  Microsoft
Windows releases Windows 7 and later disable single-DES enctypes by
default.

krb5 releases 1.17 and later flag deprecated encryption types
(including ``des3-cbc-sha1`` and ``arcfour-hmac``) in KDC logs and
kadmin output.  krb5 release 1.19 issues a warning during initial
authentication if ``des3-cbc-sha1`` is used.  Future releases will
disable ``des3-cbc-sha1`` by default and eventually remove support for
it.


Migrating away from older encryption types
------------------------------------------

Administrator intervention may be required to migrate a realm away
from legacy encryption types, especially if the realm was created
using krb5 release 1.2 or earlier.  This migration should be performed
before upgrading to krb5 versions which disable or remove support for
legacy encryption types.

If there is a **supported_enctypes** setting in :ref:`kdc.conf(5)` on
the KDC, make sure that it does not include weak or deprecated
encryption types.  This will ensure that newly created keys do not use
those encryption types by default.

Check the ``krbtgt/REALM`` principal using the :ref:`kadmin(1)`
**getprinc** command.  If it lists a weak or deprecated encryption
type as the first key, it must be migrated using the procedure in
:ref:`changing_krbtgt_key`.

Check the ``kadmin/history`` principal, which should have only one key
entry.  If it uses a weak or deprecated encryption type, it should be
upgraded following the notes in :ref:`updating_history_key`.

Check the other kadmin principals: kadmin/changepw, kadmin/admin, and
any kadmin/hostname principals that may exist.  These principals can
be upgraded with **change_password -randkey** in kadmin.

Check the ``K/M`` entry.  If it uses a weak or deprecated encryption
type, it should be upgraded following the procedure in
:ref:`updating_master_key`.

User and service principals using legacy encryption types can be
enumerated with the :ref:`kdb5_util(8)` **tabdump keyinfo** command.

Service principals can be migrated with a keytab rotation on the
service host, which can be accomplished using the :ref:`k5srvutil(1)`
**change** and **delold** commands.  Allow enough time for existing
tickets to expire between the change and delold operations.

User principals with password-based keys can be migrated with a
password change.  The realm administrator can set a password
expiration date using the :ref:`kadmin(1)` **modify_principal
-pwexpire** command to force a password change.

If a legacy encryption type has not yet been disabled by default in
the version of krb5 running on the KDC, it can be disabled
administratively with the **permitted_enctypes** variable.  For
example, setting **permitted_enctypes** to ``DEFAULT -des3 -rc4`` will
cause any database keys of the triple-DES and RC4 encryption types to
be ignored.
