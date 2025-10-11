# FreeBSD maintainer's guide to OpenSSL

## Assumptions

These instructions assume the following:

- A git clone of FreeBSD will be available at `$GIT_ROOT/src/freebsd/main` with
  an origin named `freebsd`. Example:
  `git clone -o freebsd git@gitrepo.freebsd.org:src.git "$GIT_ROOT/src/freebsd/main"`
- The vendor trees will be stored under `$GIT_ROOT/src/freebsd/vendor/`.

## Software requirements

The following additional software must be installed from ports:

- lang/perl5
- lang/python
- net/rsync
- security/gnupg

## Warning

This is a long and complicated process, in part because OpenSSL is a large,
complex, and foundational software component in the FreeBSD distribution. A
lot of the overall process has been automated to reduce potential human error,
but some rough edges still exist. These rough edges have been highlighted in
the directions.

## Process

### Notes

The following directions use X.Y.Z to describe the major, minor, subminor
versions, respectively for the OpenSSL release. Please substitute the values as
appropriate in the directions below.

All single commands are prefixed with `%`.

### Variables

```
% OPENSSL_VER_MAJOR_MINOR=X.Y
% OPENSSL_VER_FULL=X.Y.Z
% RELEASE_TARFILE="openssl-${OPENSSL_VER_FULL}.tar.gz"
% BASE_URL="https://github.com/openssl/openssl/releases/download/openssl-${OPENSSL_VER_FULL}/${RELEASE_TARFILE}"
```

### Switch to the vendor branch

```
% cd "$GIT_ROOT/src/freebsd/main"
% git worktree add -b vendor/openssl-${OPENSSL_VER_MAJOR_MINOR} \
    ../vendor/openssl-${OPENSSL_VER_MAJOR_MINOR} \
    freebsd/vendor/openssl-${OPENSSL_VER_MAJOR_MINOR}
% cd "$GIT_ROOT/src/freebsd/vendor/openssl-${OPENSSL_VER_MAJOR_MINOR}
```

### Download the latest OpenSSL release

The following instructions demonstrate how to fetch a recent OpenSSL release
and its corresponding artifacts (release SHA256 checksum; release PGP
signature) from the [official website](https://www.openssl.org/source/).

```
% (cd .. && fetch ${BASE_URL} ${BASE_URL}.asc ${BASE_URL}.sha256)
```

### Verify the release authenticity and integrity

**NOTE**: this step requires importing the project author's PGP keys beforehand.
See the [sources webpage](https://openssl-library.org/source/) for more
details.

This step uses the PGP signature and SHA256 checksum files to verify the release
authenticity and integrity, respectively.

```
% (cd .. && sha256sum -c ${RELEASE_TARFILE}.sha256)
% (cd .. && gpg --verify ${RELEASE_TARFILE}.asc)
```

### Unpack the OpenSSL tarball to the parent directory

```
% (cd .. && tar xf ../${RELEASE_TARFILE})
```

### Update the sources in the vendor branch

**IMPORTANT**: the trailing slash in the source directory is required!

```
% rsync --exclude .git --delete -av ../openssl-${OPENSSL_VER_FULL}/ .
```

### Take care of added / deleted files

```
% git add -A
```

### Commit, tag, and push

```
% git commit -m "openssl: Vendor import of OpenSSL ${OPENSSL_VER_FULL}"
% git tag -a -m "Tag OpenSSL ${OPENSSL_VER_FULL}" vendor/openssl/${OPENSSL_VER_FULL}
```

The update and tag could instead be pushed later, along with the merge
to main, but pushing now allows others to collaborate.

#### Push branch update and tag separately

At this point the vendor branch can be pushed to the FreeBSD repo via:
```
% git push freebsd vendor/openssl-${OPENSSL_VER_MAJOR_MINOR}
% git push freebsd vendor/openssl/${OPENSSL_VER_FULL}
```

**NOTE**: the second "git push" command is used to push the tag, which is not
pushed by default.

#### Push branch update and tag simultaneously

It is also possible to push the branch and tag together, but use
`--dry-run` first to ensure that no undesired tags will be pushed:

```
% git push --dry-run --follow-tags freebsd vendor/openssl-${OPENSSL_VER_MAJOR_MINOR}
% git push --follow-tags freebsd vendor/openssl-${OPENSSL_VER_MAJOR_MINOR}
```

### Remove any existing patches and generated files.

```
% make clean
```

Please note that this step does not remove any generated manpages: this happens
in a later step.

### Merge from the vendor branch and resolve conflicts

```
% git subtree merge -P crypto/openssl vendor/openssl-${OPENSSL_VER_MAJOR_MINOR}
```

**NOTE**: Some files may have been deleted from FreeBSD's copy of OpenSSL.
If git prompts for these deleted files during the merge, choose 'd'
(leaving them deleted).

### Patch, configure, and regenerate all files

The following commands turn the crank associated with the vendor release
update:

```
% make patch
% make configure
% make all
```

This process updates all generated files, syncs the manpages with the new release,
regenerates assembly files, etc.

For now, any build-related changes, e.g., a assembly source was removed, a manpage
was added, etc, will require makefile updates.

### Diff against the vendor branch

Review the diff for any unexpected changes:

```
% git diff --diff-filter=M vendor/openssl/${OPENSSL_VER_FULL} HEAD:crypto/openssl
```

The net-result should be just the applied patches from the freebsd/ directory.

### Make build-related changes

**IMPORTANT**: manual adjustments/care needed here.

Update the appropriate makefiles to reflect changes in the vendor's
`build.info` metadata file. This is especially important if source files have
been added or removed. Keep in mind that the assembly files generated belong in
`sys/crypto/openssl`, and will therefore affect the kernel as well.

If symbols have been added or removed, update the appropriate `Version.map` to
reflect these changes. Please try to stick to the new versioning scheme in the
target OpenSSL release to improve interoperability with binaries compiled
dynamically against the ports version of OpenSSL, for instance.

Compare compilation flags, the list of files built and included, the list of
symbols generated with the corresponding port if available.

### Build, install, and test

Build and install a new version of world and the kernel with the newer release
of OpenSSL. Reboot the test host and run any appropriate tests using kyua,
`make checkworld`, etc.

### Commit and push
