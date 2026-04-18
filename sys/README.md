FreeBSD Kernel Source:
----------------------

This directory contains the source files and build glue that make up the FreeBSD
kernel and its modules, including both original and contributed software.

Kernel configuration files are located in the `conf/` subdirectory of each
architecture. `GENERIC` is the configuration used in release builds. `NOTES`
contains documentation of all possible entries. `LINT` is a compile-only
configuration used to maximize build coverage and detect regressions.

Documentation:
--------------

Source code documentation is maintained in a set of man pages, under section 9.
These pages are located in [`share/man/man9`](../share/man/man9), from the
top-level of the src tree. Consult [`intro(9)`](https://man.freebsd.org/intro/9)
for an overview of existing pages.

Some additional high-level documentation of the kernel is maintained in the
[Architecture Handbook](https://docs.freebsd.org/en/books/arch-handbook/).

Source Roadmap:
---------------
| Directory | Description |
| --------- | ----------- |
| amd64 | AMD64 (64-bit x86) architecture support |
| arm | 32-bit ARM architecture support |
| arm64 | 64-bit ARM (AArch64) architecture support |
| bsm | Basic Security Module headers - `audit(4)` and `bsm(3)` |
| cam | Common Access Method storage subsystem - `cam(4)` and `ctl(4)` |
| cddl | CDDL-licensed optional sources such as DTrace |
| compat | Linux compatibility layer, FreeBSD 32-bit compatibility |
| conf | kernel build glue |
| contrib | 3rd-party imported software such as OpenZFS |
| crypto | crypto drivers |
| ddb | interactive kernel debugger - `ddb(4)` |
| dev | device drivers and other arch independent code |
| dts | FreeBSD-specific device tree sources |
| fs | most filesystems, excluding UFS, NFS, and ZFS |
| gdb | kernel remote GDB stub - `gdb(4)` |
| geom | GEOM framework - `geom(4)` |
| gnu | GPL-licensed sources |
| i386 | i386 (32-bit x86) architecture support |
| isa | PC ISA bus implementation |
| kern | main part of the kernel |
| kgssapi | kernel-space GSSAPI implementation |
| libkern | libc-like and other support functions for kernel use |
| modules | kernel module infrastructure |
| net | core networking code |
| net80211 | wireless networking (IEEE 802.11) - `net80211(4)` |
| netgraph | graph-based networking subsystem - `netgraph(4)` |
| netinet | IPv4 protocol implementation - `inet(4)` |
| netinet6 | IPv6 protocol implementation - `inet6(4)` |
| netipsec | IPsec protocol implementation - `ipsec(4)` |
| netlink | kernel network configuration protocol - `netlink(4)` |
| netpfil | packet filters - `ipfw(4)`, `pf(4)`, and `ipfilter(4)` |
| netsmb | Server Message Block protocol implementation |
| nfs | common code and headers for Network File System |
| nfsclient | NFS client implementation for mounting and stats |
| nfsserver | NFS server implementation for exporting local filesystems to network |
| nlm | Network Lock Management protocol implementation |
| ofed | OpenFabrics Enterprise Distribution implementation |
| opencrypto | OpenCrypto framework - `crypto(7)` |
| powerpc | PowerPC/POWER (32 and 64-bit) architecture support |
| riscv | 64-bit RISC-V architecture support |
| rpc | Open Network Computing Remote Procedure Call implementation - `rpc(3)` |
| security | security facilities - `audit(4)` and `mac(4)` |
| sys | kernel headers |
| teken | terminal emulation interface implementation - `teken(3)` |
| tests | kernel unit tests |
| tools | kernel build scripts and utilities |
| ufs | Unix File System - `ffs(4)` |
| vm | virtual memory system |
| x86 | code shared by AMD64 and i386 architectures |
| xdr | External Data Representation implementation - `xdr(3)` |
| xen | Xen hypervisor support - `xen(4)` |
