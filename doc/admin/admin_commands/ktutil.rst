.. _ktutil(1):

ktutil
======

SYNOPSIS
--------

**ktutil**


DESCRIPTION
-----------

The ktutil command invokes a command interface from which an
administrator can read, write, or edit entries in a keytab or Kerberos
V4 srvtab file.


COMMANDS
--------

list
~~~~

    **list**

Displays the current keylist.

Alias: **l**

read_kt
~~~~~~~

    **read_kt** *keytab*

Read the Kerberos V5 keytab file *keytab* into the current keylist.

Alias: **rkt**

read_st
~~~~~~~

    **read_st** *srvtab*

Read the Kerberos V4 srvtab file *srvtab* into the current keylist.

Alias: **rst**

write_kt
~~~~~~~~

    **write_kt** *keytab*

Write the current keylist into the Kerberos V5 keytab file *keytab*.

Alias: **wkt**

write_st
~~~~~~~~

    **write_st** *srvtab*

Write the current keylist into the Kerberos V4 srvtab file *srvtab*.

Alias: **wst**

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
    **-k** *kvno* **-e** *enctype*

Add *principal* to keylist using key or password.

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
    ktutil:  write_kt keytab
    ktutil:


SEE ALSO
--------

:ref:`kadmin(1)`, :ref:`kdb5_util(8)`
