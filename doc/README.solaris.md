# Compiling libpcap on Solaris and related OSes

* Autoconf works everywhere.
* Neither Solaris lex nor Solaris yacc are suitable.
* Neither illumos lex nor illumos yacc are suitable.
* Solaris m4 and illumos m4 are suitable.

## OmniOS r151042/AMD64

* flex 2.6.4 and GNU Bison 3.8.2 work.
* CMake 3.23.1 works.
* GCC 11.2.0 and Clang 14.0.3 work.

## OpenIndiana 2021.04/AMD64

* flex 2.6.4 and GNU Bison 3.7.6 work.
* CMake 3.21.1 works.
* GCC 7.5.0 and GCC 10.3.0 work, Clang 9.0.1 works.

For reference, the tests were done using a system installed from
`OI-hipster-text-20210430.iso` plus the following packages:
```shell
xargs -L1 pkg install <<ENDOFTEXT
developer/build/autoconf
developer/parser/bison
developer/lexer/flex
developer/build/cmake
developer/gcc-10
developer/clang-90
ENDOFTEXT
```

## Solaris 11/AMD64

* flex 2.6.4 and GNU Bison 3.7.3 work.
* CMake 3.21.0 works.
* Clang 11.0 works, GCC 11.2 works.

For reference, the tests were done using Oracle Solaris 11.4.42.111.0.

## Solaris 11/SPARC

* flex 2.6.4 and GNU Bison 3.7.1 work.
* CMake 3.14.3 works.
* Sun C 5.13, Sun C 5.14 and Sun C 5.15 work; GCC 5.5.0 and GCC 7.3.0 work.

## Solaris 10/SPARC

* libpcap build fails with rpcapd enabled.
* flex 2.6.4 and GNU Bison 3.7.1 work.
* CMake 3.14.3 works.
* Sun C 5.13 works, GCC 5.5.0 works.

## Solaris 9/SPARC

* flex 2.5.35 and GNU Bison 3.0.2 work.
* CMake 2.8.9 does not work.
* Neither Sun C 5.8 nor Sun C 5.9 work, GCC 4.6.4 works.
