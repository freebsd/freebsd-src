.. _keytab_definition:

keytab
======

A keytab (short for "key table") stores long-term keys for one or more
principals.  Keytabs are normally represented by files in a standard
format, although in rare cases they can be represented in other ways.
Keytabs are used most often to allow server applications to accept
authentications from clients, but can also be used to obtain initial
credentials for client applications.

Keytabs are named using the format *type*\ ``:``\ *value*.  Usually
*type* is ``FILE`` and *value* is the absolute pathname of the file.
The other possible value for *type* is ``MEMORY``, which indicates a
temporary keytab stored in the memory of the current process.

A keytab contains one or more entries, where each entry consists of a
timestamp (indicating when the entry was written to the keytab), a
principal name, a key version number, an encryption type, and the
encryption key itself.

A keytab can be displayed using the :ref:`klist(1)` command with the
``-k`` option.  Keytabs can be created or appended to by extracting
keys from the KDC database using the :ref:`kadmin(1)` :ref:`ktadd`
command.  Keytabs can be manipulated using the :ref:`ktutil(1)` and
:ref:`k5srvutil(1)` commands.


Default keytab
--------------

The default keytab is used by server applications if the application
does not request a specific keytab.  The name of the default keytab is
determined by the following, in decreasing order of preference:

#. The **KRB5_KTNAME** environment variable.

#. The **default_keytab_name** profile variable in :ref:`libdefaults`.

#. The hardcoded default, |keytab|.


Default client keytab
---------------------

The default client keytab is used, if it is present and readable, to
automatically obtain initial credentials for GSSAPI client
applications.  The principal name of the first entry in the client
keytab is used by default when obtaining initial credentials.  The
name of the default client keytab is determined by the following, in
decreasing order of preference:

#. The **KRB5_CLIENT_KTNAME** environment variable.

#. The **default_client_keytab_name** profile variable in
   :ref:`libdefaults`.

#. The hardcoded default, |ckeytab|.
