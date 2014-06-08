*evdev* documentation
---------------------

This package provides bindings to the generic input event interface in
Linux. The *evdev* interface serves the purpose of passing events
generated in the kernel directly to userspace through character
devices that are typically located in ``/dev/input/``.

This package also comes with bindings to *uinput*, the userspace input
subsystem. *Uinput* allows userspace programs to create and handle
input devices that can inject events directly into the input
subsystem.

Please refer to the :doc:`tutorial <tutorial>` and the :doc:`apidoc
<apidoc>` for usage information.

Contents
========

.. toctree::
   :maxdepth: 1

   tutorial
   install
   apidoc
   changelog


Similar Projects
================

* `python-uinput`_
* `ruby-evdev`_
* `evdev`_ (ctypes)


License
=======

Package :mod:`evdev` is released under the terms of the `Revised BSD License`_.


Todo
====

* Use libudev to find the uinput device node as well as the other input
  devices. Their locations are currently assumed to be ``/dev/uinput`` and
  ``/dev/input/*``.

* More tests.

* Better uinput support (setting device capabilities as in `python-uinput`_)

* Expose more input subsystem functionality (``EVIOCSKEYCODE``, ``EVIOCGREP`` etc)

* Figure out if using ``linux/input.h`` and other kernel headers in your
  userspace program binds it to the GPL2.


Indices and Tables
==================

* :ref:`genindex`
* :ref:`modindex`
* :ref:`search`


.. _`Revised BSD License`: https://raw.github.com/gvalkov/python-evdev/master/LICENSE
.. _python-uinput:     https://github.com/tuomasjjrasanen/python-uinput
.. _ruby-evdev:        http://technofetish.net/repos/buffaloplay/ruby_evdev/doc/
.. _evdev:             http://svn.navi.cx/misc/trunk/python/evdev/
