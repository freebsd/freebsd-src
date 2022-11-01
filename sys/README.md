FreeBSD Kernel Source:
----------------------

This directory contains the source files and build glue that make up the FreeBSD
kernel and its modules, including both original and contributed software.

Kernel configuration files are located in the `conf/` subdirectory of each
architecture. `GENERIC` is the configuration used in release builds. `NOTES`
contains documentation of all possible entries. `LINT` is a compile-only
configuration used to maximize build coverage and detect regressions.

Source Roadmap:
---------------
| Directory | Description |
| --------- | ----------- |
| amd64 | AMD64 (64-bit x86) architecture support |
| arm | 32-bit ARM architecture support |
| arm64 | 64-bit ARM (AArch64) architecture support |
| cam | Common Access Method storage subsystem - `cam(4)` and `ctl(4)` |
| cddl | CDDL-licensed optional sources such as DTrace |
| conf | kernel build glue |
| compat | Linux compatibility layer, FreeBSD 32-bit compatibility |
| contrib | 3rd-party imported software such as OpenZFS |
| crypto | crypto drivers |
| ddb | interactive kernel debugger - `ddb(4)` |
| fs | most filesystems, excluding UFS, NFS, and ZFS |
| dev | device drivers |
| gdb | kernel remote GDB stub - `gdb(4)` |
| geom | GEOM framework - `geom(4)` |
| i386 | i386 (32-bit x86) architecture support |
| kern | main part of the kernel |
| libkern | libc-like and other support functions for kernel use |
| modules | kernel module infrastructure |
| net | core networking code |
| net80211 | wireless networking (IEEE 802.11) - `net80211(4)` |
| netgraph | graph-based networking subsystem - `netgraph(4)` |
| netinet | IPv4 protocol implementation - `inet(4)` |
| netinet6 | IPv6 protocol implementation - `inet6(4)` |
| netipsec | IPsec protocol implementation - `ipsec(4)` |
| netpfil | packet filters - `ipfw(4)`, `pf(4)`, and `ipfilter(4)` |
| opencrypto | OpenCrypto framework - `crypto(7)` |
| powerpc | PowerPC/POWER (32 and 64-bit) architecture support |
| riscv | 64-bit RISC-V architecture support |
| security | security facilities - `audit(4)` and `mac(4)` |
| sys | kernel headers |
| tests | kernel unit tests |
| ufs | Unix File System - `ffs(7)` |
| vm | virtual memory system |
| x86 | code shared by AMD64 and i386 architectures |
