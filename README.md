OpenCSD - An open source CoreSight(tm) Trace Decode library        {#mainpage}
===========================================================

This library provides an API suitable for the decode of ARM(r) CoreSight(tm) trace streams.

The library will decode formatted trace in three stages:

1. *Frame Deformatting* : Removal CoreSight frame formatting from individual trace streams.
2. *Packet Processing*  : Separate individual trace streams into discrete packets.
3. *Packet Decode*      : Convert the packets into fully decoded trace describing the program flow on a core.

The library is implemented in C++ with an optional "C" API.

Library Versioning
------------------

From version 0.4, library versioning will use a semantic versioning format
(per http://semver.org) of the form _Major.minor.patch_ (M.m.p).

Internal library version calls, documentation and git repository will use this format moving forwards.
Where a patch version is not quoted, or quoted as .x then comments will apply to the entire release.

Releases will be at M.m.0, with patch version incremented for bugfixes or documentation updates.

Releases will appear on the master branch in the git repository with an appropriate version tag.

CoreSight Trace Component Support.
----------------------------------

_Current Version 0.12.0_

### Current support:

- ETMv4 (v4.4) instruction trace - packet processing and packet decode.
- PTM   (v1.1) instruction trace - packet processing and packet decode.
- ETMv3 (v3.5) instruction trace - packet processing and packet decode.
- ETMv3 (v3.5) data trace - packet processing.
- STM   (v1.1) software trace - packet processing and packet decode.

- External Decoders - support for addition of external / custom decoders into the library.

### Support to be added:

- ITM software trace - packet processing and decode.
- ETMv3 data trace - packet decode.
- ETMv4 data trace - packet processing and decode.

Note: for ITM and STM, packet decode is combining Master+Channel+Marker+Payload packets into a single generic
output packet.


Note on the Git Repository.
---------------------------

This git repository for OpenCSD contains only source for the OpenCSD decoder library.
From version 0.4, releases appear as versioned tags on the master branch.

From version 0.7.4, the required updates to CoreSight drivers and perf, that are not
currently upstream in the linux kernel tree, are now contained in a separate
repository to be found at:

https://github.com/Linaro/perf-opencsd


Documentation
-------------

API Documentation is provided inline in the source header files, which use the __doxygen__ standard mark-up.
Run `doxygen` on the `./doxygen_config.dox` file located in the `./docs` directory..

    doxygen ./doxygen_config.dox

This will produce the documentation in the `./docs/html` directory. The doxygen configuration also includes
the `*.md` files as part of the documentation.

Application Programming using the Library
-----------------------------------------

See the [programmers guide](@ref prog_guide) for details on usage of the library in custom applications.
(`./docs/prog_guide/prog_guide_main.md`).


Building and Installing the Library
-----------------------------------

See [build_libs.md](@ref build_lib) in the `./docs` directory for build details.

The linux build makefile now contains options to install the library for a linux environment.


How the Library is used in Linux `perf`
---------------------------------------
The library and additional infrastructure for programming CoreSight components has been integrated 
with the standard linux perfomance analysis tool `perf`.


See [HOWTO.md](@ref howto_perf) for details.

How to use the Library, perf and Trace for AutoFDO
--------------------------------------------------
Capturing trace using perf and decoding using the library can 
generate profiles for AutoFDO.

See [autofdo.md](@ref AutoFDO) for details and scripts.

(`./tests/auto-fdo/autofdo.md`).


Version and Modification Information
====================================

- _Version 0.001_:  Library development - tested with `perf` tools integration - BKK16, 8th March 2016
- _Version 0.002_:  Library development - added in PTM decoder support. Restructure header dir, replaced ARM rctdl prefix with opencsd/ocsd.
- _Version 0.003_:  Library development - added in ETMv3 instruction decoder support.
- _Version 0.4_  :  Library development - updated decode tree and C-API for generic decoder handling. Switch to semantic versioning.
- _Version 0.4.1_:  Minor Update & Bugfixes - fix to PTM decoder, ID checking on test program, adds NULL_TS support in STM packet processor.
- _Version 0.4.2_:  Minor Update - Update to documentation for perf usage in 4.8 kernel branch.
- _Version 0.5.0_:  Library Development - external decoder support. STM full decode.
- _Version 0.5.1_:  Minor Update & Bugfixes - Update HOWTO for kernel 4.9. Build fixes for parallel builds
- _Version 0.5.2_:  Minor Update & Bugfixes - Update trace info packet string o/p + Cycle count packet bugfixes.
- _Version 0.5.3_:  Doc update for using AutoFDO with ETM and additional timestamp and cycle count options.
- _Version 0.5.4_:  Updates: X-compile for arm/arm64.  Remove deprecated VS2010 builds. Bugfix: GCC inline semantics in debug build. 
- _Version 0.6.0_:  Packet printers moved from tests into the main library. C++ and C APIs updated to allow clients to use them. 
                    Update to allow perf to insert barrier packets (4xFSYNC) which the decoder can be made to use to reset the decode state.
- _Version 0.6.1_:  Bugfix: instruction follower bug on A32 branch to T32.
- _Version 0.7.0_:  Add handling for trace return stack feature to ETMv4 and PTM trace.
- _Version 0.7.1_:  Bugfix: ETMv3 packet processor.
- _Version 0.7.2_:  Bugfix: ETMv4 decoder - fix exact match packet address follower.
- _Version 0.7.3_:  Bugfix: PTM decoder - issues with initialisation and ASYNC detection.
- _Version 0.7.4_:  Notification of change of repository for perf extensions. gcc 6.x build fixes.
- _Version 0.7.5_:  Bugfix: ETMv4 decoder memory leak. Linux build update - header dependencies force rebuild.
- _Version 0.8.0_:  Header restructure and build update to enable linux version to install library and C-API headers in standard locations.
                    Library output naming changed from 'cstraced' to 'opencsd'.
- _Version 0.8.1_:  Minor updates: Use install tool to copy headers. Changes to HOWTO for perf usage.                    
- _Version 0.8.2_:  Bugfix: C++ init errors fixed for CLANG build process.
- _Version 0.8.3_:  Bugfix: ETMv4 decoder issues fixed.
- _Version 0.8.4_:  build: makefile updates and improvements to get build process compatible with Debian packaging.
- _Version 0.9.0_:  Performance improvements for perf: Additional info in instruction range output packet. Caching memory accesses. 
				    Added Programmers guide to documentation.
- _Version 0.9.1_:  Bugfix: Crash during decode when first memory access is to address where no image provided.
- _Version 0.9.2_:  Bugfix: ETMv4: Incorrect Exception number output for Genric exception packets.
                    AutoFDO: update documentation for AutoFDO usage and add in "record.sh" script
- _Version 0.9.3_:  Bugfix: Test snapshot library not handling 'offset' parameters in dump file sections.
                    Install: ocsd_if_version.h moved to opencsd/include to allow installation on OS & use in compiling client apps.
- _Version 0.10.0_: __Updates__: Add additional information about the last instruction to the generic output packet.
                    __Docs__: update docs for updated output packet.
                    __Bugfix__: typecast removed from OCSD_VER_NUM in ocsd_if_version.h to allow use in C pre-processor.
                    __Bugfix__: ETMV4: Interworking ISA change between A32-T32 occasionally missed during instruction decode.
- _Version 0.10.1_: __Updates__: Build update - allow multi-thread make (make -j<N>).
                    __Docs__: Minor update to AutoFDO documentation.
- _Version 0.11.0_: __Update__: ETM v4 decoder updated to support ETM version up to v4.4
                    __Update__: Memory access callback function - added new callback signature to provide TraceID to client when requesting memory.
                    __Update__: Created new example program to demonstrate using memory buffer in APIs.
                    __Bugfix__: Typos in docs and source.
                    __Bugfix__: Memory accessor - validate callback return values.
- _Version 0.11.1_: __Update__: build:- change -fpic to -fPIC to allow Debian build on sparc.
                    __Bugfix__: build:- remove unused variable
- _Version 0.11.2_: __Update__: docs:- HOWTO.md update to match new perf build requirements.
                    __Bugfix__: Minor spelling typos fixed.
- _Version 0.12.0_: __Update__: Frame deformatter - TPIU FSYNC and HSYNC support added.
                    __Update__: ETM v4: Bugfix & clarification on Exception trace handling. Where exception occurs at a branch target before any instructions
                    have been executed, the preferred return address is also the target address of the branch instruction. This case now includes as specific flag in
                    the packet. Additionally any context change associated with this target address was being applied incorrectly.
                    __Update__: Core / Architecture mapping to core names as used by test programs / snapshots updated to include additional recent ARM cores.
                    __Update__: Docs: Update to reflect new exception flag. Update test program example to reflect latest output.
                    __Bugfix__: ETM v4: Valid trace info packet was not handled correctly (0x01, 0x00).
                    __Bugfix__: ETM v4: Error messaging on commit stack overflow.


Licence Information
===================

This library is licensed under the [BSD three clause licence.](http://directory.fsf.org/wiki/License:BSD_3Clause)

A copy of this license is in the `LICENCE` file included with the source code.

Contact
=======

Using the github site: https://github.com/Linaro/OpenCSD

Mailing list: coresight@lists.linaro.org
