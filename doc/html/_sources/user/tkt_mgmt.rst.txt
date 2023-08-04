Ticket management
=================

On many systems, Kerberos is built into the login program, and you get
tickets automatically when you log in.  Other programs, such as ssh,
can forward copies of your tickets to a remote host.  Most of these
programs also automatically destroy your tickets when they exit.
However, MIT recommends that you explicitly destroy your Kerberos
tickets when you are through with them, just to be sure.  One way to
help ensure that this happens is to add the :ref:`kdestroy(1)` command
to your .logout file.  Additionally, if you are going to be away from
your machine and are concerned about an intruder using your
permissions, it is safest to either destroy all copies of your
tickets, or use a screensaver that locks the screen.


Kerberos ticket properties
--------------------------

There are various properties that Kerberos tickets can have:

If a ticket is **forwardable**, then the KDC can issue a new ticket
(with a different network address, if necessary) based on the
forwardable ticket.  This allows for authentication forwarding without
requiring a password to be typed in again.  For example, if a user
with a forwardable TGT logs into a remote system, the KDC could issue
a new TGT for that user with the network address of the remote system,
allowing authentication on that host to work as though the user were
logged in locally.

When the KDC creates a new ticket based on a forwardable ticket, it
sets the **forwarded** flag on that new ticket.  Any tickets that are
created based on a ticket with the forwarded flag set will also have
their forwarded flags set.

A **proxiable** ticket is similar to a forwardable ticket in that it
allows a service to take on the identity of the client.  Unlike a
forwardable ticket, however, a proxiable ticket is only issued for
specific services.  In other words, a ticket-granting ticket cannot be
issued based on a ticket that is proxiable but not forwardable.

A **proxy** ticket is one that was issued based on a proxiable ticket.

A **postdated** ticket is issued with the invalid flag set.  After the
starting time listed on the ticket, it can be presented to the KDC to
obtain valid tickets.

Ticket-granting tickets with the **postdateable** flag set can be used
to obtain postdated service tickets.

**Renewable** tickets can be used to obtain new session keys without
the user entering their password again.  A renewable ticket has two
expiration times.  The first is the time at which this particular
ticket expires.  The second is the latest possible expiration time for
any ticket issued based on this renewable ticket.

A ticket with the **initial flag** set was issued based on the
authentication protocol, and not on a ticket-granting ticket.
Application servers that wish to ensure that the user's key has been
recently presented for verification could specify that this flag must
be set to accept the ticket.

An **invalid** ticket must be rejected by application servers.
Postdated tickets are usually issued with this flag set, and must be
validated by the KDC before they can be used.

A **preauthenticated** ticket is one that was only issued after the
client requesting the ticket had authenticated itself to the KDC.

The **hardware authentication** flag is set on a ticket which required
the use of hardware for authentication.  The hardware is expected to
be possessed only by the client which requested the tickets.

If a ticket has the **transit policy** checked flag set, then the KDC
that issued this ticket implements the transited-realm check policy
and checked the transited-realms list on the ticket.  The
transited-realms list contains a list of all intermediate realms
between the realm of the KDC that issued the first ticket and that of
the one that issued the current ticket.  If this flag is not set, then
the application server must check the transited realms itself or else
reject the ticket.

The **okay as delegate** flag indicates that the server specified in
the ticket is suitable as a delegate as determined by the policy of
that realm.  Some client applications may use this flag to decide
whether to forward tickets to a remote host, although many
applications do not honor it.

An **anonymous** ticket is one in which the named principal is a
generic principal for that realm; it does not actually specify the
individual that will be using the ticket.  This ticket is meant only
to securely distribute a session key.


.. _obtain_tkt:

Obtaining tickets with kinit
----------------------------

If your site has integrated Kerberos V5 with the login system, you
will get Kerberos tickets automatically when you log in.  Otherwise,
you may need to explicitly obtain your Kerberos tickets, using the
:ref:`kinit(1)` program.  Similarly, if your Kerberos tickets expire,
use the kinit program to obtain new ones.

To use the kinit program, simply type ``kinit`` and then type your
password at the prompt. For example, Jennifer (whose username is
``jennifer``) works for Bleep, Inc. (a fictitious company with the
domain name mit.edu and the Kerberos realm ATHENA.MIT.EDU).  She would
type::

    shell% kinit
    Password for jennifer@ATHENA.MIT.EDU: <-- [Type jennifer's password here.]
    shell%

If you type your password incorrectly, kinit will give you the
following error message::

    shell% kinit
    Password for jennifer@ATHENA.MIT.EDU: <-- [Type the wrong password here.]
    kinit: Password incorrect
    shell%

and you won't get Kerberos tickets.

By default, kinit assumes you want tickets for your own username in
your default realm.  Suppose Jennifer's friend David is visiting, and
he wants to borrow a window to check his mail.  David needs to get
tickets for himself in his own realm, EXAMPLE.COM.  He would type::

    shell% kinit david@EXAMPLE.COM
    Password for david@EXAMPLE.COM: <-- [Type david's password here.]
    shell%

David would then have tickets which he could use to log onto his own
machine.  Note that he typed his password locally on Jennifer's
machine, but it never went over the network.  Kerberos on the local
host performed the authentication to the KDC in the other realm.

If you want to be able to forward your tickets to another host, you
need to request forwardable tickets.  You do this by specifying the
**-f** option::

    shell% kinit -f
    Password for jennifer@ATHENA.MIT.EDU: <-- [Type your password here.]
    shell%

Note that kinit does not tell you that it obtained forwardable
tickets; you can verify this using the :ref:`klist(1)` command (see
:ref:`view_tkt`).

Normally, your tickets are good for your system's default ticket
lifetime, which is ten hours on many systems.  You can specify a
different ticket lifetime with the **-l** option.  Add the letter
**s** to the value for seconds, **m** for minutes, **h** for hours, or
**d** for days.  For example, to obtain forwardable tickets for
``david@EXAMPLE.COM`` that would be good for three hours, you would
type::

    shell% kinit -f -l 3h david@EXAMPLE.COM
    Password for david@EXAMPLE.COM: <-- [Type david's password here.]
    shell%

.. note::

          You cannot mix units; specifying a lifetime of 3h30m would
          result in an error.  Note also that most systems specify a
          maximum ticket lifetime.  If you request a longer ticket
          lifetime, it will be automatically truncated to the maximum
          lifetime.


.. _view_tkt:

Viewing tickets with klist
--------------------------

The :ref:`klist(1)` command shows your tickets.  When you first obtain
tickets, you will have only the ticket-granting ticket.  The listing
would look like this::

    shell% klist
    Ticket cache: /tmp/krb5cc_ttypa
    Default principal: jennifer@ATHENA.MIT.EDU

    Valid starting     Expires            Service principal
    06/07/04 19:49:21  06/08/04 05:49:19  krbtgt/ATHENA.MIT.EDU@ATHENA.MIT.EDU
    shell%

The ticket cache is the location of your ticket file. In the above
example, this file is named ``/tmp/krb5cc_ttypa``. The default
principal is your Kerberos principal.

The "valid starting" and "expires" fields describe the period of time
during which the ticket is valid.  The "service principal" describes
each ticket.  The ticket-granting ticket has a first component
``krbtgt``, and a second component which is the realm name.

Now, if ``jennifer`` connected to the machine ``daffodil.mit.edu``,
and then typed "klist" again, she would have gotten the following
result::

    shell% klist
    Ticket cache: /tmp/krb5cc_ttypa
    Default principal: jennifer@ATHENA.MIT.EDU

    Valid starting     Expires            Service principal
    06/07/04 19:49:21  06/08/04 05:49:19  krbtgt/ATHENA.MIT.EDU@ATHENA.MIT.EDU
    06/07/04 20:22:30  06/08/04 05:49:19  host/daffodil.mit.edu@ATHENA.MIT.EDU
    shell%

Here's what happened: when ``jennifer`` used ssh to connect to the
host ``daffodil.mit.edu``, the ssh program presented her
ticket-granting ticket to the KDC and requested a host ticket for the
host ``daffodil.mit.edu``.  The KDC sent the host ticket, which ssh
then presented to the host ``daffodil.mit.edu``, and she was allowed
to log in without typing her password.

Suppose your Kerberos tickets allow you to log into a host in another
domain, such as ``trillium.example.com``, which is also in another
Kerberos realm, ``EXAMPLE.COM``.  If you ssh to this host, you will
receive a ticket-granting ticket for the realm ``EXAMPLE.COM``, plus
the new host ticket for ``trillium.example.com``.  klist will now
show::

    shell% klist
    Ticket cache: /tmp/krb5cc_ttypa
    Default principal: jennifer@ATHENA.MIT.EDU

    Valid starting     Expires            Service principal
    06/07/04 19:49:21  06/08/04 05:49:19  krbtgt/ATHENA.MIT.EDU@ATHENA.MIT.EDU
    06/07/04 20:22:30  06/08/04 05:49:19  host/daffodil.mit.edu@ATHENA.MIT.EDU
    06/07/04 20:24:18  06/08/04 05:49:19  krbtgt/EXAMPLE.COM@ATHENA.MIT.EDU
    06/07/04 20:24:18  06/08/04 05:49:19  host/trillium.example.com@EXAMPLE.COM
    shell%

Depending on your host's and realm's configuration, you may also see a
ticket with the service principal ``host/trillium.example.com@``.  If
so, this means that your host did not know what realm
trillium.example.com is in, so it asked the ``ATHENA.MIT.EDU`` KDC for
a referral.  The next time you connect to ``trillium.example.com``,
the odd-looking entry will be used to avoid needing to ask for a
referral again.

You can use the **-f** option to view the flags that apply to your
tickets.  The flags are:

===== =========================
  F   Forwardable
  f   forwarded
  P   Proxiable
  p   proxy
  D   postDateable
  d   postdated
  R   Renewable
  I   Initial
  i   invalid
  H   Hardware authenticated
  A   preAuthenticated
  T   Transit policy checked
  O   Okay as delegate
  a   anonymous
===== =========================

Here is a sample listing.  In this example, the user *jennifer*
obtained her initial tickets (**I**), which are forwardable (**F**)
and postdated (**d**) but not yet validated (**i**)::

    shell% klist -f
    Ticket cache: /tmp/krb5cc_320
    Default principal: jennifer@ATHENA.MIT.EDU

    Valid starting      Expires             Service principal
    31/07/05 19:06:25  31/07/05 19:16:25  krbtgt/ATHENA.MIT.EDU@ATHENA.MIT.EDU
            Flags: FdiI
    shell%

In the following example, the user *david*'s tickets were forwarded
(**f**) to this host from another host.  The tickets are reforwardable
(**F**)::

    shell% klist -f
    Ticket cache: /tmp/krb5cc_p11795
    Default principal: david@EXAMPLE.COM

    Valid starting     Expires            Service principal
    07/31/05 11:52:29  07/31/05 21:11:23  krbtgt/EXAMPLE.COM@EXAMPLE.COM
            Flags: Ff
    07/31/05 12:03:48  07/31/05 21:11:23  host/trillium.example.com@EXAMPLE.COM
            Flags: Ff
    shell%


Destroying tickets with kdestroy
--------------------------------

Your Kerberos tickets are proof that you are indeed yourself, and
tickets could be stolen if someone gains access to a computer where
they are stored.  If this happens, the person who has them can
masquerade as you until they expire.  For this reason, you should
destroy your Kerberos tickets when you are away from your computer.

Destroying your tickets is easy.  Simply type kdestroy::

    shell% kdestroy
    shell%

If :ref:`kdestroy(1)` fails to destroy your tickets, it will beep and
give an error message.  For example, if kdestroy can't find any
tickets to destroy, it will give the following message::

    shell% kdestroy
    kdestroy: No credentials cache file found while destroying cache
    shell%
