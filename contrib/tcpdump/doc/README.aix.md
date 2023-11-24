# Compiling tcpdump on AIX

* Autoconf works everywhere.

## AIX 7.1/POWER7

* Only local libpcap is suitable.
* CMake 3.16.0 does not work.
* GCC 8.3.0 and XL C 12.1.0.0 work.
* System m4 does not work, GNU m4 1.4.17 works.

## AIX 7.2/POWER8

* Only local libpcap is suitable.
* GCC 7.2.0 and XL C 13.1.3.6 work.
* System m4 does not work, GNU m4 1.4.17 works.

