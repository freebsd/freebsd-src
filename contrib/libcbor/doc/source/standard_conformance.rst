IETF standard conformance
=========================

*libcbor* is, generally speaking, a very faithful implementation of `IETF RFC 8949 (STD 94) <https://www.rfc-editor.org/info/std94>`_. There are, however, some limitations related to the numerical range and precision available in portable C99.

Bytestring length
-------------------
There is no explicit limitation of indefinite length byte strings. [#]_ *libcbor* will not handle byte strings with more chunks than the maximum value of :type:`size_t`. On any sane platform, such string would not fit in the memory anyway. It is, however, possible to process arbitrarily long strings and byte strings using the streaming decoder.

.. [#] https://www.rfc-editor.org/rfc/rfc8949.html#section-3.2.3

"Half-precision" IEEE 754 floats
---------------------------------
As of C99 and even C11, there is no standard implementation for 2 bytes floats. *libcbor* packs them as a `float <https://en.cppreference.com/w/c/language/type>`. When encoding, *libcbor* selects the appropriate wire representation based on metadata and the actual value. This applies both to canonical and normal mode.

For more information on half-float serialization, please refer to the section on :ref:`api_type_7_hard_floats`.

