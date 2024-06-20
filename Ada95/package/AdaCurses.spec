Summary: Ada95 binding for ncurses
%define AppProgram AdaCurses
%define AppVersion MAJOR.MINOR
%define AppRelease YYYYMMDD
# $Id: AdaCurses.spec,v 1.31 2022/12/18 00:08:17 tom Exp $
Name: %{AppProgram}
Version: %{AppVersion}
Release: %{AppRelease}
License: MIT
Group: Applications/Development
URL: ftp://ftp.invisible-island.net/%{AppProgram}
Source0: %{AppProgram}-%{AppRelease}.tgz
Packager: Thomas Dickey <dickey@invisible-island.net>

%description
This is the Ada95 binding from the ncurses MAJOR.MINOR distribution, for
patch-date YYYYMMDD.

In addition to a library, this package installs sample programs in
"bin/%{AppProgram}" to avoid conflict with other packages.
%prep

%global is_mandriva %(test -f /etc/mandriva-release && echo 1 || echo 0)
%global is_redhat   %(test -f /etc/redhat-release && echo 1 || echo 0)
%global is_suse     %(if grep -E -i '(opensuse)' /etc/issue >/dev/null; then echo 1; else echo 0; fi)

%define debug_package %{nil}

%define need_filter %(if grep -E -i '(red hat|fedora)' /etc/issue >/dev/null; then echo 1; elif test -f /etc/fedora-release; then echo 1; else echo 0; fi)

%if %{need_filter} == 1
# http://fedoraproject.org/wiki/EPEL:Packaging_Autoprovides_and_Requires_Filtering
%filter_from_requires /lib%{AppProgram}.so.1/d
%filter_setup
%endif

%setup -q -n %{AppProgram}-%{AppRelease}

%build

%define ada_libdir %{_libdir}/ada/adalib
%define ada_include %{_prefix}/share/ada/adainclude

%if %{is_mandriva}
# Mageia 8 lacks gprbuild, needed for building shared libraries.
%define ada_model --without-shared --without-ada-sharedlib --with-ada-objects=%{_libdir}/adalib
%else
# OpenSUSE actually lacks gprbuild, but there is a workable "community" package.
%define ada_model --with-shared --with-ada-sharedlib
%if %{is_redhat}
# Fedora 36 LTO does not work with gprbuild system configuration.
unset CFLAGS
unset LDFLAGS
unset LT_SYS_LIBRARY_PATH
%endif
%endif

INSTALL_PROGRAM='${INSTALL}' \
	./configure %{ada_model} \
		--target %{_target_platform} \
		--prefix=%{_prefix} \
		--bindir=%{_bindir} \
		--libdir=%{_libdir} \
		--libexecdir=%{_libexecdir} \
		--with-ada-include=%{ada_include} \
		--with-ada-objects=%{ada_libdir} \
		--mandir=%{_mandir} \
		--datadir=%{_datadir} \
		--disable-rpath-link \
		--disable-echo \
		--verbose \
		--enable-warnings

make

%install
[ "$RPM_BUILD_ROOT" != "/" ] && rm -rf $RPM_BUILD_ROOT

make install DESTDIR=$RPM_BUILD_ROOT
make install.examples DESTDIR=$RPM_BUILD_ROOT

%clean
if rm -rf $RPM_BUILD_ROOT; then
  echo OK
else
  find $RPM_BUILD_ROOT -type f | grep -F -v /.nfs && exit 1
fi
exit 0

%files
%defattr(-,root,root)
%{_bindir}/%{AppProgram}
%{_bindir}/adacurses*-config
%{_libexecdir}/%{AppProgram}/*
%{ada_libdir}/
%if %{need_filter} == 1
%{_libdir}/lib%{AppProgram}.*
%endif
%if %{is_suse}
%{_libdir}/lib%{AppProgram}.*
%endif
%{_mandir}/man1/adacurses*-config.1*
%{_datadir}/%{AppProgram}/*
%{ada_include}/

%changelog
# each patch should add its ChangeLog entries here

* Sat Dec 17 2022 Thomas Dickey
- install sample programs in libexec

* Sat Nov 19 2022 Thomas Dickey
- use static libraries for Mageia.

* Sat Nov 12 2022 Thomas Dickey
- unset environment variables to work around Fedora LTO bugs.
- build-fix for OpenSUSE with gprbuild.

* Sat Nov 16 2019 Thomas Dickey
- modify clean-rule to work around Fedora NFS bugs.

* Sat Sep 14 2019 Thomas Dickey
- build-fixes for Fedora29, OpenSUSE

* Sat Sep 07 2019 Thomas Dickey
- use AppProgram to replace "AdaCurses" globally
- amend install-paths to work with Fedora30

* Thu Mar 31 2011 Thomas Dickey
- use --with-shared option for consistency with --with-ada-sharelib
- ensure that MY_DATADIR is set when installing examples
- add ada_libdir symbol to handle special case where libdir is /usr/lib64
- use --disable-rpath-link to link sample programs without rpath

* Fri Mar 25 2011 Thomas Dickey
- initial version
