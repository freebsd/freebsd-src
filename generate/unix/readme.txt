Unix ACPICA makefiles
---------------------

These makefiles are intended for generating the ACPICA utilities in
a Unix-like environment, with the original ACPICA code (not linuxized),
and in the original (git tree) ACPICA directory structure.

The top level makefile will generate the following utilities:

acpiexec
acpinames
acpisrc
acpixtract
iasl


Requirements
------------

make
gcc compiler (3+ or 4+)
bison
flex


Configuration
-------------

The Makefile.config file contains the configuration information:

HOST =       _CYGWIN            /* Host system, must appear in acenv.h */
CC =         gcc-4              /* C compiler */
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

