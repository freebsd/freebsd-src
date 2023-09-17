Welcome to the Kyua project!
============================

Kyua is a **testing framework** for infrastructure software, originally
designed to equip BSD-based operating systems with a test suite.  This
means that Kyua is lightweight and simple, and that Kyua integrates well
with various build systems and continuous integration frameworks.

Kyua features an **expressive test suite definition language**, a **safe
runtime engine** for test suites and a **powerful report generation
engine**.

Kyua is for **both developers *and* users**, from the developer applying a
simple fix to a library to the system administrator deploying a new release
on a production machine.

Kyua is **able to execute test programs written with a plethora of testing
libraries and languages**.  The library of choice is
[ATF](https://github.com/jmmv/atf/), for which Kyua was originally
designed, but simple, framework-less test programs and TAP-compliant test
programs can also be executed through Kyua.

Kyua is licensed under a **[liberal BSD 3-clause license](LICENSE)**.
This is not an official Google product.

[Read more about Kyua in the About wiki page.](../../wiki/About)


Download
--------

The latest version of Kyua is 0.13 and was released on August 26th, 2016.

Download: [kyua-0.13](../../releases/tag/kyua-0.13).

See the [release notes](NEWS.md) for information about the changes in this
and all previous releases.


Installation
------------

You are encouraged to install binary packages for your operating system
wherever available:

* Fedora 20 and above: install the `kyua-cli` package with `yum install
  kyua-cli`.

* FreeBSD 10.0 and above: install the `kyua` package with `pkg install kyua`.

* NetBSD with pkgsrc: install the `pkgsrc/devel/kyua` package.

* OpenBSD with packages: install the `kyua` package with `pkg_add kyua`.

* OS X (with Homebrew): install the `kyua` package with `brew install kyua`.

Should you want to build and install Kyua from the source tree provided
here, follow the instructions in the
[INSTALL.md file](INSTALL.md).

You should also install the ATF libraries to assist in the development of
test programs.  To that end, see the
[ATF project page](https://github.com/jmmv/atf/).


Contributing
------------

Want to contribute?  Great!  But please first read the guidelines provided
in [CONTRIBUTING.md](CONTRIBUTING.md).

If you are curious about who made this project possible, you can check out
the [list of copyright holders](AUTHORS) and the [list of
individuals](CONTRIBUTORS).


Support
-------

Please use the [kyua-discuss mailing
list](https://groups.google.com/forum/#!forum/kyua-discuss) for any support
inquiries.

*Homepage:* https://github.com/jmmv/kyua/
