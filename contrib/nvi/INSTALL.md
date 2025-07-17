# Install from source

For instructions to bring nvi2 as a part of your operating system's base system, see [Porting](https://github.com/lichray/nvi2/wiki/Porting) in the Wiki. This document is an overview of the build process that allows you to give nvi2 a try.

## Prerequisites

- CMake >= 3.17;
- Ninja build system;
- libiconv (for `USE_ICONV`);
- libncursesw (for `USE_WIDECHAR`);

Anything required by a minimal nvi, notably:

- Berkeley DB1 in libc;
- /var/tmp/vi.recover/ with mode 41777.

## Building

Nvi2 uses CMake build system generator. By specifying "Ninja Multi-Config" as the build system to generate, you can compile the project in both Debug and Release modes without re-running CMake. Under the project root directory, run

```sh
cmake -G "Ninja Multi-Config" -B build
```

Now `build` becomes your build directory to hold the artifacts. To build nvi2 in Debug mode, run

```sh
ninja -C build
```

Upon finishing, the nvi2 executable will be available as `build/Debug/nvi`. To launch it in `ex` mode, you can create a symlink

```sh
ln -s nvi build/Debug/ex
```

and run `./build/Debug/ex` rather than `./build/Debug/nvi`.

To build nvi2 in Release mode, use the following command instead:

```sh
ninja -C build -f build-Release.ninja
```

Upon finishing, you will be able to edit files with `./build/Release/nvi`.

To change configure-time options, such as disabling wide character support, use `ccmake build`.
