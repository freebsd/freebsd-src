Below is a list of Skein files included on the NIST submission CD, along 
with a very brief description of each file. In both the reference and 
optimized directories, all C files should be compiled to generate a 
SHA3 NIST API "library" for Skein.

++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
The following files are identical and common between the reference and optimized 
versions of the code:

File Name           Description
--------------------------------------------------------------------------------
brg_endian.h        Brian Gladman's header file to auto-detect CPU endianness
                        (with a few extensions for handling various platforms/compilers)


brg_types.h         Brian Gladman's header file to auto-detect integer types
                        (with a few extensions for handling various platforms/compilers)


SHA3api_ref.h       API definitions for SHA3 API, implemented in SHA3api_ref.c


SHA3api_ref.c       "Wrapper" code that implements the NIST SHA3 API on top of the
                    Skein API.


skein_debug.h       Header for with routines used internally by Skein routines for
                    generating debug i/o (e.g., round-by-round intermediate values)
                    If SKEIN_DEBUG is not defined at compile time, these interface
                    declarations instead become "dummy" macros so that there is
                    no performance impact.


skein_debug.c       Debug i/o routines called by Skein functions.


skein.h             Function prototypes, data structures, and constant definitions
                    for Skein. The Skein API is more general than the NIST API 
                    (e.g., MAC functions). 

++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
The following files are different for the reference and optimized versions
of the code. Note that the source files in Optimized_32bit and Optimized_64bit 
directories are identical.

File Name           Description
--------------------------------------------------------------------------------
skein_port.h        Definitions that might need to be changed to port Skein to 
                    a different CPU platform (e.g., big-endian). The Skein code
                    should run on most CPU platforms, but the macros/functions here
                    may be helpful in making the code run more efficiently

skein.c             The main Skein interface functions: Init, Update, and Final, for
                    all three Skein block sizes. Additionally, the InitExt() function
                    allows for MAC and other extended functionality.

skein_block.c       The Skein block processing function, based on the Threefish block
                    cipher. This module contains the most performance-sensitive code
                    and can be replaced by the assembly modules for slight speedups
                    on some platforms. The functions here are only for internal use
                    inside "skein.c" and are not intended for external APIs.
                    
skein_iv.h          Initial values for various Skein hash functions. Note that these
                    values are NOT "magic constants", as they are computed using
                    the initial Skein "configuration" block.  These values are used 
                    only by the optimized code, in order to speed up the hash 
                    computations.

++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
The following files are included in the Additional_Implementations directory:

File Name           Description
--------------------------------------------------------------------------------
skein_test.c        The Skein test module, used to measure performance and generate
                    KAT vectors for testing. This module should be compiled together
                    with the Skein source files (i.e., from the Reference or the
                    Optimized directories) to generate an executable, skein_test.exe.
                    This program is used internally to test/validate/compare different
                    implementations (e.g., Reference, Optimized, Assembly).

skein_block_x64.asm This is the 64-bit assembly language version of skein_block.c. 
                    It may be used to replace that file in the Optimized_64bit 
                    directory to improve performance on 64-bit Intel/AMD systems. 
                    It should be assembled with ml64.exe.

skein_block_x86.asm This is the 32-bit assembly language version of skein_block.c. 
                    It may be used to replace that file in the Optimized_32bit 
                    directory to improve performance on 32-bit Intel/AMD systems. 
                    It should be assembled with ml.exe.

skein_rot_search.c  This is the program that searches for the Threefish rotation 
                    constants. It has many different command-line switches, but by
                    default it generates the constants used in the Skein paper.
                    This file is a stand-alone C file. To run it, simply re-direct
                    the output to a test file:  "skein_rot_search > srs_log.txt".
                    Note that it takes nearly 3 DAYS on a Core 2 Duo to complete
                    program execution in this case. Alternately, to generate individual
                    files, run the following command lines:
                        skein_rot_search -b256  > srs_256.txt
                        skein_rot_search -b512  > srs_512.txt
                        skein_rot_search -b1024 > srs_1024.txt

srs_256.txt         These three files contain the results of running skein_rot_search.exe
srs_512.txt         for the three different Skein block sizes. They are rather large.
srs_1024.txt        At the end of each file, the "finalists" are re-graded with different
                    number of random samples.

Atmel_AVR.c         This file was used to compile on the Atmel AVR 8-bit CPU.
                    It includes the optimized versions of skein.c and skein_block.c
                    with compile-time settings to only implement one at time.
                    This was compiled with the free AVR tool set from Atmel
                    and simulated to give the 8-bit C performance numbers.

skein_8bit_estimates.xls
                    This file is a spreadsheet used to generate the estimates for
                    code size and speed of assembly versions of Skein on the Atmel
                    8-bit CPU family. Note that this is MUCH faster than the C
                    versions, since it uses static variables, with optimized loading
                    and rotations.  No attempt is made here to minimize code size by 
                    sharing code using calls, although the code size could be shrunk 
                    significantly using calls, at some cost in performance.

skein_perf_core2.txt
                    This file contains code size and performance data running on
                    an Intel Core 2 Duo CPU under Windows Vista 64-bit, using the
                    Microsoft and other compilers and assemblers. It includes
                    results for both 32-bit and 64-bit code. 

skein_MSC_v9_perf.txt
                    This file contains a subset of the skein_perf_core2.txt file,
                    including only results from the MSVC 2008 compiler, with message
                    sizes that are powers of 10.

++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
The following files are included in the KAT_MCT directory, in addition to the 
KAT/MCT files required by NIST:

genKAT.c            NIST-supplied source file for generating KAT_MCT vectors.
                    This module should be compiled together with the Skein source 
                    files (i.e., from the Reference or the Optimized directories) 
                    to generate an executable genKAT.exe, which can generate the
                    KAT_MCT vectors.
                    [FWIW, compiling this source file under gcc gives several nasty compiler warnings!]
                    
skein_golden_kat.txt
                    The "golden" KAT file generated using "skein_test.exe -k". This 
                    file tries to cover various block sizes, message sizes, and output 
                    sizes, as well as MAC modes. It is used for testing compliance of
                    a Skein implementation, using skein_test.c

skein_golden_kat_internals.txt
                    The KAT file generated using "skein_test.exe -k -dc". It covers
                    the same test as "skein_golden_kat.txt" , but also prints out
                    intermediate (round-by-round) values. The file is very large, but
                    it is quite useful in debugging when porting Skein to a new
                    CPU platform and/or programming language.

skein_golden_kat_short.txt
                    This is a shorter version (subset) of skein_golden_kat.txt

skein_golden_kat_short_internals.txt
                    This is a shorter version (subset) of skein_golden_kat_internals.txt
