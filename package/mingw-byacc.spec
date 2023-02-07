Summary: public domain Berkeley LALR Yacc parser generator

%global AppVersion 2.0
%global AppPatched 20230201

%global UseProgram yacc

# $Id: mingw-byacc.spec,v 1.47 2023/02/02 00:12:06 tom Exp $
Name: byacc
Version: %{AppVersion}.%{AppPatched}
Release: 1
License: Public Domain, MIT
URL: https://invisible-island.net/%{name}/
Source0: https://invisible-mirror.net/archives/%{name}/%{name}-%{AppPatched}.tgz

%description
This package provides a parser generator utility that reads a grammar
specification from a file and generates an LR(1) parser for it.  The
parsers consist of a set of LALR(1) parsing tables and a driver
routine written in the C programming language.  It has a public domain
license which includes the generated C.

%prep

%global debug_package %{nil}

%setup -q -n %{name}-%{AppPatched}

%build
%configure --verbose \
		--program-prefix=b \
		--target %{_target_platform} \
		--prefix=%{_prefix} \
		--bindir=%{_bindir} \
		--libdir=%{_libdir} \
		--mandir=%{_mandir}

make

%install
[ "$RPM_BUILD_ROOT" != "/" ] && rm -rf $RPM_BUILD_ROOT

make install DESTDIR=$RPM_BUILD_ROOT
( cd $RPM_BUILD_ROOT%{_bindir} && ln -s %{name} %{UseProgram} )

strip $RPM_BUILD_ROOT%{_bindir}/%{name}

%clean
[ "$RPM_BUILD_ROOT" != "/" ] && rm -rf $RPM_BUILD_ROOT

%files
%doc ACKNOWLEDGEMENTS CHANGES NEW_FEATURES NOTES NO_WARRANTY README
%license LICENSE
%{_bindir}/%{name}
%{_bindir}/%{UseProgram}
%{_mandir}/man1/%{name}.*

%changelog
# each patch should add its ChangeLog entries here

* Sun Jan 09 2022 Thomas Dickey
- rpmlint

* Sun Jul 09 2017 Thomas Dickey
- use predefined "configure"

* Wed Sep 25 2013 Thomas Dickey
- cloned from byacc.spec
