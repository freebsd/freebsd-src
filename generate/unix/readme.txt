Generic Unix ACPICA makefiles
-----------------------------

These makefiles are intended to generate the ACPICA utilities in
a Unix-like environment, with the original ACPICA code (not linuxized),
and in the original (git tree) ACPICA directory structure.

Windows binary versions of these tools are available at:

http://www.acpica.org/downloads/binary_tools.php

Documentation is available at acpica.org:

http://www.acpica.org/documentation/

The top level makefile will generate the following utilities:
Note: These utilities are tested and supported as 32-bit versions
only.

acpibin
acpiexec
acpihelp
acpinames
acpisrc
acpixtract
iasl

To generate all utilities:

cd acpica/generate/unix
make
make install   /* install all binaries to /usr/bin */


Requirements
------------

make
gcc compiler (4+)
bison or yacc
flex or lex


Configuration
-------------

The Makefile.config file contains the configuration information:

HOST =       _CYGWIN            /* Host system, must appear in acenv.h */
CC =         gcc                /* C compiler */
ACPICA_SRC = ../../../source    /* Location of acpica source tree */


Intermediate Files
------------------

The intermediate files for each utility (.o, etc.) are placed in the
subdirectory corresponding to each utility, not in the source code 
tree itself. This prevents collisions when different utilities compile
the same source modules with different options.


Output
------

The executable utilities are copied to the local bin directory.

"make install" will install the binaries to /usr/bin



1) acpibin, an AML file tool

acpibin compares AML files, dumps AML binary files to text files,
extracts binary AML from text files, and other AML file
manipulation.


2) acpiexec, a user-space AML interpreter

acpiexec allows the loading of ACPI tables and execution of control
methods from user space. Useful for debugging AML code and testing
the AML interpreter. Hardware access is simulated.


3) acpihelp, syntax help for ASL operators and reserved names

acpihelp displays the syntax for all of the ASL operators, as well
as information about the ASL/ACPI reserved names (4-char names that
start with underscore.)


4) acpinames, load and dump acpi namespace

acpinames loads an ACPI namespace from a binary ACPI table file.
This is a smaller version of acpiexec that loads an acpi table and
dumps the resulting namespace. It is primarily intended to demonstrate
the configurability of ACPICA.


5) acpisrc, a source code conversion tool

acpisrc converts the standard form of the acpica source release (included
here) into a version that meets Linux coding guidelines. This consists
mainly of performing a series of string replacements and transformations
to the code. It can also be used to clean the acpica source and generate
statistics.


6) acpixtract, extract binary ACPI tables from an acpidump

acpixtract is used to extract binary ACPI tables from the ASCII text
output of an acpidump utility (available on several different hosts.)


7) iasl, an optimizing ASL compiler/disassembler

iasl compiles ASL (ACPI Source Language) into AML (ACPI Machine
Language). This AML is suitable for inclusion as a DSDT in system
firmware. It also can disassemble AML, for debugging purposes.
