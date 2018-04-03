.. _kadm5.acl(5):

kadm5.acl
=========

DESCRIPTION
-----------

The Kerberos :ref:`kadmind(8)` daemon uses an Access Control List
(ACL) file to manage access rights to the Kerberos database.
For operations that affect principals, the ACL file also controls
which principals can operate on which other principals.

The default location of the Kerberos ACL file is
|kdcdir|\ ``/kadm5.acl``  unless this is overridden by the *acl_file*
variable in :ref:`kdc.conf(5)`.

SYNTAX
------

Empty lines and lines starting with the sharp sign (``#``) are
ignored.  Lines containing ACL entries have the format::

    principal  permissions  [target_principal  [restrictions] ]

.. note::

          Line order in the ACL file is important.  The first matching entry
          will control access for an actor principal on a target principal.

*principal*
    (Partially or fully qualified Kerberos principal name.) Specifies
    the principal whose permissions are to be set.

    Each component of the name may be wildcarded using the ``*``
    character.

*permissions*
    Specifies what operations may or may not be performed by a
    *principal* matching a particular entry.  This is a string of one or
    more of the following list of characters or their upper-case
    counterparts.  If the character is *upper-case*, then the operation
    is disallowed.  If the character is *lower-case*, then the operation
    is permitted.

    == ======================================================
    a  [Dis]allows the addition of principals or policies
    c  [Dis]allows the changing of passwords for principals
    d  [Dis]allows the deletion of principals or policies
    e  [Dis]allows the extraction of principal keys
    i  [Dis]allows inquiries about principals or policies
    l  [Dis]allows the listing of all principals or policies
    m  [Dis]allows the modification of principals or policies
    p  [Dis]allows the propagation of the principal database (used in :ref:`incr_db_prop`)
    s  [Dis]allows the explicit setting of the key for a principal
    x  Short for admcilsp. All privileges (except ``e``)
    \* Same as x.
    == ======================================================

.. note::

        The ``extract`` privilege is not included in the wildcard
        privilege; it must be explicitly assigned.  This privilege
        allows the user to extract keys from the database, and must be
        handled with great care to avoid disclosure of important keys
        like those of the kadmin/* or krbtgt/* principals.  The
        **lockdown_keys** principal attribute can be used to prevent
        key extraction from specific principals regardless of the
        granted privilege.

*target_principal*
    (Optional. Partially or fully qualified Kerberos principal name.)
    Specifies the principal on which *permissions* may be applied.
    Each component of the name may be wildcarded using the ``*``
    character.

    *target_principal* can also include back-references to *principal*,
    in which ``*number`` matches the corresponding wildcard in
    *principal*.

*restrictions*
    (Optional) A string of flags. Allowed restrictions are:

        {+\|-}\ *flagname*
            flag is forced to the indicated value.  The permissible flags
            are the same as those for the **default_principal_flags**
            variable in :ref:`kdc.conf(5)`.

        *-clearpolicy*
            policy is forced to be empty.

        *-policy pol*
            policy is forced to be *pol*.

        -{*expire, pwexpire, maxlife, maxrenewlife*} *time*
            (:ref:`getdate` string) associated value will be forced to
            MIN(*time*, requested value).

    The above flags act as restrictions on any add or modify operation
    which is allowed due to that ACL line.

.. warning::

    If the kadmind ACL file is modified, the kadmind daemon needs to be
    restarted for changes to take effect.

EXAMPLE
-------

Here is an example of a kadm5.acl file::

    */admin@ATHENA.MIT.EDU    *                               # line 1
    joeadmin@ATHENA.MIT.EDU   ADMCIL                          # line 2
    joeadmin/*@ATHENA.MIT.EDU i   */root@ATHENA.MIT.EDU       # line 3
    */root@ATHENA.MIT.EDU     ci  *1@ATHENA.MIT.EDU           # line 4
    */root@ATHENA.MIT.EDU     l   *                           # line 5
    sms@ATHENA.MIT.EDU        x   * -maxlife 9h -postdateable # line 6

(line 1) Any principal in the ``ATHENA.MIT.EDU`` realm with an
``admin`` instance has all administrative privileges except extracting
keys.

(lines 1-3) The user ``joeadmin`` has all permissions except
extracting keys with his ``admin`` instance,
``joeadmin/admin@ATHENA.MIT.EDU`` (matches line 1).  He has no
permissions at all with his null instance, ``joeadmin@ATHENA.MIT.EDU``
(matches line 2).  His ``root`` and other non-``admin``, non-null
instances (e.g., ``extra`` or ``dbadmin``) have inquire permissions
with any principal that has the instance ``root`` (matches line 3).

(line 4) Any ``root`` principal in ``ATHENA.MIT.EDU`` can inquire
or change the password of their null instance, but not any other
null instance.  (Here, ``*1`` denotes a back-reference to the
component matching the first wildcard in the actor principal.)

(line 5) Any ``root`` principal in ``ATHENA.MIT.EDU`` can generate
the list of principals in the database, and the list of policies
in the database.  This line is separate from line 4, because list
permission can only be granted globally, not to specific target
principals.

(line 6) Finally, the Service Management System principal
``sms@ATHENA.MIT.EDU`` has all permissions except extracting keys, but
any principal that it creates or modifies will not be able to get
postdateable tickets or tickets with a life of longer than 9 hours.

MODULE BEHAVIOR
---------------

The ACL file can coexist with other authorization modules in release
1.16 and later, as configured in the :ref:`kadm5_auth` section of
:ref:`krb5.conf(5)`.  The ACL file will positively authorize
operations according to the rules above, but will never
authoritatively deny an operation, so other modules can authorize
operations in addition to those authorized by the ACL file.

To operate without an ACL file, set the *acl_file* variable in
:ref:`kdc.conf(5)` to the empty string with ``acl_file = ""``.

SEE ALSO
--------

:ref:`kdc.conf(5)`, :ref:`kadmind(8)`
