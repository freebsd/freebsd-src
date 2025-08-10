Kerberos Database (KDB) Formats
===============================

Dump format
-----------

Files created with the :ref:`kdb5_util(8)` **dump** command begin with
a versioned header "kdb5_util load_dump version 7".  This version has
been in use since MIT krb5 release 1.11; some previous versions are
supported but are not described here.

Each subsequent line of the dump file contains one or more
tab-separated fields describing either a principal entry or a policy
entry.  The fields of a principal entry line are:

* the word "princ"
* the string "38" (this was originally a length field)
* the length of the principal name in string form
* the decimal number of tag-length data elements
* the decimal number of key-data elements
* the string "0" (this was originally an extension length field)
* the principal name in string form
* the principal attributes as a decimal number; when converted to
  binary, the bits from least significant to most significant are:

  - disallow_postdated
  - disallow_forwardable
  - disallow_tgt_based
  - disallow_renewable
  - disallow_proxiable
  - disallow_dup_skey
  - disallow_all_tix
  - requires_preauth
  - requires_hwauth
  - requires_pwchange
  - disallow_svr
  - pwchange_service
  - support_desmd5
  - new_princ
  - ok_as_delegate
  - ok_to_auth_as_delegate
  - no_auth_data_required
  - lockdown_keys

* the maximum ticket lifetime, as a decimal number of seconds
* the maximum renewable ticket lifetime, as a decimal number of seconds
* the principal expiration time, as a decimal POSIX timestamp
* the password expiration time, as a decimal POSIX timestamp
* the last successful authentication time, as a decimal POSIX
  timestamp
* the last failed authentication time, as a decimal POSIX timestamp
* the decimal number of failed authentications since the last
  successful authentication time
* for each tag-length data value:

  - the tag value in decimal
  - the length in decimal
  - the data as a lowercase hexadecimal byte string, or "-1" if the length is 0

* for each key-data element:

  - the string "2" if this element has non-normal salt type, "1"
    otherwise
  - the key version number of this element
  - the encryption type
  - the length of the encrypted key value
  - the encrypted key as a lowercase hexadecimal byte string
  - if this element has non-normal salt type:

    - the salt type
    - the length of the salt data
    - the salt data as a lowercase hexadecimal byte string, or the
      string "-1" if the salt data length is 0

* the string "-1;" (this was originally an extension field)

The fields of a policy entry line are:

* the string "policy"
* the policy name
* the minimum password lifetime as a decimal number of seconds
* the maximum password lifetime as a decimal number of seconds
* the minimum password length, in decimal
* the minimum number of character classes, in decimal
* the number of historical keys to be stored, in decimal
* the policy reference count (no longer used)
* the maximum number of failed authentications before lockout
* the time interval after which the failed authentication count is
  reset, as a decimal number of seconds
* the lockout duration, as a decimal number of seconds
* the required principal attributes, in decimal (currently unenforced)
* the maximum ticket lifetime as a decimal number of seconds
  (currently unenforced)
* the maximum renewable lifetime as a decimal number of seconds
  (currently unenforced)
* the allowed key/salt types, or "-" if unrestricted
* the number of tag-length values
* for each tag-length data value:

  - the tag value in decimal
  - the length in decimal
  - the data as a lowercase hexadecimal byte string, or "-1" if the
    length is 0


Tag-length data formats
-----------------------

The currently defined tag-length data types are:

* (1) last password change: a four-byte little-endian POSIX timestamp
  giving the last password change time
* (2) last modification data: a four-byte little-endian POSIX
  timestamp followed by a zero-terminated principal name in string
  form, giving the time of the last principal change and the principal
  who performed it
* (3) kadmin data: the XDR encoding of a per-principal kadmin data
  record (see below)
* (8) master key version: a two-byte little-endian integer containing
  the master key version used to encrypt this principal's key data
* (9) active kvno: see below
* (10) master key auxiliary data: see below
* (11) string attributes: one or more iterations of a zero-terminated
  string key followed by a zero-terminated string value
* (12) alias target principal: a zero-terminated principal name in
  string form
* (255) LDAP object information: see below
* (768) referral padata: a DER-encoded PA-SVR-REFERRAL-DATA to be sent
  to a TGS-REQ client within encrypted padata (see Appendix A of
  :rfc:`1606`)
* (1792) last admin unlock: a four-byte little-endian POSIX timestamp
  giving the time of the last administrative account unlock
* (32767) database arguments: a zero-terminated key=value string (may
  appear multiple times); used by the kadmin protocol to
  communicate -x arguments to kadmind

Per-principal kadmin data
~~~~~~~~~~~~~~~~~~~~~~~~~

Per-principal kadmin data records use a modified XDR encoding of the
kadmin_data type defined as follows:

.. code-block:: c

    struct key_data {
        int numfields;
        unsigned int kvno;
        int enctype;
        int salttype;
        unsigned int keylen;
        unsigned int saltlen;
        opaque key<>;
        opaque salt<>;
    };

    struct hist_entry {
        key_data keys<>;
    };

    struct kadmin_data {
        int version_number;
        nullstring policy;
        int aux_attributes;
        unsigned int old_key_next;
        unsigned int admin_history_kvno;
        hist_entry old_keysets<>;
    };

The type "nullstring" uses a custom string encoder where the length
field is zero or the string length plus one; a length of zero
indicates that no policy object is specified for the principal.  The
field "version_number" contains 0x12345C01.  The aux_attributes field
contains the bit 0x800 if a policy object is associated with the
principal.

Within a key_data record, numfields is 2 if the key data has
non-normal salt type, 1 otherwise.

Active kvno and master key auxiliary data
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

These types only appear in the entry of the master key principal
(K/M).  They use little-endian binary integer encoding.

The active kvno table determines which master key version is active
for a given timestamp.  It uses the following binary format:

.. code-block:: bnf

    active-key-version-table ::=
        version (16 bits) [with the value 1]
        version entry 1 (key-version-entry)
        version entry 2 (key-version-entry)
        ...

    key-version-entry ::=
        key version (16 bits)
        timestamp (32 bits) [when this key version becomes active]

The master key auxiliary data record contains copies of the current
master key encrypted in each older master key.  It uses the following
binary format:

.. code-block:: bnf

    master-key-aux ::=
        version (16 bits) [with the value 1]
        key entry 1 (key-entry)
        key entry 2 (key-entry)
        ...

    key-entry ::=
        old master key version (16 bits)
        latest master key version (16 bits)
        latest master key encryption type (16 bits)
        encrypted key length (16 bits)
        encrypted key contents

LDAP object information
~~~~~~~~~~~~~~~~~~~~~~~

This type appears in principal entries retrieved with the LDAP KDB
module.  The value uses the following binary format, using big-endian
integer encoding:

.. code-block:: bnf

    ldap-principal-data ::=
        record 1 (ldap-tl-data)
        record 2 (ldap-tl-data)
        ...

    ldap-tl-data ::=
        type (8 bits)
        length (16 bits)
        data

The currently defined ldap-tl-data types are (all integers are
big-endian):

* (1) principal type: 16 bits containing the value 1, indicating that
  the LDAP object containing the principal entry is a standalone
  principal object
* (2) principal count: 16 bits containing the number of
  krbPrincipalName values in the LDAP object
* (3) user DN: the string representation of the distinguished name of
  the LDAP object
* (5) attribute mask: 16 bits indicating which Kerberos-specific LDAP
  attributes are present in the LDAP object (see below)
* (7) link DN: the string representation of the distinguished name of
  an LDAP object this object is linked to; may appear multiple times

When converted to binary, the attribute mask bits, from least
significant to most significant, correspond to the following LDAP
attributes:

* krbMaxTicketLife
* krbMaxRenewableAge
* krbTicketFlags
* krbPrincipalExpiration
* krbTicketPolicyReference
* krbPrincipalAuthInd
* krbPwdPolicyReference
* krbPasswordExpiration
* krbPrincipalKey
* krbLastPwdChange
* krbExtraData
* krbLastSuccessfulAuth
* krbLastFailedAuth
* krbLoginFailedCount
* krbLastAdminUnlock
* krbPwdHistory


Alias principal entries
-----------------------

To allow aliases to be represented in dump files and within the
incremental update protocol, the krb5 database library supports the
concept of an alias principal entry.  An alias principal entry
contains an alias target principal in its tag-length data, has its
attributes set to disallow_all_tix, and has zero or empty values for
all other fields.  The database glue library recognizes alias entries
and iteratively looks up the alias target up to a depth of 10 chained
aliases.  (Added in release 1.22.)


DB2 principal and policy formats
--------------------------------

The DB2 KDB module uses the string form of a principal name, with zero
terminator, as a lookup key for principal entries.  Principal entry
values use the following binary format with little-endian integer
encoding:

.. code-block:: bnf

    db2-principal-entry ::=
        len (16 bits) [always has the value 38]
        attributes (32 bits)
        max ticket lifetime (32 bits)
        max renewable lifetime (32 bits)
        principal expiration timestamp (32 bits)
        password expiration timestamp (32 bits)
        last successful authentication timestamp (32 bits)
        last failed authentication timestamp (32 bits)
        failed authentication counter (32 bits)
        number of tag-length elements (16 bits)
        number of key-data elements (16 bits)
        length of string-form principal with zero terminator (16 bits)
        string-form principal with zero terminator
        tag-length entry 1 (tag-length-data)
        tag-length entry 2 (tag-length-data)
        ...
        key-data entry 1 (key-data)
        key-data entry 2 (key-data)
        ...

    tag-length-data ::=
        type tag (16 bits)
        data length (16 bits)
        data

    key-data ::=
        salt indicator (16 bits) [1 for default salt, 2 otherwise]
        key version (16 bits)
        encryption type (16 bits)
        encrypted key length (16 bits)
        encrypted key
        salt type (16 bits) [omitted if salt indicator is 1]
        salt data length (16 bits) [omitted if salt indicator is 1]
        salt data [omitted if salt indicator is 1]

DB2 policy entries reside in a separate database file.  The lookup key
is the policy name with zero terminator.  Policy entry values use a
modified XDR encoding of the policy type defined as follows:

.. code-block:: c

    struct tl_data {
        int type;
        opaque data<>;
        tl_data *next;
    };

    struct policy {
        int version_number;
        unsigned int min_life;
        unsigned int max_pw_life;
        unsigned int min_length;
        unsigned int min_classes;
        unsigned int history_num;
        unsigned int refcount;
        unsigned int max_fail;
        unsigned int failcount_interval;
        unsigned int lockout_duration;
        unsigned int attributes;
        unsigned int max_ticket_life;
        unsigned int max_renewable_life;
        nullstring allowed_keysalts;
        int n_tl_data;
        tl_data *tag_length_data;
    };

The type "nullstring" uses the same custom encoder as in the
per-principal kadmin data.

The field "version_number" contains 0x12345D01, 0x12345D02, or
0x12345D03 for versions 1, 2, and 3 respectively.  Versions 1 and 2
omit the fields "attributes" through "tag_length_data".  Version 1
also omits the fields "max_fail" through "lockout_duration".  Encoding
uses the lowest version that can represent the policy entry.

The field "refcount" is no longer used and its value is ignored.


LMDB principal and policy formats
---------------------------------

In the LMDB KDB module, principal entries are stored in the
"principal" database within the main LMDB environment (typically named
"principal.mdb"), with the exception of lockout-related fields which
are stored in the "lockout" table of the lockout LMDB environment
(typically named "principal.lockout.mdb").  For both databases the key
is the principal name in string form, with no zero terminator.  Values
in the "principal" database use the following binary format with
little-endian integer encoding:

.. code-block:: bnf

    lmdb-principal-entry ::=
        attributes (32 bits)
        max ticket lifetime (32 bits)
        max renewable lifetime (32 bits)
        principal expiration timestamp (32 bits)
        password expiration timestamp (32 bits)
        number of tag-length elements (16 bits)
        number of key-data elements (16 bits)
        tag-length entry 1 (tag-length-data)
        tag-length entry 2 (tag-length-data)
        ...
        key-data entry 1 (key-data)
        key-data entry 2 (key-data)
        ...

    tag-length-data ::=
        type tag (16 bits)
        data length (16 bits)
        data value

    key-data ::=
        salt indicator (16 bits) [1 for default salt, 2 otherwise]
        key version (16 bits)
        encryption type (16 bits)
        encrypted key length (16 bits)
        encrypted key
        salt type (16 bits) [omitted if salt indicator is 1]
        salt data length (16 bits) [omitted if salt indicator is 1]
        salt data [omitted if salt indicator is 1]

Values in the "lockout" database have the following binary format with
little-endian integer encoding:

.. code-block:: bnf

    lmdb-lockout-entry ::=
        last successful authentication timestamp (32 bits)
        last failed authentication timestamp (32 bits)
        failed authentication counter (32 bits)

In the "policy" database, the lookup key is the policy name with no
zero terminator.  Values in this database use the following binary
format with little-endian integer encoding:

.. code-block:: bnf

    lmdb-policy-entry ::=
        minimum password lifetime (32 bits)
        maximum password lifetime (32 bits)
        minimum password length (32 bits)
        minimum character classes (32 bits)
        number of historical keys (32 bits)
        maximum failed authentications before lockout (32 bits)
        time interval to reset failed authentication counter (32 bits)
        lockout duration (32 bits)
        required principal attributes (32 bits) [currently unenforced]
        maximum ticket lifetime (32 bits) [currently unenforced]
        maximum renewable lifetime (32 bits) [currently unenforced]
        allowed key/salt type specification length [32 bits]
        allowed key/salt type specification
        number of tag-length values (16 bits)
        tag-length entry 1 (tag-length-data)
        tag-length entry 2 (tag-length-data)
        ...

    tag-length-data ::=
        type tag (16 bits)
        data length (16 bits)
        data value
