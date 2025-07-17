# pam-krb5

[![Build
status](https://github.com/rra/pam-krb5/workflows/build/badge.svg)](https://github.com/rra/pam-krb5/actions)
[![Debian
package](https://img.shields.io/debian/v/libpam-krb5/unstable)](https://tracker.debian.org/pkg/libpam-krb5)

Copyright 2005-2010, 2014-2015, 2017, 2020-2021 Russ Allbery
<eagle@eyrie.org>.  Copyright 2009-2011 The Board of Trustees of the
Leland Stanford Junior University.  Copyright 2005 Andres Salomon
<dilinger@debian.org>.  Copyright 1999-2000 Frank Cusack
<fcusack@fcusack.com>.  This software is distributed under a BSD-style
license.  Please see the section [License](#license) below for more
information.

## Blurb

pam-krb5 is a Kerberos PAM module for either MIT Kerberos or Heimdal.  It
supports ticket refreshing by screen savers, configurable authorization
handling, authentication of non-local accounts for network services,
password changing, and password expiration, as well as all the standard
expected PAM features.  It works correctly with OpenSSH, even with
ChallengeResponseAuthentication and PrivilegeSeparation enabled, and
supports extensive configuration either by PAM options or in krb5.conf or
both.  PKINIT is supported with recent versions of both MIT Kerberos and
Heimdal and FAST is supported with recent MIT Kerberos.

## Description

pam-krb5 provides a Kerberos PAM module that supports authentication, user
ticket cache handling, simple authorization (via .k5login or checking
Kerberos principals against local usernames), and password changing.  It
can be configured through either options in the PAM configuration itself
or through entries in the system krb5.conf file, and it tries to work
around PAM implementation flaws in commonly-used PAM-enabled applications
such as OpenSSH and xdm.  It supports both PKINIT and FAST to the extent
that the underlying Kerberos libraries support these features.

This is not the Kerberos PAM module maintained on Sourceforge and used on
Red Hat systems.  It is an independent implementation that, if it ever
shared any common code, diverged long ago.  It supports some features that
the Sourceforge module does not (particularly around authorization), and
does not support some options (particularly ones not directly related to
Kerberos) that it does.  This module will never support Kerberos v4 or
AFS.  For an AFS session module that works with this module (or any other
Kerberos PAM module), see
[pam-afs-session](https://www.eyrie.org/~eagle/software/pam-afs-session/).

If there are other options besides AFS and Kerberos v4 support from the
Sourceforge PAM module that you're missing in this module, please let me
know.

## Requirements

Either MIT Kerberos (or Kerberos implementations based on it) or Heimdal
are supported.  MIT Keberos 1.3 or later may be required; this module has
not been tested with earlier versions.

For PKINIT support, Heimdal 0.8rc1 or later or MIT Kerberos 1.6.3 or later
are required.  Earlier MIT Kerberos 1.6 releases have a bug in their
handling of PKINIT options.  MIT Kerberos 1.12 or later is required to use
the use_pkinit PAM option.

For FAST (Flexible Authentication Secure Tunneling) support, MIT Kerberos
1.7 or higher is required.  For anonymous FAST support, anonymous
authentication (generally anonymous PKINIT) support is required in both
the Kerberos libraries and in the local KDC.

This module should work on Linux and build with gcc or clang.  It may
still work on Solaris and build with the Sun C compiler, but I have only
tested it on Linux recently.  There is beta-quality support for the AIX
NAS Kerberos implementation that has not been tested in years.  Other PAM
implementations will probably require some porting, although untested
build system support is present for FreeBSD, Mac OS X, and HP-UX.  I
personally can only test on Linux and rely on others to report problems on
other operating systems.

Old versions of OpenSSH are known to call `pam_authenticate` followed by
`pam_setcred(PAM_REINITIALIZE_CRED)` without first calling
`pam_open_session`, thereby requesting that an existing ticket cache be
renewed (similar to what a screensaver would want) rather than requesting
a new ticket cache be created.  Since this behavior is indistinguishable
at the PAM level from a screensaver, pam-krb5 when used with these old
versions of OpenSSH will refresh the ticket cache of the OpenSSH daemon
rather than setting up a new ticket cache for the user.  The resulting
ticket cache will have the correct permissions (this is not a security
concern), but will not be named correctly or referenced in the user's
environment and will be overwritten by the next user login.  The best
solution to this problem is to upgrade OpenSSH.  I'm not sure exactly when
this problem was fixed, but at the very least OpenSSH 4.3 and later do not
exhibit it.

To bootstrap from a Git checkout, or if you change the Automake files and
need to regenerate Makefile.in, you will need Automake 1.11 or later.  For
bootstrap or if you change configure.ac or any of the m4 files it includes
and need to regenerate configure or config.h.in, you will need Autoconf
2.64 or later.  Perl is also required to generate manual pages from a
fresh Git checkout.

## Building and Installation

You can build and install pam-krb5 with the standard commands:

```
    ./configure
    make
    make install
```

If you are building from a Git clone, first run `./bootstrap` in the
source directory to generate the build files.  `make install` will
probably have to be done as root.  Building outside of the source
directory is also supported, if you wish, by creating an empty directory
and then running configure with the correct relative path.

The module will be installed in `/usr/local/lib/security` by default, but
expect to have to override this using `--libdir`.  The correct
installation path for PAM modules varies considerably between systems.
The module will always be installed in a subdirectory named `security`
under the specified value of `--libdir`.  On Red Hat Linux, for example,
`--libdir=/usr/lib64` is appropriate to install the module into the system
PAM directory.  On Debian's amd64 architecture,
`--libdir=/usr/lib/x86_64-linux-gnu` would be correct.

Normally, configure will use `krb5-config` to determine the flags to use
to compile with your Kerberos libraries.  To specify a particular
`krb5-config` script to use, either set the `PATH_KRB5_CONFIG` environment
variable or pass it to configure like:

```
    ./configure PATH_KRB5_CONFIG=/path/to/krb5-config
```

If `krb5-config` isn't found, configure will look for the standard
Kerberos libraries in locations already searched by your compiler.  If the
the `krb5-config` script first in your path is not the one corresponding
to the Kerberos libraries you want to use, or if your Kerberos libraries
and includes aren't in a location searched by default by your compiler,
you need to specify a different Kerberos installation root via
`--with-krb5=PATH`.  For example:

```
    ./configure --with-krb5=/usr/pubsw
```

You can also individually set the paths to the include directory and the
library directory with `--with-krb5-include` and `--with-krb5-lib`.  You
may need to do this if Autoconf can't figure out whether to use `lib`,
`lib32`, or `lib64` on your platform.

To not use `krb5-config` and force library probing even if there is a
`krb5-config` script on your path, set `PATH_KRB5_CONFIG` to a nonexistent
path:

```
    ./configure PATH_KRB5_CONFIG=/nonexistent
```

`krb5-config` is not used and library probing is always done if either
`--with-krb5-include` or `--with-krb5-lib` are given.

Pass `--enable-silent-rules` to configure for a quieter build (similar to
the Linux kernel).  Use `make warnings` instead of `make` to build with
full GCC compiler warnings (requires either GCC or Clang and may require a
relatively current version of the compiler).

You can pass the `--enable-reduced-depends` flag to configure to try to
minimize the shared library dependencies encoded in the binaries.  This
omits from the link line all the libraries included solely because other
libraries depend on them and instead links the programs only against
libraries whose APIs are called directly.  This will only work with shared
libraries and will only work on platforms where shared libraries properly
encode their own dependencies (this includes most modern platforms such as
all Linux).  It is intended primarily for building packages for Linux
distributions to avoid encoding unnecessary shared library dependencies
that make shared library migrations more difficult.  If none of the above
made any sense to you, don't bother with this flag.

## Testing

pam-krb5 comes with a comprehensive test suite, but it requires some
configuration in order to test anything other than low-level utility
functions.  For the full test suite, you will need to have a running KDC
in which you can create two test accounts, one with admin access to the
other.  Using a test KDC environment, if you have one, is recommended.

Follow the instructions in `tests/config/README` to configure the test
suite.

Now, you can run the test suite with:

```
    make check
```

If a test fails, you can run a single test with verbose output via:

```
    tests/runtests -o <name-of-test>
```

Do this instead of running the test program directly since it will ensure
that necessary environment variables are set up.

The default libkadm5clnt library on the system must match the
implementation of your KDC for the module/expired test to work, since the
two kadmin protocols are not compatible.  If you use the MIT library
against a Heimdal server, the test will be skipped; if you use the Heimdal
library against an MIT server, the test suite may hang.

Several `module/expired` tests are expected to fail with Heimdal 1.5 due
to a bug in Heimdal with reauthenticating immediately after a
library-mediated password change of an expired password.  This is fixed in
later releases of Heimdal.

To run the full test suite, Perl 5.10 or later is required.  The following
additional Perl modules will be used if present:

* Test::Pod
* Test::Spelling

All are available on CPAN.  Those tests will be skipped if the modules are
not available.

To enable tests that don't detect functionality problems but are used to
sanity-check the release, set the environment variable `RELEASE_TESTING`
to a true value.  To enable tests that may be sensitive to the local
environment or that produce a lot of false positives without uncovering
many problems, set the environment variable `AUTHOR_TESTING` to a true
value.

## Configuring

Just installing the module does not enable it or change anything about
your system authentication configuration.  To use the module for all
system authentication on Debian systems, put something like:

```
    auth  sufficient   pam_krb5.so minimum_uid=1000
    auth  required     pam_unix.so try_first_pass nullok_secure
```

in `/etc/pam.d/common-auth`, something like:

```
    session  optional  pam_krb5.so minimum_uid=1000
    session  required  pam_unix.so
```

in `/etc/pam.d/common-session`, and something like:

```
    account  required  pam_krb5.so minimum_uid=1000
    account  required  pam_unix.so
```

in `/etc/pam.d/common-account`.  The `minimum_uid` setting tells the PAM
module to pass on any users with a UID lower than 1000, thereby bypassing
Kerberos authentication for the root account and any system accounts.  You
normally want to do this since otherwise, if the network is down, the
Kerberos authentication can time out and make it difficult to log in as
root and fix matters.  This also avoids problems with Kerberos principals
that happen to match system accounts accidentally getting access to those
accounts.

Be sure to include the module in the session group as well as the auth
group.  Without the session entry, the user's ticket cache will not be
created properly for ssh logins (among possibly others).

If your users should normally all use Kerberos passwords exclusively,
putting something like:

```
    password sufficient pam_krb5.so minimum_uid=1000
    password required   pam_unix.so try_first_pass obscure md5
```

in `/etc/pam.d/common-password` will change users' passwords in Kerberos
by default and then only fall back on Unix if that doesn't work.  (You can
make this tighter by using the more complex new-style PAM configuration.)
If you instead want to synchronize local and Kerberos passwords and change
them both at the same time, you can do something like:

```
    password required   pam_unix.so obscure sha512
    password required   pam_krb5.so use_authtok minimum_uid=1000
```

If you have multiple environments that you want to synchronize and you
don't want password changes to continue if the Kerberos password change
fails, use the `clear_on_fail` option.  For example:

```
    password required   pam_krb5.so clear_on_fail minimum_uid=1000
    password required   pam_unix.so use_authtok obscure sha512
    password required   pam_smbpass.so use_authtok
```

In this case, if `pam_krb5` cannot change the password (due to password
strength rules on the KDC, for example), it will clear the stored password
(because of the `clear_on_fail` option), and since `pam_unix` and
`pam_smbpass` are both configured with `use_authtok`, they will both fail.
`clear_on_fail` is not the default because it would interfere with the
more common pattern of falling back to local passwords if the user doesn't
exist in Kerberos.

If you use a more complex configuration with the Linux PAM `[]` syntax for
the session and account groups, note that `pam_krb5` returns a status of
ignore, not success, if the user didn't log on with Kerberos.  You may
need to handle that explicitly with `ignore=ignore` in your action list.

There are many, many other possibilities.  See the Linux PAM documentation
for all the configuration options.

On Red Hat systems, modify `/etc/pam.d/system-auth` instead, which
contains all of the configuration for the different stacks.

You can also use pam-krb5 only for specific services.  In that case,
modify the files in `/etc/pam.d` for that particular service to use
`pam_krb5.so` for authentication.  For services that are using passwords
over TLS to authenticate users, you may want to use the `ignore_k5login`
and `no_ccache` options to the authenticate module.  `.k5login`
authorization is only meaningful for local accounts and ticket caches are
usually (although not always) only useful for interactive sessions.

Configuring the module for Solaris is both simpler and less flexible,
since Solaris (at least Solaris 8 and 9, which are the last versions of
Solaris with which this module was extensively tested) use a single
`/etc/pam.conf` file that contains configuration for all programs.  For
console login on Solaris, try something like:

```
    login auth sufficient /usr/local/lib/security/pam_krb5.so minimum_uid=100
    login auth required /usr/lib/security/pam_unix_auth.so.1 use_first_pass
    login account required /usr/local/lib/security/pam_krb5.so minimum_uid=100
    login account required /usr/lib/security/pam_unix_account.so.1
    login session required /usr/local/lib/security/pam_krb5.so retain_after_close minimum_uid=100
    login session required /usr/lib/security/pam_unix_session.so.1
```

A similar configuration could be used for other services, such as ssh.
See the pam.conf(5) man page for more information.  When using this module
with Solaris login (at least on Solaris 8 and 9), you will probably also
need to add `retain_after_close` to the PAM configuration to avoid having
the user's credentials deleted before they are logged in.

The Solaris Kerberos library reportedly does not support prompting for a
password change of an expired account during authentication.  Supporting
password change for expired accounts on Solaris with native Kerberos may
therefore require setting the `defer_pwchange` or `force_pwchange` option
for selected login applications.  See the description and warnings about
that option in the pam_krb5(5) man page.

Some configuration options may be put in the `krb5.conf` file used by your
Kerberos libraries (usually `/etc/krb5.conf` or
`/usr/local/etc/krb5.conf`) instead or in addition to the PAM
configuration.  See the man page for more details.

The Kerberos library, via pam-krb5, will prompt the user to change their
password if their password is expired, but when using OpenSSH, this will
only work when `ChallengeResponseAuthentication` is enabled.  Unless this
option is enabled, OpenSSH doesn't pass PAM messages to the user and can
only respond to a simple password prompt.

If you are using MIT Kerberos, be aware that users whose passwords are
expired will not be prompted to change their password unless the KDC
configuration for your realm in `[realms]` in `krb5.conf` contains a
`master_kdc` setting or, if using DNS SRV records, you have a DNS entry
for `_kerberos-master` as well as `_kerberos`.

## Debugging

The first step when debugging any problems with this module is to add
`debug` to the PAM options for the module (either in the PAM configuration
or in `krb5.conf`).  This will significantly increase the logging from the
module and should provide a trace of exactly what failed and any available
error information.

Many Kerberos authentication problems are due to configuration issues in
`krb5.conf`.  If pam-krb5 doesn't work, first check that `kinit` works on
the same system.  That will test your basic Kerberos configuration.  If
the system has a keytab file installed that's readable by the process
doing authentication via PAM, make sure that the keytab is current and
contains a key for `host/<system>` where <system> is the fully-qualified
hostname.  pam-krb5 prevents KDC spoofing by checking the user's
credentials when possible, but this means that if a keytab is present it
must be correct or authentication will fail.  You can check the keytab
with `klist -k` and `kinit -k`.

Be sure that all libraries and modules, including PAM modules, loaded by a
program use the same Kerberos libraries.  Sometimes programs that use PAM,
such as current versions of OpenSSH, also link against Kerberos directly.
If your sshd is linked against one set of Kerberos libraries and pam-krb5
is linked against a different set of Kerberos libraries, this will often
cause problems (such as segmentation faults, bus errors, assertions, or
other strange behavior).  Similar issues apply to the com_err library or
any other library used by both modules and shared libraries and by the
application that loads them.  If your OS ships Kerberos libraries, it's
usually best if possible to build all Kerberos software on the system
against those libraries.

## Implementation Notes

The normal sequence of actions taken for a user login is:

```
    pam_authenticate
    pam_setcred(PAM_ESTABLISH_CRED)
    pam_open_session
    pam_acct_mgmt
```

and then at logout:

```
    pam_close_session
```

followed by closing the open PAM session.  The corresponding `pam_sm_*`
functions in this module are called when an application calls those public
interface functions.  Not all applications call all of those functions, or
in particularly that order, although `pam_authenticate` is always first
and has to be.

When `pam_authenticate` is called, pam-krb5 creates a temporary ticket
cache in `/tmp` and sets the PAM environment variable `PAM_KRB5CCNAME` to
point to it.  This ticket cache will be automatically destroyed when the
PAM session is closed and is there only to pass the initial credentials to
the call to `pam_setcred`.  The module would use a memory cache, but
memory caches will only work if the application preserves the PAM
environment between the calls to `pam_authenticate` and `pam_setcred`.
Most do, but OpenSSH notoriously does not and calls `pam_authenticate` in
a subprocess, so this method is used to pass the tickets to the
`pam_setcred` call in a different process.

`pam_authenticate` does a complete authentication, including checking the
resulting TGT by obtaining a service ticket for the local host if
possible, but this requires read access to the system keytab.  If the
keytab doesn't exist, can't be read, or doesn't include the appropriate
credentials, the default is to accept the authentication.  This can be
controlled by setting `verify_ap_req_nofail` to true in `[libdefaults]` in
`/etc/krb5.conf`.  `pam_authenticate` also does a basic authorization
check, by default calling `krb5_kuserok` (which uses `~/.k5login` if
available and falls back to checking that the principal corresponds to the
account name).  This can be customized with several options documented in
the pam_krb5(5) man page.

pam-krb5 treats `pam_open_session` and `pam_setcred(PAM_ESTABLISH_CRED)`
as synonymous, as some applications call one and some call the other.
Both copy the initial credentials from the temporary cache into a
permanent cache for this session and set `KRB5CCNAME` in the environment.
It will remember when the credential cache has been established and then
avoid doing any duplicate work afterwards, since some applications call
`pam_setcred` or `pam_open_session` multiple times (most notably X.Org 7
and earlier xdm, which also throws away the module settings the last time
it calls them).

`pam_acct_mgmt` finds the ticket cache, reads it in to obtain the
authenticated principal, and then does is another authorization check
against `.k5login` or the local account name as described above.

After the call to `pam_setcred` or `pam_open_session`, the ticket cache
will be destroyed whenever the calling application either destroys the PAM
environment or calls `pam_close_session`, which it should do on user
logout.

The normal sequence of events when refreshing a ticket cache (such as
inside a screensaver) is:

```
    pam_authenticate
    pam_setcred(PAM_REINITIALIZE_CRED)
    pam_acct_mgmt
```

(`PAM_REFRESH_CRED` may be used instead.)  Authentication proceeds as
above.  At the `pam_setcred` stage, rather than creating a new ticket
cache, the module instead finds the current ticket cache (from the
`KRB5CCNAME` environment variable or the default ticket cache location
from the Kerberos library) and then reinitializes it with the credentials
from the temporary `pam_authenticate` ticket cache.  When refreshing a
ticket cache, the application should not open a session.  Calling
`pam_acct_mgmt` is optional; pam-krb5 doesn't do anything different when
it's called in this case.

If `pam_authenticate` apparently didn't succeed, or if an account was
configured to be ignored via `ignore_root` or `minimum_uid`, `pam_setcred`
(and therefore `pam_open_session`) and `pam_acct_mgmt` return
`PAM_IGNORE`, which tells the PAM library to proceed as if that module
wasn't listed in the PAM configuration at all.  `pam_authenticate`,
however, returns failure in the ignored user case by default, since
otherwise a configuration using `ignore_root` with pam-krb5 as the only
PAM module would allow anyone to log in as root without a password.  There
doesn't appear to be a case where returning `PAM_IGNORE` instead would
improve the module's behavior, but if you know of a case, please let me
know.

By default, `pam_authenticate` intentionally does not follow the PAM
standard for handling expired accounts and instead returns failure from
`pam_authenticate` unless the Kerberos libraries are able to change the
account password during authentication.  Too many applications either do
not call `pam_acct_mgmt` or ignore its exit status.  The fully correct PAM
behavior (returning success from `pam_authenticate` and
`PAM_NEW_AUTHTOK_REQD` from `pam_acct_mgmt`) can be enabled with the
`defer_pwchange` option.

The `defer_pwchange` option is unfortunately somewhat tricky to implement.
In this case, the calling sequence is:

```
    pam_authenticate
    pam_acct_mgmt
    pam_chauthtok
    pam_setcred
    pam_open_session
```

During the first `pam_authenticate`, we can't obtain credentials and
therefore a ticket cache since the password is expired.  But
`pam_authenticate` isn't called again after `pam_chauthtok`, so
`pam_chauthtok` has to create a ticket cache.  We however don't want it to
do this for the normal password change (`passwd`) case.

What we do is set a flag in our PAM data structure saying that we're
processing an expired password, and `pam_chauthtok`, if it sees that flag,
redoes the authentication with password prompting disabled after it
finishes changing the password.

Unfortunately, when handling password changes this way, `pam_chauthtok`
will always have to prompt the user for their current password again even
though they just typed it.  This is because the saved authentication
tokens are cleared after `pam_authenticate` returns, for security reasons.
We could hack around this by saving the password in our PAM data
structure, but this would let the application gain access to it (exactly
what the clearing is intended to prevent) and breaks a PAM library
guarantee.  We could also work around this by having `pam_authenticate`
get the `kadmin/changepw` authenticator in the expired password case and
store it for `pam_chauthtok`, but it doesn't seem worth the hassle.

## History and Acknowledgements

Originally written by Frank Cusack <fcusack@fcusack.com>, with the
following acknowledgement:

> Thanks to Naomaru Itoi <itoi@eecs.umich.edu>, Curtis King
> <curtis.king@cul.ca>, and Derrick Brashear <shadow@dementia.org>, all of
> whom have written and made available Kerberos 4/5 modules.  Although no
> code in this module is directly from these author's modules, (except the
> get_user_info() routine in support.c; derived from whichever of these
> authors originally wrote the first module the other 2 copied from), it
> was extremely helpful to look over their code which aided in my design.

The module was then patched for the FreeBSD ports collection with
additional modifications by unknown maintainers and then was modified by
Joel Kociolek <joko@logidee.com> to be usable with Debian GNU/Linux.

It was packaged by Sam Hartman as the Kerberos v5 PAM module for Debian
and improved and modified by him and later by Russ Allbery to fix bugs and
add additional features.  It was then adopted by Andres Salomon, who added
support for refreshing credentials.

The current distribution is maintained by Russ Allbery, who also added
support for reading configuration from `krb5.conf`, added many features
for compatibility with the Sourceforge module, commented and standardized
the formatting of the code, and overhauled the documentation.

Thanks to Douglas E. Engert for the initial implementation of PKINIT
support.  I have since modified and reworked it extensively, so any bugs
or compilation problems are my fault.

Thanks to Markus Moeller for lots of debugging and multiple patches and
suggestions for improved portability.

Thanks to Booker Bense for the implementation of the `alt_auth_map`
option.

Thanks to Sam Hartman for the FAST support implementation.

## Support

The [pam-krb5 web page](https://www.eyrie.org/~eagle/software/pam-krb5/)
will always have the current version of this package, the current
documentation, and pointers to any additional resources.

For bug tracking, use the [issue tracker on
GitHub](https://github.com/rra/pam-krb5/issues).  However, please be aware
that I tend to be extremely busy and work projects often take priority.
I'll save your report and get to it as soon as I can, but it may take me a
couple of months.

## Source Repository

pam-krb5 is maintained using Git.  You can access the current source on
[GitHub](https://github.com/rra/pam-krb5) or by cloning the repository at:

https://git.eyrie.org/git/kerberos/pam-krb5.git

or [view the repository on the
web](https://git.eyrie.org/?p=kerberos/pam-krb5.git).

The eyrie.org repository is the canonical one, maintained by the author,
but using GitHub is probably more convenient for most purposes.  Pull
requests are gratefully reviewed and normally accepted.

## License

The pam-krb5 package as a whole is covered by the following copyright
statement and license:

> Copyright 2005-2010, 2014-2015, 2017, 2020-2021
>     Russ Allbery <eagle@eyrie.org>
>
> Copyright 2009-2011
>     The Board of Trustees of the Leland Stanford Junior University
>
> Copyright 2005
>     Andres Salomon <dilinger@debian.org>
>
> Copyright 1999-2000
>     Frank Cusack <fcusack@fcusack.com>
>
> Redistribution and use in source and binary forms, with or without
> modification, are permitted provided that the following conditions are
> met:
>
> 1. Redistributions of source code must retain the above copyright
>    notice, and the entire permission notice in its entirety, including
>    the disclaimer of warranties.
>
> 2. Redistributions in binary form must reproduce the above copyright
>    notice, this list of conditions and the following disclaimer in the
>    documentation and/or other materials provided with the distribution.
>
> 3. The name of the author may not be used to endorse or promote products
>    derived from this software without specific prior written permission.
>
> ALTERNATIVELY, this product may be distributed under the terms of the GNU
> General Public License, in which case the provisions of the GPL are
> required INSTEAD OF the above restrictions.  (This clause is necessary due
> to a potential bad interaction between the GPL and the restrictions
> contained in a BSD-style copyright.)
>
> THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
> INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
> AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL
> THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
> EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
> PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
> PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
> LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
> NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
> SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

Some files in this distribution are individually released under different
licenses, all of which are compatible with the above general package
license but which may require preservation of additional notices.  All
required notices, and detailed information about the licensing of each
file, are recorded in the LICENSE file.

Files covered by a license with an assigned SPDX License Identifier
include SPDX-License-Identifier tags to enable automated processing of
license information.  See https://spdx.org/licenses/ for more information.

For any copyright range specified by files in this package as YYYY-ZZZZ,
the range specifies every single year in that closed interval.
