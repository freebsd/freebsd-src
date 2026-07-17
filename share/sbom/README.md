# FreeBSD Software Bill Of Material (SBOM)

FreeBSD Software Bill Of Material is a document that contains operating system assets listed and evaluated. It tries to answer the question, "What is inside my system?". It does not tell what these components or applications do but rather what FreeBSD contains in general. The first effort is having all components for base packages installed from release ISO image. Meaning what a user has when FreeBSD is installed without customization. From now on, Software Bill Of Material is just marked as the acronym SBOM.

## Why does FreeBSD need SBOM?

We live in a very different world than when FreeBSD was created, and it no bet to say the world will changed rapidly in next years and degades. Supply chain attacks have become more common than an oddity. Regulations have also become stricter after digital services became the backbone of societies. An example of stricter regulation is the EU Cyber Resilience Act (CRA), which mandates SBOM when you have any commercial service with network-accessible parts. The United States has also started to enforce stricter laws for internet-based services and machines that connects to Internet.

As FreeBSD serves as a silent enabler for internet services, routing multi billions of packets per second from and to the Internet, it is crucial to provide an SBOM that downstream users and base users can rely on and extend SBOM in the future.

## What SBOM documents?

What is documented in SBOM? Currently, the FreeBSD SBOM aims to answer these questions:

1. What applications are inside my system?
2. Who holds copyrights for the application, and what is its license?
3. Is the application or library developed by the FreeBSD Project or a third party dependency?
4. When were binaries built and by whom?

As this is just the first wave, it will improve in the time being buut currently, the SBOM contains full information about `bin`, `sbin`, `usr/bin`, and `usr/sbin` directories when a non-customized installation has been made from an ISO image. It only includes CLI tools and does not contain graphical environments such as X.org or Wayland. As stated earlier, the first step is to document all applications that live in the FreeBSD `src` Git repository.

## How Information Is Gathered

Information from FreeBSD src has been gathered mainly using `scancode-toolkit`, which scans source files and provides predictions about licenses and copyrights. These predictions have been semimanually reviewed, but as most are accurate, human interaction was only needed for a small percentage of corner cases. Since the tool produces unnecessary information, Lua tooling has been developed to format the JSON output into a more usable version for FreeBSD SBOM creation.

## Licenses

FreeBSD src primarily three licenses dominates the space and then there is fine herd of other liberal licenses. In SBOM these licenses are expressed as SPDX license indetifiers and expressions. Why use SPDX license identifiers instead of FreeBSD ports license indentifiers? There’s no simple answer! SPDX license identifiers have become a popular way to represent licenses. They provide a common, machine-readable format for expressing licenses. 

## Copyrights

Source file license copyrights are also gathered and processed. Currently pkgconf SBOM tools still lacks correct copyright text handling which needs to implemented but as they are crucial metadata they should be added in near future.

## Used tools
Integration with the FreeBSD build environment is currently under review. It includes several metadata `.pc` files along with integration into the FreeBSD build system that automatically exports SPDX 2.2 and SPDX Lite 3.0.1 files after building binaries. This happens automaticly when a `.pc` file exists for applications name.

Other tools are:

1. **scancode-toolkit**
2. **FreeBSD Lua tools for SBOM** – These are mainly new tools written as part of several FreeBSD Foundation-sponsored projects. They are somewhat rough but tailored for processing the main SBOM files from scancode-toolkit output and other required resources. They are heavily a work in progress (WIP) at the moment.
3. **pkgconf** – A liberally licensed application primarily used for linking and retrieving metadata for C applications. It is similar to pkg-config, and the file format they use is the same. Pkgconf extends pkg-config tags with additional ones needed to create SBOMs. Tools for creating SBOMs include `bomtool`, which outputs SPDX standard 2.2, and `spdxtool`, which outputs SPDX Lite 3.0.1 in JSON-LD format.

### FreeBSD Build System Integration
For testing build system integration it's currently only available outside the FreeBSD src tree at [https://github.com/khorben/freebsd-src/tree/khorben/sbom-bomtool](https://github.com/khorben/freebsd-src/tree/khorben/sbom-bomtool). SBOM files can be only build from that particular branch. FreeBSD src contains the pkgconf tool anf it can be found in `contrib/pkgconf`. The review for integration is available at [https://reviews.freebsd.org/D56474](https://reviews.freebsd.org/D56474).

When building from the review branch `khorben/sbom-bomtool`, creating SBOM files is enabled by default. You can build SBOM files with:

```sh
make buildworld
```

The produced files are located in `/usr/obj/…/worldstage/usr/share/sbom/` and have a `.spdx` extension.

### Scancode-toolkit
According to its documentation, ScanCode Toolkit is:
> A set of code scanning tools that detect the origin (copyrights), license, and vulnerabilities of code, packages, and dependencies in a codebase.

It is primarily a Python-based tool that scans source files and predicts licenses and copyrights based on its rules. While there are other similar tools, Scancode-Toolkit has been the most reliable with fewer false positives. This may change in the future, but for now, it remains the chosen tool for this job.

#### Example
The currently used scancode CLI command is (example scans `bin/` directory and outputs to `output.json` file):

```sh
scancode --info --copyright --license --license-text --ignore "test*" --ignore "*.1" --ignore "*.2" --ignore "*.3" --ignore "*.4" --ignore "*.5" --ignore "*.6" --ignore "*.7" --ignore "*.8" --ignore "*.sh" --ignore "*.pc" --ignore "*.json" --ignore "Makefile*" --include "*.c" --include "*.h" --include "*.cc" --include "*.hh" --strip-root --license-score 98 --json-pp output.json bin/
```

### FreeBSD Lua Tools for SBOM
There are several Lua-based tools used to update and manipulate package data. They will be documented here once they are more suited for general use.

### pkgconf
Pkgconf is similar to `pkg-config` but has a more liberal license. It is currently included in the FreeBSD src tree and will be part of the next major FreeBSD release. Pkgconf describes itself on its development pages as:
> A program that helps configure compiler and linker flags for development libraries. It is a superset of the functionality provided by pkg-config from 
freedesktop.org but does not provide bug-compatibility with the original pkg-config.

While pkgconf was originally designed for C/C++ compiling and linking, there are also tools to produce SPDX 2.2 and Lite 3.0.1 SBOMs from additional tags like `Copyright:` and `License:`.

#### pkgconf bomtool
Pkgconf’s `bomtool` is used to create SPDX 2.2 SBOMs in SPDX format.

#### pkgconf spdxtool
Pkgconf’s `spdxtool` is used to create SPDX Lite 3.0.1 SBOMs in RDF JSON-LD format. Spdxtool is not yet released in any pkgconf release, but it will officially debut with pkgconf 3.0.

## What Is in the pkgconfig Directory
SBOM production depends on metadata files located in `share/sbom/pkgconfig`. Files in this directory are in `.pc` -format and contain tags like `Name:` and `Version:`, which pkgconf parses.

### .pc File Format
Normally, these files include:
* **`Name`**: Provides a human-readable name for the package.
* **`Version`**: The version of the package.
* **`Requires`, `Conflicts`**: These specify dependencies and are used to produce a dependency tree in FreeBSD SBOMs.
* **`Cflags`, `Libs`**: Used for linking and compiling; they are currently unused in FreeBSD SBOM creation.
* **`Copyright`**: The copyright information for the application.
* **`License`**: The application’s license(s), expressed in SPDX License format (e.g., `BSD-4-Clause AND BSD-3-Clause`). Licenses are gathered using `scancode-toolkit` and SPDX-License-Identifier tags if available in source or header files.
* **`Source`**: The location where the package is built.
* **`URL`**: The application’s homepage. If no alternative exists, man pages are used, especially for non-third-party applications.

For example, the FreeBSD `cat` application’s `.pc` file looks like this:

```
# Copyright (c) 2026 The FreeBSD Foundation
#
# SPDX-License-Identifier: BSD-2-Clause AND LicenseRef-FreeBSD-SBOM
#
Copyright: 1989, 1993 The Regents of the University of California
Description: Concatenate and print files
License: BSD-3-Clause
Name: cat
Requires: csu, libc, libcapsicum, libcasper, libcasper.services.cap_fileargs, libcasper.services.cap_net, libcompiler_rt
Source: https://cgit.freebsd.org/src/tree/bin/cat
URL: https://man.freebsd.org/cgi/man.cgi?query=cat(1)
Version: ${FREEBSD_RELEASE}
```

# What is in LICENSES-directory
Liceses directory will be populated FreeBSD application license texts. There is several versions of text for example one of most popular license BSD-2-Clause.
