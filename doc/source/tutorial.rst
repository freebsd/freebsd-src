Tutorial
===========================

*libcbor* is a C library to encode, decode, and manipulate CBOR data. It is to CBOR to what `cJSON <https://github.com/DaveGamble/cJSON>`_ is to JSON. We assume you are familiar with the CBOR standard. If not, we recommend `cbor.io <http://cbor.io/>`_. 


Where to start
--------------

- Skim through the Crash course section below.
- Examples of of how to read, write, manipulate, and translate data to and from JSON using *libcbor* are in the `examples directory <https://github.com/PJK/libcbor/tree/master/examples>`_.
- The :doc:`API documentation <api>` is a complete reference of *libcbor*.


Crash course 
----------------

CBOR data objects are ``cbor_item_t``
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. literalinclude:: ../../examples/crash_course.c
    :language: C
    :start-after: // Part 1: Begin
    :end-before: // Part 1: End


Objects can be serialized and deserialized
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. literalinclude:: ../../examples/crash_course.c
    :language: C
    :start-after: // Part 2: Begin
    :end-before: // Part 2: End


Reference counting
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. literalinclude:: ../../examples/crash_course.c
    :language: C
    :start-after: // Part 3: Begin
    :end-before: // Part 3: End


Moving intermediate values
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. literalinclude:: ../../examples/crash_course.c
    :language: C
    :start-after: // Part 4: Begin
    :end-before: // Part 4: End


Ownership
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. literalinclude:: ../../examples/crash_course.c
    :language: C
    :start-after: // Part 5: Begin
    :end-before: // Part 5: End


Streaming IO
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

See https://github.com/PJK/libcbor/blob/master/examples/streaming_array.c, https://github.com/PJK/libcbor/blob/master/examples/streaming_parser.c