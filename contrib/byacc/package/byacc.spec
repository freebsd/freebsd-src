Summary: public domain Berkeley LALR Yacc parser generator

%global AppVersion 2.0
%global AppPatched 20230201

%global AltProgram byacc2
%global UseProgram yacc

# $Id: byacc.spec,v 1.70 2023/02/02 00:12:06 tom Exp $
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

%package -n byacc2
Summary: public domain Berkeley LALR Yacc parser generator with backtracking

%description -n byacc2
This package provides a parser generator utility that reads a grammar
specification from a file and generates an LR(1) parser for it.  The
parsers consist of a set of LALR(1) parsing tables and a driver
routine written in the C programming language.  It has a public domain
license which includes the generated C.

This package has the backtracking extension.

%prep

%global debug_package %{nil}

%setup -q -n %{name}-%{AppPatched}

%build
%define my_srcdir ..
%define CFG_OPTS \\\
  --verbose \\\
  --disable-echo \\\
  --enable-stdnoreturn \\\
  --target %{_target_platform} \\\
  --prefix=%{_prefix} \\\
  --srcdir=%{my_srcdir} \\\
  --bindir=%{_bindir} \\\
  --libdir=%{_libdir} \\\
  --mandir=%{_mandir}

%global _configure ../configure

mkdir BUILD-byacc
pushd BUILD-byacc
CONFIGURE_TOP=%{my_srcdir} \
%configure %{CFG_OPTS} \
  --program-prefix=b \
make
popd

mkdir BUILD-byacc2
pushd BUILD-byacc2
CONFIGURE_TOP=%{my_srcdir} \
%configure %{CFG_OPTS} \
  --enable-btyacc \
  --program-transform-name='s,\<yacc,byacc2,g' \
  --with-max-table-size=123456 \
make
popd

%install
[ "$RPM_BUILD_ROOT" != "/" ] && rm -rf $RPM_BUILD_ROOT

pushd BUILD-byacc
make install DESTDIR=$RPM_BUILD_ROOT
( cd $RPM_BUILD_ROOT%{_bindir} && ln -vs %{name} %{UseProgram} )
popd

pushd BUILD-byacc2
make install DESTDIR=$RPM_BUILD_ROOT
popd

%clean
[ "$RPM_BUILD_ROOT" != "/" ] && rm -rf $RPM_BUILD_ROOT

%files
%doc ACKNOWLEDGEMENTS CHANGES NEW_FEATURES NOTES NO_WARRANTY README
%license LICENSE
%{_bindir}/%{name}
%{_bindir}/%{UseProgram}
%{_mandir}/man1/%{name}.*

%files -n byacc2
%doc ACKNOWLEDGEMENTS CHANGES NEW_FEATURES NOTES NO_WARRANTY README README.BTYACC
%license LICENSE
%{_bindir}/%{AltProgram}
%{_mandir}/man1/%{AltProgram}.*

%changelog
# each patch should add its ChangeLog entries here

* Sun Jan 09 2022 Thomas Dickey
- rpmlint

* Sat Jan 01 2022 Thomas Dickey
- rename btyacc package to byacc2 to co-exist with traditional btyacc

* Fri May 25 2018 Thomas Dickey
- add btyacc package

* Sun Jul 09 2017 Thomas Dickey
- use predefined "configure"

* Sun Jun 06 2010 Thomas Dickey
- initial version
