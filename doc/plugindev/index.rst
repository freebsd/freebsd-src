For plugin module developers
============================

Kerberos plugin modules allow increased control over MIT krb5 library
and server behavior.  This guide describes how to create dynamic
plugin modules and the currently available pluggable interfaces.

See :ref:`plugin_config` for information on how to register dynamic
plugin modules and how to enable and disable modules via
:ref:`krb5.conf(5)`.

.. TODO: update the above reference when we have a free-form section
   in the admin guide about plugin configuration


Contents
--------

.. toctree::
   :maxdepth: 2

   general.rst
   clpreauth.rst
   kdcpreauth.rst
   ccselect.rst
   pwqual.rst
   kadm5_hook.rst
   hostrealm.rst
   localauth.rst
   locate.rst
   profile.rst
   gssapi.rst
   internal.rst

.. TODO: GSSAPI mechanism plugins
