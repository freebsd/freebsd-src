# generic-ebpf
Generic eBPF runtime. It (currently) consists of three components

1. ebpf: Portable interpreter, JIT compiler, and ebpf subsystems (e.g. map) library, works in both of userspace and kernel.
2. ebpf_dev: Character device for loading ebpf program or other related objects (e.g. map) into kernel. Alternative of Linux bpf(2).
3. libgbpf: A library which implements abstruction layer for interacting with various eBPF systems and eBPF ELF parser.
Currently supports ebpf_dev and Linux's native eBPF (experimental) as backends.

Current support status

|               |ebpf                           |ebpf_dev           |
|:--------------|:------------------------------|:------------------|
|FreeBSD Kernel |Yes                            |Yes                |
|FreeBSD User   |Yes                            |-                  |
|Linux Kernel   |Yes                            |Yes                |
|Linux User     |Yes                            |-                  |
|MacOSX User    |Yes                            |-                  |

# Build

```
$ make
```

After compilation, you will see at least one of below
- ebpf.ko: Kernel module for ebpf library
- ebpf-dev.ko: Kernel module for ebpf character device
- libebpf.so: User space library for ebpf
- libgbpf.a: Library for interacting with various eBPF systems

Please load or link them.

## Running tests

### Tests for user space library
```
// Install Python packages
$ pip install -r requirements.txt

// After make
$ make check
```

### Tests for kernel
```
// After make
# make load
# make check-kern
```

## Example Applications

### [VALE-BPF](https://github.com/YutaroHayakawa/vale-bpf)

Enhansing eBPF programmability to [VALE](http://info.iet.unipi.it/~luigi/papers/20121026-vale.pdf)
(a.k.a. [mSwitch](https://pdfs.semanticscholar.org/ec44/8ceb3e05b9222113366dace9fdd2a62322de.pdf))
 a very fast and modular software switch.
 
## Notes
Our ebpf interpreter and jit codes (and its tests) are based on [ubpf](https://github.com/iovisor/ubpf)

## Dependencies

- libelf (for libgbpf)
- Concurrency Kit (for userspace targets)
- Google Test (for testing)

Concurrency Kit and Google Test are contained to this repository. You don't have to
install them by yourself.
