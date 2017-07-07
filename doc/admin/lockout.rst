Account lockout
===============

As of release 1.8, the KDC can be configured to lock out principals
after a number of failed authentication attempts within a period of
time.  Account lockout can make it more difficult to attack a
principal's password by brute force, but also makes it easy for an
attacker to deny access to a principal.


Configuring account lockout
---------------------------

Account lockout only works for principals with the
**+requires_preauth** flag set.  Without this flag, the KDC cannot
know whether or not a client successfully decrypted the ticket it
issued.  It is also important to set the **-allow_svr** flag on a
principal to protect its password from an off-line dictionary attack
through a TGS request.  You can set these flags on a principal with
:ref:`kadmin(1)` as follows::

    kadmin: modprinc +requires_preauth -allow_svr PRINCNAME

Account lockout parameters are configured via :ref:`policy objects
<policies>`.  There may be an existing policy associated with user
principals (such as the "default" policy), or you may need to create a
new one and associate it with each user principal.

The policy parameters related to account lockout are:

* :ref:`maxfailure <policy_maxfailure>`: the number of failed attempts
  before the principal is locked out
* :ref:`failurecountinterval <policy_failurecountinterval>`: the
  allowable interval between failed attempts
* :ref:`lockoutduration <policy_lockoutduration>`: the amount of time
  a principal is locked out for

Here is an example of setting these parameters on a new policy and
associating it with a principal::

    kadmin: addpol -maxfailure 10 -failurecountinterval 180
        -lockoutduration 60 lockout_policy
    kadmin: modprinc -policy lockout_policy PRINCNAME


Testing account lockout
-----------------------

To test that account lockout is working, try authenticating as the
principal (hopefully not one that might be in use) multiple times with
the wrong password.  For instance, if **maxfailure** is set to 2, you
might see::

    $ kinit user
    Password for user@KRBTEST.COM:
    kinit: Password incorrect while getting initial credentials
    $ kinit user
    Password for user@KRBTEST.COM:
    kinit: Password incorrect while getting initial credentials
    $ kinit user
    kinit: Client's credentials have been revoked while getting initial credentials


Account lockout principal state
-------------------------------

A principal entry keeps three pieces of state related to account
lockout:

* The time of last successful authentication
* The time of last failed authentication
* A counter of failed attempts

The time of last successful authentication is not actually needed for
the account lockout system to function, but may be of administrative
interest.  These fields can be observed with the **getprinc** kadmin
command.  For example::

    kadmin: getprinc user
    Principal: user@KRBTEST.COM
    ...
    Last successful authentication: [never]
    Last failed authentication: Mon Dec 03 12:30:33 EST 2012
    Failed password attempts: 2
    ...

A principal which has been locked out can be administratively unlocked
with the **-unlock** option to the **modprinc** kadmin command::

    kadmin: modprinc -unlock PRINCNAME

This command will reset the number of failed attempts to 0.


KDC replication and account lockout
-----------------------------------

The account lockout state of a principal is not replicated by either
traditional :ref:`kprop(8)` or incremental propagation.  Because of
this, the number of attempts an attacker can make within a time period
is multiplied by the number of KDCs.  For instance, if the
**maxfailure** parameter on a policy is 10 and there are four KDCs in
the environment (a master and three slaves), an attacker could make as
many as 40 attempts before the principal is locked out on all four
KDCs.

An administrative unlock is propagated from the master to the slave
KDCs during the next propagation.  Propagation of an administrative
unlock will cause the counter of failed attempts on each slave to
reset to 1 on the next failure.

If a KDC environment uses a replication strategy other than kprop or
incremental propagation, such as the LDAP KDB module with multi-master
LDAP replication, then account lockout state may be replicated between
KDCs and the concerns of this section may not apply.


KDC performance and account lockout
-----------------------------------

In order to fully track account lockout state, the KDC must write to
the the database on each successful and failed authentication.
Writing to the database is generally more expensive than reading from
it, so these writes may have a significant impact on KDC performance.
As of release 1.9, it is possible to turn off account lockout state
tracking in order to improve performance, by setting the
**disable_last_success** and **disable_lockout** variables in the
database module subsection of :ref:`kdc.conf(5)`.  For example::

    [dbmodules]
        DB = {
            disable_last_success = true
            disable_lockout = true
        }

Of the two variables, setting **disable_last_success** will usually
have the largest positive impact on performance, and will still allow
account lockout policies to operate.  However, it will make it
impossible to observe the last successful authentication time with
kadmin.


KDC setup and account lockout
-----------------------------

To update the account lockout state on principals, the KDC must be
able to write to the principal database.  For the DB2 module, no
special setup is required.  For the LDAP module, the KDC DN must be
granted write access to the principal objects.  If the KDC DN has only
read access, account lockout will not function.
