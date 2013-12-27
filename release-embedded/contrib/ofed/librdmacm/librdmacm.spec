%define ver 1.0.11

Name: librdmacm
Version: 1.0.11
Release: 1%{?dist}
Summary: Userspace RDMA Connection Manager

Group: System Environment/Libraries
License: GPLv2 or BSD
Url: http://www.openfabrics.org/
Source: http://www.openfabrics.org/downloads/rdmacm/%{name}-%{version}.tar.gz
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)

BuildRequires: libibverbs-devel >= 1.1-1

%description
librdmacm provides a userspace RDMA Communication Managment API.

%package devel
Summary: Development files for the librdmacm library
Group: System Environment/Libraries
Requires: %{name} = %{version}-%{release} %{_includedir}/infiniband/verbs.h

%description devel
Development files for the librdmacm library.

%package utils
Summary: Examples for the librdmacm library
Group: System Environment/Libraries
Requires: %{name} = %{version}-%{release}

%description utils
Example test programs for the librdmacm library.

%prep
%setup -q -n %{name}-%{ver}

%build
%configure
make %{?_smp_mflags}

%install
rm -rf $RPM_BUILD_ROOT
%makeinstall
# remove unpackaged files from the buildroot
rm -f $RPM_BUILD_ROOT%{_libdir}/*.la

%clean
rm -rf $RPM_BUILD_ROOT

%post -p /sbin/ldconfig
%postun -p /sbin/ldconfig

%files
%defattr(-,root,root,-)
%{_libdir}/librdmacm*.so.*
%doc AUTHORS COPYING README

%files devel
%defattr(-,root,root)
%{_libdir}/lib*.so
%{_libdir}/*.a
%{_includedir}/*
%{_mandir}/man3/*
%{_mandir}/man7/*

%files utils
%defattr(-,root,root,-)
%{_bindir}/*
%{_mandir}/man1/*

%changelog

* Fri Feb 15 2008 Roland Dreier <rdreier@cisco.com> - 1.0.6-1
- Initial Fedora spec file
