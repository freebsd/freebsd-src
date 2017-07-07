Configuration Files
===================

Kerberos uses configuration files to allow administrators to specify
settings on a per-machine basis.  :ref:`krb5.conf(5)` applies to all
applications using the Kerboros library, on clients and servers.
For KDC-specific applications, additional settings can be specified in
:ref:`kdc.conf(5)`; the two files are merged into a configuration profile
used by applications accessing the KDC database directly.  :ref:`kadm5.acl(5)`
is also only used on the KDC, it controls permissions for modifying the
KDC database.

Contents
--------
.. toctree::
   :maxdepth: 1

   krb5_conf
   kdc_conf
   kadm5_acl
