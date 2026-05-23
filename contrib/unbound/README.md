# Unbound

[![Github Build Status](https://github.com/NLnetLabs/unbound/actions/workflows/ci.yml/badge.svg?branch=master)](https://github.com/NLnetLabs/unbound/actions)
[![Packaging status](https://repology.org/badge/tiny-repos/unbound.svg)](https://repology.org/project/unbound/versions)
[![Fuzzing Status](https://oss-fuzz-build-logs.storage.googleapis.com/badges/unbound.svg)](https://bugs.chromium.org/p/oss-fuzz/issues/list?sort=-opened&can=1&q=proj:unbound)
[![Documentation Status](https://readthedocs.org/projects/unbound/badge/?version=latest)](https://unbound.readthedocs.io/en/latest/?badge=latest)
[![Mastodon Follow](https://img.shields.io/mastodon/follow/114692612288811644?domain=social.nlnetlabs.nl&style=social)](https://social.nlnetlabs.nl/@nlnetlabs)

Unbound is a validating, recursive, caching DNS resolver. It is designed to be
fast and lean and incorporates modern features based on open standards. If you
have any feedback, we would love to hear from you. Don’t hesitate to
[create an issue on Github](https://github.com/NLnetLabs/unbound/issues/new)
or post a message on the [Unbound mailing list](https://lists.nlnetlabs.nl/mailman/listinfo/unbound-users).
You can learn more about Unbound by reading our
[documentation](https://unbound.docs.nlnetlabs.nl/).

## Compiling

Make sure you have the C toolchain, OpenSSL and its include files, and libexpat
installed.
If building from the repository source you also need flex and bison installed.
Unbound can be compiled and installed using:

```
./configure && make && make install
```

You can use libevent if you want. libevent is useful when using many (e.g.,
10000) outgoing ports.
Use the `--with-libevent` configure option to compile Unbound with libevent
support.

If not, the default builtin alternative opens max 256 ports at the same time
and is equally capable and a little faster.


## Unbound configuration

All of Unbound's configuration options are described in the `unbound.conf(5)`
man page, which will be installed and is also available on the Unbound
[documentation page](https://unbound.docs.nlnetlabs.nl/en/latest/manpages/unbound.conf.html)
for the latest version.

An example configuration file, with minimal documentation, is located in
[doc/example.conf](https://github.com/NLnetLabs/unbound/blob/master/doc/example.conf.in).
