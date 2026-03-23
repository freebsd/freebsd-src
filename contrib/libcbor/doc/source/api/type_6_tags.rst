Type 6 – Semantic tags 
=============================

Tag are additional metadata that can be used to extend or specialize the meaning or interpretation of the other data items.

For example, one might tag an array of numbers to communicate that it should be interpreted as a vector.

Please consult the official `IANA repository of CBOR tags <https://www.iana.org/assignments/cbor-tags/cbor-tags.xhtml>`_ for known registered values.

Please note that libcbor does *not* understand the semantics of tags and will
process all `well-formed <https://datatracker.ietf.org/doc/html/rfc8949#name-terminology>`_
tags regardless of whether they are valid with respect to the data they are
applied to.

==================================  ======================================================
Corresponding :type:`cbor_type`     ``CBOR_TYPE_TAG``
Number of allocations               One plus any manipulations with the data
                                    reallocations relative  to chunk count
Storage requirements                ``sizeof(cbor_item_t) + the tagged item``
==================================  ======================================================

.. doxygenfunction:: cbor_new_tag
.. doxygenfunction:: cbor_tag_item
.. doxygenfunction:: cbor_tag_value
.. doxygenfunction:: cbor_tag_set_item

