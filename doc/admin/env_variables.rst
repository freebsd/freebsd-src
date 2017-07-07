Environment variables
=====================

The following environment variables can be used during runtime:

**KRB5_CONFIG**
    Main Kerberos configuration file.  Multiple filenames can be
    specified, separated by a colon; all files which are present will
    be read.  (See :ref:`mitK5defaults` for the default path.)

**KRB5_KDC_PROFILE**
    KDC configuration file.  (See :ref:`mitK5defaults` for the default
    name.)

**KRB5_KTNAME**
    Default keytab file name.  (See :ref:`mitK5defaults` for the
    default name.)

**KRB5_CLIENT_KTNAME**
    Default client keytab file name.  (See :ref:`mitK5defaults` for
    the default name.)

**KRB5CCNAME**
    Default name for the credentials cache file, in the form *type*\:\
    *residual*.  The type of the default cache may determine the
    availability of a cache collection.  For instance, a default cache
    of type ``DIR`` causes caches within the directory to be present
    in the global cache collection.

**KRB5RCACHETYPE**
    Default replay cache type.  Defaults to ``dfl``.  A value of
    ``none`` disables the replay cache.

**KRB5RCACHEDIR**
    Default replay cache directory.  (See :ref:`mitK5defaults` for the
    default location.)

**KPROP_PORT**
    :ref:`kprop(8)` port to use.  Defaults to 754.

**KRB5_TRACE**
    Filename for trace-logging output (introduced in release 1.9).
    For example, ``env KRB5_TRACE=/dev/stdout kinit`` would send
    tracing information for kinit to ``/dev/stdout``.  Some programs
    may ignore this variable (particularly setuid or login system
    programs).
