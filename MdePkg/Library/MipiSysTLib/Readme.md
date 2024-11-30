## Introduction of MipiSysTLib ##
MipiSysTLib library is a upper level library consuming MIPI SYS-T submodule.
It provides MIPI-related APIs in EDK2 format to be consumed.

## MipiSysTLib Version ##
EDK2 supports building with v1.1+edk2 official version which was fully validated.

## HOW to Install MipiSysTLib for UEFI Building ##
MIPI SYS-T repository was added as a submodule of EDK2 project. Please
refer to edk2/Readme.md for how to clone the code.

## About GenMipiSystH.py ##
"GenMipiSystH.py" is a Python script which is used for customizing the
mipi_syst.h.in in mipi sys-T repository. The resulting file, mipi_syst.h, will
be put to same folder level as this script.
```
  mipisyst submodule                        MipiSysTLib library
|---------------------| GenMipiSystH.py   |---------------------|
|   mipi_syst.h.in    |-----------------> |   mipi_syst.h       |
|---------------------|                   |---------------------|
```
This script needs to be done once by a developer when adding some
project-related definition or a new version of mipi_syst.h.in was released.
Normal users do not need to do this, since the resulting file is stored
in the EDK2 git repository.
