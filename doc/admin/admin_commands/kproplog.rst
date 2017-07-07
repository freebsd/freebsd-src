.. _kproplog(8):

kproplog
========

SYNOPSIS
--------

**kproplog** [**-h**] [**-e** *num*] [-v]
**kproplog** [-R]


DESCRIPTION
-----------

The kproplog command displays the contents of the KDC database update
log to standard output.  It can be used to keep track of incremental
updates to the principal database.  The update log file contains the
update log maintained by the :ref:`kadmind(8)` process on the master
KDC server and the :ref:`kpropd(8)` process on the slave KDC servers.
When updates occur, they are logged to this file.  Subsequently any
KDC slave configured for incremental updates will request the current
data from the master KDC and update their log file with any updates
returned.

The kproplog command requires read access to the update log file.  It
will display update entries only for the KDC it runs on.

If no options are specified, kproplog displays a summary of the update
log.  If invoked on the master, kproplog also displays all of the
update entries.  If invoked on a slave KDC server, kproplog displays
only a summary of the updates, which includes the serial number of the
last update received and the associated time stamp of the last update.


OPTIONS
-------

**-R**
    Reset the update log.  This forces full resynchronization.  If used
    on a slave then that slave will request a full resync.  If used on
    the master then all slaves will request full resyncs.

**-h**
    Display a summary of the update log.  This information includes
    the database version number, state of the database, the number of
    updates in the log, the time stamp of the first and last update,
    and the version number of the first and last update entry.

**-e** *num*
    Display the last *num* update entries in the log.  This is useful
    when debugging synchronization between KDC servers.

**-v**
    Display individual attributes per update.  An example of the
    output generated for one entry::

        Update Entry
           Update serial # : 4
           Update operation : Add
           Update principal : test@EXAMPLE.COM
           Update size : 424
           Update committed : True
           Update time stamp : Fri Feb 20 23:37:42 2004
           Attributes changed : 6
                 Principal
                 Key data
                 Password last changed
                 Modifying principal
                 Modification time
                 TL data


ENVIRONMENT
-----------

kproplog uses the following environment variables:

* **KRB5_KDC_PROFILE**


SEE ALSO
--------

:ref:`kpropd(8)`
