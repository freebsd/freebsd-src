*evdev*
-------

This package provides bindings to the generic input event interface in
Linux. The *evdev* interface serves the purpose of passing events
generated in the kernel directly to userspace through character
devices that are typically located in ``/dev/input/``.

This package also comes with bindings to *uinput*, the userspace input
subsystem. *Uinput* allows userspace programs to create and handle
input devices that can inject events directly into the input
subsystem.

Documentation:
    http://python-evdev.readthedocs.org/en/latest/

Development:
    https://github.com/gvalkov/python-evdev

Package:
    http://pypi.python.org/pypi/evdev

Changelog:
    http://python-evdev.readthedocs.org/en/latest/changelog.html
