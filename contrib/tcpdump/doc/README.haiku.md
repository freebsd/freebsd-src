# Compiling tcpdump on Haiku

## 64-bit x86 R1/beta4

* Both system and local libpcap are suitable.
* Autoconf 2.71 works.
* CMake 3.24.2 works.
* GCC 11.2.0 works.
* Clang 12.0.1 works with the latest llvm12_clang-12.0.1-5 version.

The following command will install respective non-default packages:
```
pkgman install libpcap_devel cmake llvm12_clang
```

For reference, the tests were done using a system installed from
`haiku-r1beta4-x86_64-anyboot.iso`.

## 32-bit x86 R1/beta4

* Both system and local libpcap are suitable.
* Autoconf 2.71 works.
* CMake 3.24.2 works.
* GCC 11.2.0 works.
* Clang does not work.

The following command will install respective non-default packages:
```
pkgman install libpcap_x86_devel cmake_x86
```

For reference, the tests were done using a system installed from
`haiku-r1beta4-x86_gcc2h-anyboot.iso`.
