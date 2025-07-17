Type 4 – Arrays
=============================

CBOR arrays, just like :doc:`byte strings <type_2>` and :doc:`strings <type_3>`, can be encoded either as definite, or as indefinite.
Definite arrays have a fixed size which is stored in the header, whereas indefinite arrays do not and are terminated by a special "break" byte instead.

Arrays are explicitly created or decoded as definite or indefinite and will be encoded using the corresponding wire representation, regardless of whether the actual size is known at the time of encoding.

.. note:: Indefinite arrays can be conveniently used with streaming :doc:`decoding <streaming_decoding>` and :doc:`encoding <streaming_encoding>`.

==================================  =====================================================================================
Corresponding :type:`cbor_type`     ``CBOR_TYPE_ARRAY``
Number of allocations (definite)    Two plus any manipulations with the data
Number of allocations (indefinite)  Two plus logarithmically many
                                    reallocations relative to additions
Storage requirements (definite)     ``(sizeof(cbor_item_t) + 1) * size``
Storage requirements (indefinite)   ``<= sizeof(cbor_item_t) + sizeof(cbor_item_t) * size * BUFFER_GROWTH``
==================================  =====================================================================================


Examples
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

::

    0x9f        Start indefinite array
        0x01        Unsigned integer 1
        0xff        "Break" control token

::

    0x9f        Start array, 1B length follows
    0x20        Unsigned integer 32
        ...        32 items follow


Getting metadata
~~~~~~~~~~~~~~~~~

.. doxygenfunction:: cbor_array_size
.. doxygenfunction:: cbor_array_allocated
.. doxygenfunction:: cbor_array_is_definite
.. doxygenfunction:: cbor_array_is_indefinite

Reading data
~~~~~~~~~~~~~

.. doxygenfunction:: cbor_array_handle
.. doxygenfunction:: cbor_array_get

Creating new items
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. doxygenfunction:: cbor_new_definite_array
.. doxygenfunction:: cbor_new_indefinite_array


Modifying items
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. doxygenfunction:: cbor_array_push
.. doxygenfunction:: cbor_array_replace
.. doxygenfunction:: cbor_array_set
