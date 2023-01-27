Name:        libxo
Version:     1.6.0
Release:     1%{?dist}
Summary:     The libxo library

Prefix:      /usr

Vendor:      Juniper Networks, Inc.
Packager:    Phil Shafer <phil@juniper.net>
License:     BSD

Group:       Development/Libraries
URL:         https://github.com/Juniper/libxo
Source0:     https://github.com/Juniper/libxo/releases/1.6.0/libxo-1.6.0.tar.gz


%description
Welcome to libxo, a library that generates text, XML, JSON, and HTML
from a single source code path.

%prep
%setup -q

%build
%configure
make %{?_smp_mflags}

%install
rm -rf $RPM_BUILD_ROOT
%make_install

%clean
rm -rf $RPM_BUILD_ROOT

%post -p /sbin/ldconfig

%files
%{_bindir}/*
%{_includedir}/libxo/*
%{_libdir}/*
%{_datadir}/doc/libxo/*
%docdir %{_datadir}/doc/libxo/*
%{_mandir}/*/*
%docdir %{_mandir}/*/*
