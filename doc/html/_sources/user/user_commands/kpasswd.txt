.. _kpasswd(1):

kpasswd
=======

SYNOPSIS
--------

**kpasswd** [*principal*]


DESCRIPTION
-----------

The kpasswd command is used to change a Kerberos principal's password.
kpasswd first prompts for the current Kerberos password, then prompts
the user twice for the new password, and the password is changed.

If the principal is governed by a policy that specifies the length
and/or number of character classes required in the new password, the
new password must conform to the policy.  (The five character classes
are lower case, upper case, numbers, punctuation, and all other
characters.)


OPTIONS
-------

*principal*
    Change the password for the Kerberos principal principal.
    Otherwise, kpasswd uses the principal name from an existing ccache
    if there is one; if not, the principal is derived from the
    identity of the user invoking the kpasswd command.


SEE ALSO
--------

:ref:`kadmin(1)`, :ref:`kadmind(8)`
