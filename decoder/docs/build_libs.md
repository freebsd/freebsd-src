Building and using the Library   {#build_lib}
==============================

@brief How to build the library and test programs and include the library in an application

Platform Support
----------------

The current makefiles and build projects support building the library on Linux and Windows, 
x86 or x64 hosts.

Support is expected for ARM linux and baremetal, AArch32 and AArch64 platforms.


Building the Library
--------------------

The library and test programs are built from the library `./build/<platform>` directory.

See [`./docs/test_progs.md`](@ref test_progs) for further information on use of the test 
programs.

### Linux x86/x64 ###

Go to the `./build/linux/` and run `make` in that directory.

Options to pass to the makefile are:-
- `LINUX64=1` : build the 64 bit version of the library
- `DEBUG=1`   : build the debug version of the library.

Libraries are delivered to the `./lib/linux<bitsize>/<dbg\rel>` directories.
e.g. `./lib/linux64/rel` will contain the linux 64 bit release libraries.

The following libraries are built:-
- `libcstraced.so`       : shared library containing the main C++ based decoder library
- `libcstraced_c_api.so` : shared library containing the C-API wrapper library. Dependent on `libcstraced.so`
- `libcstraced.a`        : static library containing the main C++ based decoder library.
- `libcstraced_c_api.a`  : static library containing the C-API wrapper library.

Test programs are delivered to the `./tests/bin/linux<bitsize>/<dgb\rel>` directories.

The test programs are built to used the .so versions of the libraries. 
-  `trc_pkt_lister`         - dependent on `libcstraced.so`.
-  `simple_pkt_print_c_api` - dependent on `libcstraced_c_api.so` & hence `libcstraced.so`.

The test program build for `trc_pkt_lister` also builds an auxiliary library used by this program for test purposes only.
This is the `libsnapshot_parser.a` library, delivered to the `./tests/lib/linux<bitsize>/<dgb\rel>` directories.

### Windows ###

Use the `.\build\win\ref_trace_decode_lib\ref_trace_decode_lib.sln` file to load a solution
which contains all library and test build projects.

Libraries are delivered to the `./lib/win<bitsize>/<dbg\rel>` directories.
e.g. `./lib/win64/rel` will contain the windows 64 bit release libraries.

The solution contains four configurations:-
- *Debug* : builds debug versions of static C++ main library and C-API libraries, test programs linked to the static library.
- *Debug-dll* : builds debug versions of static main library and C-API DLL. C-API statically linked to the main library. 
C-API test built as `simple_pkt_print_c_api-dl.exe` and linked against the DLL version of the C-API library.
- *Release* : builds release static library versions, test programs linked to static libraries.
- *Release-dll* : builds release C-API DLL, static main library.

_Note_: Currently there is no Windows DLL version of the main C++ library. This may follow once
the project is nearer completion with further decode protocols, and the classes requiring export are established..

Libraries built are:-
- `libcstraced.lib` : static main C++ decoder library.
- `cstraced_c_api.dll` : C-API DLL library. Statically linked against `libcstraced.lib` at .DLL build time.
- `libcstraced_c_api.lib` : C-API static library. 

There is also a project file to build an auxiliary library used `trc_pkt_lister` for test purposes only.
This is the `snapshot_parser_lib.lib` library, delivered to the `./tests/lib/win<bitsize>/<dgb\rel>` directories.


### Additional Build Options ###

__Library Virtual Address Size__

The ocsd_if_types.h file includes a #define that controls the size of the virtual addresses 
used within the library. By default this is a 64 bit `uint64_t` value.

When building for ARM architectures that have only a 32 bit Virtual Address, and building on 
32 bit ARM architectures it may be desirable to build a library that uses a v-addr size of 
32 bits. Define `USE_32BIT_V_ADDR` to enable this option


Including the Library in an Application
---------------------------------------

The user source code includes a header according to the API to be used:-

- Main C++ decoder library - include `opencsd.h`. Link to C++ library. 
- C-API library - include `opencsd_c_api.h`. Link to C-API library.

### Linux build ###

By default linux builds will link against the .so versions of the library. Using the C-API library will also
introduce a dependency on the main C++ decoder .so. Ensure that the library paths and link commands are part of the 
application makefile.

To use the static versions use appropriate linker options.

### Windows build ###

To link against the C-API DLL, include the .DLL name as a dependency in the application project options.

To link against the C-API static library, include the library name in the dependency list, and define the macro 
`OCSD_USE_STATIC_C_API` in the preprocessor definitions. This ensures that the correct static bindings are declared in
the header file. Also link against the main C++ library.

To link against the main C++ library include the library name in the dependency list.

