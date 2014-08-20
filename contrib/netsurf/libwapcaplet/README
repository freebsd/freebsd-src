LibWapcaplet - a string internment library
==========================================

Overview
--------

LibWapcaplet provides a reference counted string internment system
designed to store small strings and allow rapid comparison of them in
terms of equality. It supports a caseless comparison where it will
automatically intern a lowercased variant of the string and use that
for comparison if needed.

Rationale
---------

Prior to LibWapcaplet, LibParserUtils contained a dictionary and hash
implementation along with a red-black tree implementation
internally. These three things were then used by client applications
and libraries such as LibCSS. However, the code was deemed to be
inefficient and the features in the wrong library. The behaviour
required of the client libraries was therefore split out so that
internment would still be able to be shared between different client
libraries in the same application. (E.g. LibCSS and Hubbub)

For those interested, The name 'Wapcaplet' is from the Monty Python
sketch in which Mr Simpson (who is not French) attempts to sell
122,000 miles of string which was unfortunately cut up into 3 inch
lengths, and Adrian Wapcaplet comes up with the idea of "Simpson's
individual emperor stringettes - Just the right length!"

Requirements
------------

To compile LibWapcaplet you need:

 * GNU Make 3.81 or better
 * A version of GCC capable of -MMD -MF (unless you change the build
   system)

To compile the test suite you need:

 * Check v0.9.5 or better.

Compilation
-----------

To build LibWapcaplet in release mode, type 'make'. To build it in
debug mode type 'make BUILD=debug'.  To install, run 'make
install'. If you wish to install LibWapcaplet into somewhere other
than /usr/local/ then add PREFIX=/path/to/place to the installation
make command.

Verification
------------

To build and run the tests, run 'make test'.

In release mode, fewer tests will be run as the assert() calls will be
elided.

API documentation
-----------------

For API documentation see include/libwapcaplet/libwapcaplet.h

