# UOS Mobile OS - Root Filesystem

This directory contains the root filesystem layout that gets installed to the target device or disk image.

## Directory Structure

```
rootfs/
├── bin/                    # Essential user binaries (symlinks to /rescue)
├── lib/                    # Shared libraries
├── libexec/                # System daemons and utilities
├── usr/
│   ├── bin/                # User utilities
│   ├── lib/                # Libraries
│   └── share/              # Architecture-independent data
├── etc/                    # Configuration files
│   ├── rc.conf             # System configuration
│   ├── fstab               # Filesystem table
│   ├── passwd              # User accounts
│   ├── group               # Group definitions
│   ├── hostname            # System hostname
│   ├── resolv.conf         # DNS resolver config
│   ├── pkgctl/             # Package manager config
│   │   ├── repos.conf      # Repository list
│   │   └── pkglist.txt     # Installed packages
│   └── rc.d/               # Startup scripts
├── var/
│   ├── log/                # Log files
│   ├── db/                 # Local databases
│   ├── run/                # Runtime data (tmpfs)
│   └── tmp/                # Temporary files (tmpfs)
└── tmp/                    # Temporary directory
```

## Installation

```bash
# Copy rootfs to target
cp -r rootfs/* /mnt/target/

# Or via make
make -C mobile install-rootfs DESTDIR=/mnt/target
```
