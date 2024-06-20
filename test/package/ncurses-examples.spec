Summary: example/test programs from ncurses
%global AppProgram ncurses-examples
%global AltProgram ncursest-examples
%global AppVersion MAJOR.MINOR
%global AppRelease YYYYMMDD
# $Id: ncurses-examples.spec,v 1.22 2023/02/25 23:10:49 tom Exp $
Name: %{AppProgram}
Version: %{AppVersion}
Release: %{AppRelease}
License: MIT
Group: Applications/Development
URL: https://invisible-island.net/ncurses/%{AppProgram}.html
Source: https://invisible-island.net/archives/%{AppProgram}/%{AppProgram}-%{release}.tgz

%description
These are the example/test programs from the ncurses MAJOR.MINOR distribution,
for patch-date YYYYMMDD.

This package installs in "bin/%{AppProgram}" to avoid conflict with other
packages.

%package -n %{AltProgram}
Summary:  examples/test programs from ncurses with POSIX thread support

%description -n %{AltProgram}
These are the example/test programs from the ncurses MAJOR.MINOR distribution,
for patch-date YYYYMMDD, using the "ncurseswt" library to demonstrate the
use of POSIX threads, e.g., in ditto, rain, and worm.

This package installs in "bin/%{AltProgram}" to avoid conflict with other
packages.

%prep

%setup -q -n %{AppProgram}-%{AppRelease}

%define debug_package %{nil}

%build

%global _configure ../configure
%define my_srcdir ..

mkdir BUILD-%{AppProgram}
pushd BUILD-%{AppProgram}
INSTALL_PROGRAM='${INSTALL}' \
NCURSES_CONFIG_SUFFIX=dev \
CONFIGURE_TOP=%{my_srcdir} \
%configure \
	--target %{_target_platform} \
	--prefix=%{_prefix} \
	--datadir=%{_datarootdir}/%{AppProgram} \
	--with-screen=ncursesw6dev \
	--disable-rpath-hack

make
popd

mkdir BUILD-%{AltProgram}
pushd BUILD-%{AltProgram}
INSTALL_PROGRAM='${INSTALL}' \
NCURSES_CONFIG_SUFFIX=dev \
CONFIGURE_TOP=%{my_srcdir} \
%configure \
	--target %{_target_platform} \
	--prefix=%{_prefix} \
	--datadir=%{_datarootdir}/%{AltProgram} \
	--with-screen=ncursestw6dev \
	--disable-rpath-hack

make
popd

%install
[ "$RPM_BUILD_ROOT" != "/" ] && rm -rf $RPM_BUILD_ROOT

pushd BUILD-%{AppProgram}
make install PACKAGE=%{AppProgram} DESTDIR=$RPM_BUILD_ROOT
popd

pushd BUILD-%{AltProgram}
make install PACKAGE=%{AltProgram} DESTDIR=$RPM_BUILD_ROOT
popd

%files -n %{AppProgram}
%defattr(-,root,root)
%{_bindir}/%{AppProgram}
%{_libexecdir}/%{AppProgram}/*
%{_datarootdir}/%{AppProgram}/*

%files -n %{AltProgram}
%defattr(-,root,root)
%{_bindir}/%{AltProgram}
%{_libexecdir}/%{AltProgram}/*
%{_datarootdir}/%{AltProgram}/*

%changelog
# each patch should add its ChangeLog entries here

* Sat Feb 25 2023 Thomas Dickey
- amend URLs per rpmlint

* Sat Dec 18 2021 Thomas Dickey
- use libexecdir for programs rather than subdir of bindir

* Sat Nov 16 2019 Thomas Dickey
- modify clean-rule to work around Fedora NFS bugs.

* Sat Nov 11 2017 Thomas Dickey
- add example data-files
- use rpm built-in "configure"
- suppress debug-package

* Thu Mar 25 2010 Thomas Dickey
- initial version
