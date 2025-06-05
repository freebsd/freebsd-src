.. _ksu(1):

ksu
===

SYNOPSIS
--------

**ksu**
[ *target_user* ]
[ **-n** *target_principal_name* ]
[ **-c** *source_cache_name* ]
[ **-k** ]
[ **-r** time ]
[ **-p** | **-P**]
[ **-f** | **-F**]
[ **-l** *lifetime* ]
[ **-z | Z** ]
[ **-q** ]
[ **-e** *command* [ args ...  ] ] [ **-a** [ args ...  ] ]


REQUIREMENTS
------------

Must have Kerberos version 5 installed to compile ksu.  Must have a
Kerberos version 5 server running to use ksu.


DESCRIPTION
-----------

ksu is a Kerberized version of the su program that has two missions:
one is to securely change the real and effective user ID to that of
the target user, and the other is to create a new security context.

.. note::

          For the sake of clarity, all references to and attributes of
          the user invoking the program will start with "source"
          (e.g., "source user", "source cache", etc.).

          Likewise, all references to and attributes of the target
          account will start with "target".

AUTHENTICATION
--------------

To fulfill the first mission, ksu operates in two phases:
authentication and authorization.  Resolving the target principal name
is the first step in authentication.  The user can either specify his
principal name with the **-n** option (e.g., ``-n jqpublic@USC.EDU``)
or a default principal name will be assigned using a heuristic
described in the OPTIONS section (see **-n** option).  The target user
name must be the first argument to ksu; if not specified root is the
default.  If ``.`` is specified then the target user will be the
source user (e.g., ``ksu .``).  If the source user is root or the
target user is the source user, no authentication or authorization
takes place.  Otherwise, ksu looks for an appropriate Kerberos ticket
in the source cache.

The ticket can either be for the end-server or a ticket granting
ticket (TGT) for the target principal's realm.  If the ticket for the
end-server is already in the cache, it's decrypted and verified.  If
it's not in the cache but the TGT is, the TGT is used to obtain the
ticket for the end-server.  The end-server ticket is then verified.
If neither ticket is in the cache, but ksu is compiled with the
**GET_TGT_VIA_PASSWD** define, the user will be prompted for a
Kerberos password which will then be used to get a TGT.  If the user
is logged in remotely and does not have a secure channel, the password
may be exposed.  If neither ticket is in the cache and
**GET_TGT_VIA_PASSWD** is not defined, authentication fails.


AUTHORIZATION
-------------

This section describes authorization of the source user when ksu is
invoked without the **-e** option.  For a description of the **-e**
option, see the OPTIONS section.

Upon successful authentication, ksu checks whether the target
principal is authorized to access the target account.  In the target
user's home directory, ksu attempts to access two authorization files:
:ref:`.k5login(5)` and .k5users.  In the .k5login file each line
contains the name of a principal that is authorized to access the
account.

For example::

    jqpublic@USC.EDU
    jqpublic/secure@USC.EDU
    jqpublic/admin@USC.EDU

The format of .k5users is the same, except the principal name may be
followed by a list of commands that the principal is authorized to
execute (see the **-e** option in the OPTIONS section for details).

Thus if the target principal name is found in the .k5login file the
source user is authorized to access the target account.  Otherwise ksu
looks in the .k5users file.  If the target principal name is found
without any trailing commands or followed only by ``*`` then the
source user is authorized.  If either .k5login or .k5users exist but
an appropriate entry for the target principal does not exist then
access is denied.  If neither file exists then the principal will be
granted access to the account according to the aname->lname mapping
rules.  Otherwise, authorization fails.


EXECUTION OF THE TARGET SHELL
-----------------------------

Upon successful authentication and authorization, ksu proceeds in a
similar fashion to su.  The environment is unmodified with the
exception of USER, HOME and SHELL variables.  If the target user is
not root, USER gets set to the target user name.  Otherwise USER
remains unchanged.  Both HOME and SHELL are set to the target login's
default values.  In addition, the environment variable **KRB5CCNAME**
gets set to the name of the target cache.  The real and effective user
ID are changed to that of the target user.  The target user's shell is
then invoked (the shell name is specified in the password file).  Upon
termination of the shell, ksu deletes the target cache (unless ksu is
invoked with the **-k** option).  This is implemented by first doing a
fork and then an exec, instead of just exec, as done by su.


CREATING A NEW SECURITY CONTEXT
-------------------------------

ksu can be used to create a new security context for the target
program (either the target shell, or command specified via the **-e**
option).  The target program inherits a set of credentials from the
source user.  By default, this set includes all of the credentials in
the source cache plus any additional credentials obtained during
authentication.  The source user is able to limit the credentials in
this set by using **-z** or **-Z** option.  **-z** restricts the copy
of tickets from the source cache to the target cache to only the
tickets where client == the target principal name.  The **-Z** option
provides the target user with a fresh target cache (no creds in the
cache).  Note that for security reasons, when the source user is root
and target user is non-root, **-z** option is the default mode of
operation.

While no authentication takes place if the source user is root or is
the same as the target user, additional tickets can still be obtained
for the target cache.  If **-n** is specified and no credentials can
be copied to the target cache, the source user is prompted for a
Kerberos password (unless **-Z** specified or **GET_TGT_VIA_PASSWD**
is undefined).  If successful, a TGT is obtained from the Kerberos
server and stored in the target cache.  Otherwise, if a password is
not provided (user hit return) ksu continues in a normal mode of
operation (the target cache will not contain the desired TGT).  If the
wrong password is typed in, ksu fails.

.. note::

          During authentication, only the tickets that could be
          obtained without providing a password are cached in the
          source cache.


OPTIONS
-------

**-n** *target_principal_name*
    Specify a Kerberos target principal name.  Used in authentication
    and authorization phases of ksu.

    If ksu is invoked without **-n**, a default principal name is
    assigned via the following heuristic:

    * Case 1: source user is non-root.

      If the target user is the source user the default principal name
      is set to the default principal of the source cache.  If the
      cache does not exist then the default principal name is set to
      ``target_user@local_realm``.  If the source and target users are
      different and neither ``~target_user/.k5users`` nor
      ``~target_user/.k5login`` exist then the default principal name
      is ``target_user_login_name@local_realm``.  Otherwise, starting
      with the first principal listed below, ksu checks if the
      principal is authorized to access the target account and whether
      there is a legitimate ticket for that principal in the source
      cache.  If both conditions are met that principal becomes the
      default target principal, otherwise go to the next principal.

      a) default principal of the source cache
      b) target_user\@local_realm
      c) source_user\@local_realm

      If a-c fails try any principal for which there is a ticket in
      the source cache and that is authorized to access the target
      account.  If that fails select the first principal that is
      authorized to access the target account from the above list.  If
      none are authorized and ksu is configured with
      **PRINC_LOOK_AHEAD** turned on, select the default principal as
      follows:

      For each candidate in the above list, select an authorized
      principal that has the same realm name and first part of the
      principal name equal to the prefix of the candidate.  For
      example if candidate a) is ``jqpublic@ISI.EDU`` and
      ``jqpublic/secure@ISI.EDU`` is authorized to access the target
      account then the default principal is set to
      ``jqpublic/secure@ISI.EDU``.

    * Case 2: source user is root.

      If the target user is non-root then the default principal name
      is ``target_user@local_realm``.  Else, if the source cache
      exists the default principal name is set to the default
      principal of the source cache.  If the source cache does not
      exist, default principal name is set to ``root\@local_realm``.

**-c** *source_cache_name*

    Specify source cache name (e.g., ``-c FILE:/tmp/my_cache``).  If
    **-c** option is not used then the name is obtained from
    **KRB5CCNAME** environment variable.  If **KRB5CCNAME** is not
    defined the source cache name is set to ``krb5cc_<source uid>``.
    The target cache name is automatically set to ``krb5cc_<target
    uid>.(gen_sym())``, where gen_sym generates a new number such that
    the resulting cache does not already exist.  For example::

        krb5cc_1984.2

**-k**
    Do not delete the target cache upon termination of the target
    shell or a command (**-e** command).  Without **-k**, ksu deletes
    the target cache.

**-z**
    Restrict the copy of tickets from the source cache to the target
    cache to only the tickets where client == the target principal
    name.  Use the **-n** option if you want the tickets for other then
    the default principal.  Note that the **-z** option is mutually
    exclusive with the **-Z** option.

**-Z**
    Don't copy any tickets from the source cache to the target cache.
    Just create a fresh target cache, where the default principal name
    of the cache is initialized to the target principal name.  Note
    that the **-Z** option is mutually exclusive with the **-z**
    option.

**-q**
    Suppress the printing of status messages.

Ticket granting ticket options:

**-l** *lifetime* **-r** *time* **-p** **-P** **-f** **-F**
    The ticket granting ticket options only apply to the case where
    there are no appropriate tickets in the cache to authenticate the
    source user.  In this case if ksu is configured to prompt users
    for a Kerberos password (**GET_TGT_VIA_PASSWD** is defined), the
    ticket granting ticket options that are specified will be used
    when getting a ticket granting ticket from the Kerberos server.

**-l** *lifetime*
    (:ref:`duration` string.)  Specifies the lifetime to be requested
    for the ticket; if this option is not specified, the default ticket
    lifetime (12 hours) is used instead.

**-r** *time*
    (:ref:`duration` string.)  Specifies that the **renewable** option
    should be requested for the ticket, and specifies the desired
    total lifetime of the ticket.

**-p**
    specifies that the **proxiable** option should be requested for
    the ticket.

**-P**
    specifies that the **proxiable** option should not be requested
    for the ticket, even if the default configuration is to ask for
    proxiable tickets.

**-f**
    option specifies that the **forwardable** option should be
    requested for the ticket.

**-F**
    option specifies that the **forwardable** option should not be
    requested for the ticket, even if the default configuration is to
    ask for forwardable tickets.

**-e** *command* [*args* ...]
    ksu proceeds exactly the same as if it was invoked without the
    **-e** option, except instead of executing the target shell, ksu
    executes the specified command. Example of usage::

        ksu bob -e ls -lag

    The authorization algorithm for **-e** is as follows:

    If the source user is root or source user == target user, no
    authorization takes place and the command is executed.  If source
    user id != 0, and ``~target_user/.k5users`` file does not exist,
    authorization fails.  Otherwise, ``~target_user/.k5users`` file
    must have an appropriate entry for target principal to get
    authorized.

    The .k5users file format:

    A single principal entry on each line that may be followed by a
    list of commands that the principal is authorized to execute.  A
    principal name followed by a ``*`` means that the user is
    authorized to execute any command.  Thus, in the following
    example::

        jqpublic@USC.EDU ls mail /local/kerberos/klist
        jqpublic/secure@USC.EDU *
        jqpublic/admin@USC.EDU

    ``jqpublic@USC.EDU`` is only authorized to execute ``ls``,
    ``mail`` and ``klist`` commands.  ``jqpublic/secure@USC.EDU`` is
    authorized to execute any command.  ``jqpublic/admin@USC.EDU`` is
    not authorized to execute any command.  Note, that
    ``jqpublic/admin@USC.EDU`` is authorized to execute the target
    shell (regular ksu, without the **-e** option) but
    ``jqpublic@USC.EDU`` is not.

    The commands listed after the principal name must be either a full
    path names or just the program name.  In the second case,
    **CMD_PATH** specifying the location of authorized programs must
    be defined at the compilation time of ksu.  Which command gets
    executed?

    If the source user is root or the target user is the source user
    or the user is authorized to execute any command (``*`` entry)
    then command can be either a full or a relative path leading to
    the target program.  Otherwise, the user must specify either a
    full path or just the program name.

**-a** *args*
    Specify arguments to be passed to the target shell.  Note that all
    flags and parameters following -a will be passed to the shell,
    thus all options intended for ksu must precede **-a**.

    The **-a** option can be used to simulate the **-e** option if
    used as follows::

        -a -c [command [arguments]].

    **-c** is interpreted by the c-shell to execute the command.


INSTALLATION INSTRUCTIONS
-------------------------

ksu can be compiled with the following four flags:

**GET_TGT_VIA_PASSWD**
    In case no appropriate tickets are found in the source cache, the
    user will be prompted for a Kerberos password.  The password is
    then used to get a ticket granting ticket from the Kerberos
    server.  The danger of configuring ksu with this macro is if the
    source user is logged in remotely and does not have a secure
    channel, the password may get exposed.

**PRINC_LOOK_AHEAD**
    During the resolution of the default principal name,
    **PRINC_LOOK_AHEAD** enables ksu to find principal names in
    the .k5users file as described in the OPTIONS section
    (see **-n** option).

**CMD_PATH**
    Specifies a list of directories containing programs that users are
    authorized to execute (via .k5users file).

**HAVE_GETUSERSHELL**
    If the source user is non-root, ksu insists that the target user's
    shell to be invoked is a "legal shell".  *getusershell(3)* is
    called to obtain the names of "legal shells".  Note that the
    target user's shell is obtained from the passwd file.

Sample configuration::

    KSU_OPTS = -DGET_TGT_VIA_PASSWD -DPRINC_LOOK_AHEAD -DCMD_PATH='"/bin /usr/ucb /local/bin"

ksu should be owned by root and have the set user id bit turned on.

ksu attempts to get a ticket for the end server just as Kerberized
telnet and rlogin.  Thus, there must be an entry for the server in the
Kerberos database (e.g., ``host/nii.isi.edu@ISI.EDU``).  The keytab
file must be in an appropriate location.


SIDE EFFECTS
------------

ksu deletes all expired tickets from the source cache.


AUTHOR OF KSU
-------------

GENNADY (ARI) MEDVINSKY


ENVIRONMENT
-----------

See :ref:`kerberos(7)` for a description of Kerberos environment
variables.


SEE ALSO
--------

:ref:`kerberos(7)`, :ref:`kinit(1)`
