.. _k5srvutil(1):

k5srvutil
=========

SYNOPSIS
--------

**k5srvutil** *operation*
[**-i**]
[**-f** *filename*]
[**-e** *keysalts*]

DESCRIPTION
-----------

k5srvutil allows an administrator to list keys currently in
a keytab, to obtain new keys for a principal currently in a keytab,
or to delete non-current keys from a keytab.

*operation* must be one of the following:

**list**
    Lists the keys in a keytab, showing version number and principal
    name.

**change**
    Uses the kadmin protocol to update the keys in the Kerberos
    database to new randomly-generated keys, and updates the keys in
    the keytab to match.  If a key's version number doesn't match the
    version number stored in the Kerberos server's database, then the
    operation will fail.  If the **-i** flag is given, k5srvutil will
    prompt for confirmation before changing each key.  If the **-k**
    option is given, the old and new keys will be displayed.
    Ordinarily, keys will be generated with the default encryption
    types and key salts.  This can be overridden with the **-e**
    option.  Old keys are retained in the keytab so that existing
    tickets continue to work, but **delold** should be used after
    such tickets expire, to prevent attacks against the old keys.

**delold**
    Deletes keys that are not the most recent version from the keytab.
    This operation should be used some time after a change operation
    to remove old keys, after existing tickets issued for the service
    have expired.  If the **-i** flag is given, then k5srvutil will
    prompt for confirmation for each principal.

**delete**
    Deletes particular keys in the keytab, interactively prompting for
    each key.

In all cases, the default keytab is used unless this is overridden by
the **-f** option.

k5srvutil uses the :ref:`kadmin(1)` program to edit the keytab in
place.


SEE ALSO
--------

:ref:`kadmin(1)`, :ref:`ktutil(1)`
