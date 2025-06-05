.. _ktutil(1):

ktutil
======

SYNOPSIS
--------

**ktutil**


DESCRIPTION
-----------

The ktutil command invokes a command interface from which an
administrator can read, write, or edit entries in a keytab.  (Kerberos
V4 srvtab files are no longer supported.)


COMMANDS
--------

list
~~~~

    **list** [**-t**] [**-k**] [**-e**]

Displays the current keylist.  If **-t**, **-k**, and/or **-e** are
specified, also display the timestamp, key contents, or enctype
(respectively).

Alias: **l**

read_kt
~~~~~~~

    **read_kt** *keytab*

Read the Kerberos V5 keytab file *keytab* into the current keylist.

Alias: **rkt**

write_kt
~~~~~~~~

    **write_kt** *keytab*

Write the current keylist into the Kerberos V5 keytab file *keytab*.

Alias: **wkt**

clear_list
~~~~~~~~~~

       **clear_list**

Clear the current keylist.

Alias: **clear**

delete_entry
~~~~~~~~~~~~

    **delete_entry** *slot*

Delete the entry in slot number *slot* from the current keylist.

Alias: **delent**

add_entry
~~~~~~~~~

    **add_entry** {**-key**\|\ **-password**} **-p** *principal*
    **-k** *kvno* [**-e** *enctype*] [**-f**\|\ **-s** *salt*]

Add *principal* to keylist using key or password.  If the **-f** flag
is specified, salt information will be fetched from the KDC; in this
case the **-e** flag may be omitted, or it may be supplied to force a
particular enctype.  If the **-f** flag is not specified, the **-e**
flag must be specified, and the default salt will be used unless
overridden with the **-s** option.

Alias: **addent**

list_requests
~~~~~~~~~~~~~

    **list_requests**

Displays a listing of available commands.

Aliases: **lr**, **?**

quit
~~~~

    **quit**

Quits ktutil.

Aliases: **exit**, **q**


EXAMPLE
-------

 ::

    ktutil:  add_entry -password -p alice@BLEEP.COM -k 1 -e
        aes128-cts-hmac-sha1-96
    Password for alice@BLEEP.COM:
    ktutil:  add_entry -password -p alice@BLEEP.COM -k 1 -e
        aes256-cts-hmac-sha1-96
    Password for alice@BLEEP.COM:
    ktutil:  write_kt alice.keytab
    ktutil:


ENVIRONMENT
-----------

See :ref:`kerberos(7)` for a description of Kerberos environment
variables.


SEE ALSO
--------

:ref:`kadmin(1)`, :ref:`kdb5_util(8)`, :ref:`kerberos(7)`
