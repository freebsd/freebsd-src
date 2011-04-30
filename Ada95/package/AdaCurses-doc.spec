Summary: AdaCurses - Ada95 binding documentation for ncurses
%define AppProgram AdaCurses
%define AppVersion MAJOR.MINOR
%define AppRelease YYYYMMDD
%define AppPackage %{AppProgram}-doc
# $Id: AdaCurses-doc.spec,v 1.1 2011/03/26 19:22:39 tom Exp $
Name: %{AppPackage}
Version: %{AppVersion}
Release: %{AppRelease}
License: MIT
Group: Applications/Development
URL: ftp://invisible-island.net/%{AppProgram}
Source0: %{AppProgram}-%{AppRelease}.tgz
Packager: Thomas Dickey <dickey@invisible-island.net>

%description
This is the Ada95 binding documentation from the ncurses MAJOR.MINOR
distribution, for patch-date YYYYMMDD.
%prep

%setup -q -n %{AppProgram}-%{AppRelease}

%build

INSTALL_PROGRAM='${INSTALL}' \
	./configure \
		--target %{_target_platform} \
		--prefix=%{_prefix} \
		--datadir=%{_datadir} \
		--with-ada-sharedlib

%install
[ "$RPM_BUILD_ROOT" != "/" ] && rm -rf $RPM_BUILD_ROOT

(cd doc && make install.html DESTDIR=$RPM_BUILD_ROOT )

%clean
[ "$RPM_BUILD_ROOT" != "/" ] && rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root)
%{_datadir}/doc/AdaCurses

%changelog
# each patch should add its ChangeLog entries here

* Sat Mar 26 2010 Thomas Dickey
- initial version
