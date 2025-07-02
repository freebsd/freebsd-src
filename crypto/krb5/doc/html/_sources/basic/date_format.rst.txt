.. _datetime:

Supported date and time formats
===============================

.. _duration:

Time duration
-------------

This format is used to express a time duration in the Kerberos
configuration files and user commands.  The allowed formats are:

 ====================== ============== ============
 Format                      Example        Value
 ---------------------- -------------- ------------
  h:m[:s]                36:00          36 hours
  NdNhNmNs               8h30s          8 hours 30 seconds
  N (number of seconds)  3600           1 hour
 ====================== ============== ============

Here *N* denotes a number, *d* - days, *h* - hours, *m* - minutes,
*s* - seconds.

.. note::

    The time interval should not exceed 2147483647 seconds.

Examples::

    Request a ticket valid for one hour, five hours, 30 minutes
    and 10 days respectively:

      kinit -l 3600
      kinit -l 5:00
      kinit -l 30m
      kinit -l "10d 0h 0m 0s"


.. _getdate:

getdate time
------------

Some of the kadmin and kdb5_util commands take a date-time in a
human-readable format.  Some of the acceptable date-time
strings are:

 +-----------+------------------+-----------------+
 |           |   Format         | Example         |
 +===========+==================+=================+
 | Date      |   mm/dd/yy       | 07/27/12        |
 |           +------------------+-----------------+
 |           | month dd, yyyy   | Jul 27, 2012    |
 |           +------------------+-----------------+
 |           |   yyyy-mm-dd     |  2012-07-27     |
 +-----------+------------------+-----------------+
 | Absolute  | HH:mm[:ss]pp     |  08:30 PM       |
 | time      +------------------+-----------------+
 |           | hh:mm[:ss]       |  20:30          |
 +-----------+------------------+-----------------+
 | Relative  | N tt             |  30 sec         |
 | time      |                  |                 |
 +-----------+------------------+-----------------+
 | Time zone | Z                |  EST            |
 |           +------------------+-----------------+
 |           | z                |  -0400          |
 +-----------+------------------+-----------------+

(See :ref:`abbreviation`.)

Examples::

    Create a principal that expires on the date indicated:
        addprinc test1 -expire "3/27/12 10:00:07 EST"
        addprinc test2 -expire "January 23, 2015 10:05pm"
        addprinc test3 -expire "22:00 GMT"
    Add a principal that will expire in 30 minutes:
        addprinc test4 -expire "30 minutes"


.. _abstime:

Absolute time
-------------

This rarely used date-time format can be noted in one of the
following ways:


 +------------------------+----------------------+--------------+
 | Format                 | Example              | Value        |
 +========================+======================+==============+
 | yyyymmddhhmmss         | 20141231235900       | One minute   |
 +------------------------+----------------------+ before 2015  |
 | yyyy.mm.dd.hh.mm.ss    | 2014.12.31.23.59.00  |              |
 +------------------------+----------------------+              |
 | yymmddhhmmss           | 141231235900         |              |
 +------------------------+----------------------+              |
 | yy.mm.dd.hh.mm.ss      | 14.12.31.23.59.00    |              |
 +------------------------+----------------------+              |
 | dd-month-yyyy:hh:mm:ss | 31-Dec-2014:23:59:00 |              |
 +------------------------+----------------------+--------------+
 | hh:mm:ss               | 20:00:00             | 8 o'clock in |
 +------------------------+----------------------+ the evening  |
 | hhmmss                 | 200000               |              |
 +------------------------+----------------------+--------------+

(See :ref:`abbreviation`.)

Example::

    Set the default expiration date to July 27, 2012 at 20:30
    default_principal_expiration = 20120727203000


.. _abbreviation:

Abbreviations used in this document
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

| *month*  : locale’s month name or its abbreviation;
| *dd*   : day of month (01-31);
| *HH*   : hours (00-12);
| *hh*   : hours (00-23);
| *mm*   : in time - minutes (00-59); in date - month (01-12);
| *N*    : number;
| *pp*   : AM or PM;
| *ss*   : seconds  (00-60);
| *tt*   : time units (hours, minutes, min, seconds, sec);
| *yyyy* : year;
| *yy*   : last two digits of the year;
| *Z*    : alphabetic time zone abbreviation;
| *z*    : numeric time zone;

.. note::

     - If the date specification contains spaces, you may need to
       enclose it in double quotes;
     - All keywords are case-insensitive.
