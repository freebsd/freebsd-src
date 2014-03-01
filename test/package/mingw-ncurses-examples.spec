Summary: ncurses-examples - example/test programs from ncurses
%?mingw_package_header

%define AppProgram ncurses-examples
%define AppVersion MAJOR.MINOR
%define AppRelease YYYYMMDD
# $Id: mingw-ncurses-examples.spec,v 1.3 2013/10/26 23:30:21 tom Exp $
Name: mingw32-ncurses6-examples
Version: %{AppVersion}
Release: %{AppRelease}
License: X11
Group: Development/Libraries
Source: ncurses-examples-%{release}.tgz
# URL: http://invisible-island.net/ncurses/

BuildRequires:  mingw32-ncurses6

BuildRequires:  mingw32-filesystem >= 95
BuildRequires:  mingw32-gcc
BuildRequires:  mingw32-binutils

BuildRequires:  mingw64-ncurses6

BuildRequires:  mingw64-filesystem >= 95
BuildRequires:  mingw64-gcc
BuildRequires:  mingw64-binutils

%define CC_NORMAL -Wall -Wstrict-prototypes -Wmissing-prototypes -Wshadow -Wconversion
%define CC_STRICT %{CC_NORMAL} -W -Wbad-function-cast -Wcast-align -Wcast-qual -Wmissing-declarations -Wnested-externs -Wpointer-arith -Wwrite-strings -ansi -pedantic

%description -n mingw32-ncurses6-examples
Cross-compiling support for ncurses to mingw32.

The ncurses library routines are a terminal-independent method of
updating character screens with reasonable optimization.

This package is used for testing ABI 6 with cross-compiles to MinGW.

%package -n mingw64-ncurses6-examples
Summary:        Curses library for MinGW64

%description -n mingw64-ncurses6-examples
Cross-compiling support for ncurses to mingw64.

The ncurses library routines are a terminal-independent method of
updating character screens with reasonable optimization.

This package is used for testing ABI 6 with cross-compiles to MinGW.

%prep

# override location of bindir, e.g., to avoid conflict with pdcurses
%global mingw32_bindir %{mingw32_exec_prefix}/bin/%{AppProgram} 
%global mingw64_bindir %{mingw64_exec_prefix}/bin/%{AppProgram} 

%define CFG_OPTS \\\
	--disable-echo \\\
	--enable-warnings \\\
	--verbose

%define debug_package %{nil}
%setup -q -n ncurses-examples-%{release}

%build
mkdir BUILD-W32
pushd BUILD-W32
CFLAGS="%{CC_NORMAL}" \
CC=%{mingw32_cc} \
%mingw32_configure %{CFG_OPTS}
cp config.status /tmp/ming32-config.status
make
popd

mkdir BUILD-W64
pushd BUILD-W64
CFLAGS="%{CC_NORMAL}" \
CC=%{mingw64_cc} \
%mingw64_configure %{CFG_OPTS}
cp config.status /tmp/ming64-config.status
make
popd

%install
rm -rf $RPM_BUILD_ROOT

pushd BUILD-W32
%{mingw32_make} install DESTDIR=$RPM_BUILD_ROOT
popd

pushd BUILD-W64
%{mingw64_make} install DESTDIR=$RPM_BUILD_ROOT
popd

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root,-)

%files -n mingw32-ncurses6-examples
%{mingw32_bindir}/*

%files -n mingw64-ncurses6-examples
%{mingw64_bindir}/*

%changelog

* Sat Oct 19 2013 Thomas E. Dickey
- initial version
