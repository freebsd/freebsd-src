How to build this documentation from the source
===============================================

Pre-requisites for a simple build, or to update man pages:

* Sphinx 1.0.4 or higher (See http://sphinx.pocoo.org) with the autodoc
  extension installed.

Additional prerequisites to include the API reference based on Doxygen
markup:

* Python 2.5 with the Cheetah, lxml, and xml modules
* Doxygen


Simple build without API reference
----------------------------------

To test simple changes to the RST sources, you can build the
documentation without the Doxygen reference by running, from the doc
directory::

    sphinx-build . test_html

You will see a number of warnings about missing files.  This is
expected.  If there is not already a ``doc/version.py`` file, you will
need to create one by first running ``make version.py`` in the
``src/doc`` directory of a configured build tree.


Updating man pages
------------------

Man pages are generated from the RST sources and checked into the
``src/man`` directory of the repository.  This allows man pages to be
installed without requiring Sphinx when using a source checkout.  To
regenerate these files, run ``make man`` from the man subdirectory
of a configured build tree.  You can also do this from an unconfigured
source tree with::

    cd src/man
    make -f Makefile.in top_srcdir=.. srcdir=. man
    make clean

As with the simple build, it is normal to see warnings about missing
files when rebuilding the man pages.


Building for a release tarball or web site
------------------------------------------

To generate documentation in HTML format, run ``make html`` in the
``doc`` subdirectory of a configured build tree (the build directory
corresponding to ``src/doc``, not the top-level ``doc`` directory).
The output will be placed in the top-level ``doc/html`` directory.
This build will include the API reference generated from Doxygen
markup in the source tree.

Documentation generated this way will use symbolic names for paths
(like ``BINDIR`` for the directory containing user programs), with the
symbolic names being links to a table showing typical values for those
paths.

You can also do this from an unconfigured source tree with::

    cd src/doc
    make -f Makefile.in SPHINX_ARGS= htmlsrc


Building for an OS package or site documentation
------------------------------------------------

To generate documentation specific to a build of MIT krb5 as you have
configured it, run ``make substhtml`` in the ``doc`` subdirectory of a
configured build tree (the build directory corresponding to
``src/doc``, not the top-level ``doc`` directory).  The output will be
placed in the ``html_subst`` subdirectory of that build directory.
This build will include the API reference.

Documentation generated this way will use concrete paths (like
``/usr/local/bin`` for the directory containing user programs, for a
default custom build).
