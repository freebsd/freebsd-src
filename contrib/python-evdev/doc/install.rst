Installation
============

Before installing :mod:`evdev`, make sure that the Python and Linux
kernel headers are installed on your system.

On a Debian compatible OS:

.. code-block:: bash

    $ apt-get install python-dev
    $ apt-get install linux-headers-$(uname -r)

On a Redhat compatible OS:

.. code-block:: bash

    $ yum install python-devel
    $ yum install kernel-headers-$(uname -r)

The latest stable version can be installed from pypi_, while the
development version can be installed from github_:

.. code-block:: bash

    $ pip install evdev  # latest stable version
    $ pip install git+git://github.com/gvalkov/python-evdev.git # latest development version

:mod:`evdev` can also be installed like any other :mod:`setuptools`
package.

.. code-block:: bash

    $ git clone github.com/gvalkov/python-evdev.git
    $ cd python-evdev
    $ git checkout $versiontag
    $ python setup.py install

The :mod:`evdev` package works with CPython **>= 2.7**.

.. _pypi:              http://pypi.python.org/pypi/evdev
.. _github:            https://github.com/gvalkov/python-evdev
