Summary: shared libraries for terminal handling
Name: ncurses6
Version: 5.9
Release: 20140222
License: X11
Group: Development/Libraries
Source: ncurses-%{version}-%{release}.tgz
# URL: http://invisible-island.net/ncurses/

%define CC_NORMAL -Wall -Wstrict-prototypes -Wmissing-prototypes -Wshadow -Wconversion
%define CC_STRICT %{CC_NORMAL} -W -Wbad-function-cast -Wcast-align -Wcast-qual -Wmissing-declarations -Wnested-externs -Wpointer-arith -Wwrite-strings -ansi -pedantic

%define _prefix /usr/local/ncurses6
%define MYDATA /usr/local/ncurses/share/terminfo

%description
The ncurses library routines are a terminal-independent method of
updating character screens with reasonable optimization.

This package is used for testing ABI 6.

%prep

%define debug_package %{nil}
%setup -q -n ncurses-%{version}-%{release}

%build
CFLAGS="%{CC_NORMAL}" \
RPATH_LIST=../lib:%{_prefix}/lib \
%configure \
	--target %{_target_platform} \
	--prefix=%{_prefix} \
	--includedir='${prefix}/include' \
	--with-default-terminfo-dir=%{MYDATA} \
	--with-install-prefix=$RPM_BUILD_ROOT \
	--with-terminfo-dirs=%{MYDATA}:/usr/share/terminfo \
	--disable-echo \
	--disable-getcap \
	--disable-leaks \
	--disable-macros  \
	--disable-overwrite  \
	--disable-termcap \
	--enable-const \
	--enable-ext-colors \
	--enable-ext-mouse \
	--enable-hard-tabs \
	--enable-interop \
	--enable-rpath \
	--enable-sp-funcs \
	--enable-warnings \
	--enable-widec \
	--verbose \
	--with-develop \
	--with-shared \
	--with-termlib \
	--with-ticlib \
	--with-trace \
	--with-cxx-shared \
	--with-xterm-kbs=DEL \
	--without-ada \
	--without-debug \
	--without-normal

make

%install
rm -rf $RPM_BUILD_ROOT

make install.libs install.progs
rm -f test/ncurses
( cd test && make ncurses LOCAL_LIBDIR=%{_libdir} && mv ncurses $RPM_BUILD_ROOT/%{_bindir}/ncurses6 )

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root,-)
%{_bindir}/*
%{_includedir}/*
%{_libdir}/*
#%{_datadir}/*

%changelog

* Sat Mar 09 2013 Thomas E. Dickey
- add --with-cxx-shared option to demonstrate c++ binding as shared library

* Sat Oct 27 2012 Thomas E. Dickey
- add ncurses program as "ncurses6" to provide demonstration.

* Fri Jun 08 2012 Thomas E. Dickey
- initial version.
