Principal manipulation and parsing
==================================

Kerberos principal structure

..

:c:type:`krb5_principal_data`

:c:type:`krb5_principal`

..

Create and free principal

..

:c:func:`krb5_build_principal()`

:c:func:`krb5_build_principal_alloc_va()`

:c:func:`krb5_build_principal_ext()`

:c:func:`krb5_copy_principal()`

:c:func:`krb5_free_principal()`

:c:func:`krb5_cc_get_principal()`

..

Comparing

..

:c:func:`krb5_principal_compare()`

:c:func:`krb5_principal_compare_flags()`

:c:func:`krb5_principal_compare_any_realm()`

:c:func:`krb5_sname_match()`

:c:func:`krb5_sname_to_principal()`

..


Parsing:

..

:c:func:`krb5_parse_name()`

:c:func:`krb5_parse_name_flags()`

:c:func:`krb5_unparse_name()`

:c:func:`krb5_unparse_name_flags()`

..

Utilities:

..

:c:func:`krb5_is_config_principal()`

:c:func:`krb5_kuserok()`

:c:func:`krb5_set_password()`

:c:func:`krb5_set_password_using_ccache()`

:c:func:`krb5_set_principal_realm()`

:c:func:`krb5_realm_compare()`

..
