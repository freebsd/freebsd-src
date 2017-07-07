Differences between Heimdal and MIT Kerberos API
================================================

.. tabularcolumns:: |l|l|

.. table::

 ======================================== =================================================
  :c:func:`krb5_auth_con_getaddrs()`       H5l: If either of the pointers to local_addr
                                           and remote_addr is not NULL, it is freed
                                           first and then reallocated before being
                                           populated with the content of corresponding
                                           address from authentication context.
  :c:func:`krb5_auth_con_setaddrs()`       H5l: If either address is NULL, the previous
                                           address remains in place
  :c:func:`krb5_auth_con_setports()`       H5l: Not implemented as of version 1.3.3
  :c:func:`krb5_auth_con_setrecvsubkey()`  H5l: If either port is NULL, the previous
                                           port remains in place
  :c:func:`krb5_auth_con_setsendsubkey()`  H5l: Not implemented as of version 1.3.3
  :c:func:`krb5_cc_set_config()`           MIT: Before version 1.10 it was assumed that
                                           the last argument *data* is ALWAYS non-zero.
  :c:func:`krb5_cccol_last_change_time()`  H5l takes 3 arguments: krb5_context context,
                                           const char \*type, krb5_timestamp \*change_time
                                           MIT takes two arguments: krb5_context context,
                                           krb5_timestamp \*change_time
  :c:func:`krb5_set_default_realm()`       H5l: Caches the computed default realm context
                                           field.  If the second argument is NULL,
                                           it tries to retrieve it from libdefaults or DNS.
                                           MIT: Computes the default realm each time
                                           if it wasn't explicitly set in the context
 ======================================== =================================================
