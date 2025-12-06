..
    WHEN EDITING MAKE SURE EACH SENTENCE STARTS ON A NEW LINE

..
    IT HELPS RENDERERS TO DO THE RIGHT THING WRT SPACE

..
    IT HELPS PEOPLE DIFFING THE CHANGES

..
    WHEN EDITING MAKE SURE EACH SENTENCE STARTS ON A NEW LINE

..
    IT HELPS RENDERERS TO DO THE RIGHT THING WRT SPACE

..
    IT HELPS PEOPLE DIFFING THE CHANGES

..
    WHEN EDITING MAKE SURE EACH SENTENCE STARTS ON A NEW LINE

..
    IT HELPS RENDERERS TO DO THE RIGHT THING WRT SPACE

..
    IT HELPS PEOPLE DIFFING THE CHANGES

..
    WHEN EDITING MAKE SURE EACH SENTENCE STARTS ON A NEW LINE

..
    IT HELPS RENDERERS TO DO THE RIGHT THING WRT SPACE

..
    IT HELPS PEOPLE DIFFING THE CHANGES

.. program:: unbound-checkconf

unbound-checkconf(8)
====================

Synopsis
--------

**unbound-checkconf** [``-hf``] [``-o option``] [cfgfile]

Description
-----------

``unbound-checkconf`` checks the configuration file for the
:doc:`unbound(8)</manpages/unbound>` DNS resolver for syntax and other errors.
The config file syntax is described in
:doc:`unbound.conf(5)</manpages/unbound.conf>`.

The available options are:

.. option:: -h

    Show the version and commandline option help.

.. option:: -f

    Print full pathname, with chroot applied to it.
    Use with the :option:`-o` option.

.. option:: -q

    Make the operation quiet, suppress output on success.

.. option:: -o <option>

    If given, after checking the config file the value of this option is
    printed to stdout.
    For ``""`` (disabled) options an empty line is printed.

.. option:: cfgfile

    The config file to read with settings for Unbound.
    It is checked.
    If omitted, the config file at the default location is checked.

Exit Code
---------

The ``unbound-checkconf`` program exits with status code 1 on error, 0 for a
correct config file.

Files
-----

@ub_conf_file@
    Unbound configuration file.

See Also
--------

:doc:`unbound.conf(5)</manpages/unbound.conf>`,
:doc:`unbound(8)</manpages/unbound>`.
