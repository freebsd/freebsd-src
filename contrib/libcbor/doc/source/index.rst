libcbor
===================================

Documentation for version |release|, updated on |today|.

Overview
--------
*libcbor* is a C library for parsing and generating CBOR_, the general-purpose schema-less binary data format.


Main features
 - Complete `IETF RFC 8949 (STD 94) <https://www.rfc-editor.org/info/std94>`_ conformance [#]_
 - Robust C99 implementation
 - Layered architecture offers both control and convenience
 - Flexible memory management
 - No shared global state - threading friendly [#]_
 - Proper handling of UTF-8
 - Full support for streams & incremental processing
 - Extensive documentation and test suite
 - No runtime dependencies, small footprint

.. [#] See :doc:`standard_conformance`

.. [#] With the exception of custom memory allocators (see :doc:`api/item_reference_counting`)

Contents
----------
.. toctree::

   getting_started
   using
   api
   tests
   standard_conformance
   internal
   changelog
   development

.. _CBOR: https://www.rfc-editor.org/info/std94
