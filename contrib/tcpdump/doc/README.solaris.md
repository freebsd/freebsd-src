# Compiling tcpdump on Solaris and related OSes

* Autoconf works everywhere.

## OmniOS r151042/AMD64

* Both system and local libpcap are suitable.
* CMake 3.23.1 works.
* GCC 11.2.0 and Clang 14.0.3 work.

## OpenIndiana 2021.04/AMD64

* Both system and local libpcap are suitable.
* CMake 3.21.1 works.
* GCC 7.5.0 and GCC 10.3.0 work, Clang 9.0.1 works.

For reference, the tests were done using a system installed from
`OI-hipster-text-20210430.iso` plus the following packages:
```shell
xargs -L1 pkg install <<ENDOFTEXT
developer/build/autoconf
developer/build/cmake
developer/gcc-10
developer/clang-90
ENDOFTEXT
```

## Oracle Solaris 11.4.42/AMD64

* Both system and local libpcap are suitable.
* GCC 11.2 and Clang 11.0 work.

For reference, the tests were done on a VM booted from `sol-11_4-vbox.ova`
and updated to 11.4.42.111.0 plus the following packages:
```shell
xargs -L1 pkg install <<ENDOFTEXT
developer/build/autoconf
developer/gcc
developer/llvm/clang
ENDOFTEXT
```

## Solaris 9

This version of this OS is not supported because the snprintf(3) implementation
in its libc is not suitable.
