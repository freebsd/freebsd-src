%define ver 1.0.5

Name: libibcm
Version: 1.0.5
Release: 1%{?dist}
Summary: Userspace InfiniBand Communication Manager.

Group: System Environment/Libraries
License: GPL/BSD
Url: http://www.openfabrics.org/
Source: http://www.openfabrics.org/downloads/%{name}-%{version}.tar.gz
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)

%description
libibcm provides a userspace InfiniBand Communication Managment library.

%package devel
Summary: Development files for the libibcm library
Group: System Environment/Libraries
Requires: %{name} = %{version}-%{release} %{_includedir}/infiniband/verbs.h

%description devel
Development files for the libibcm library.

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
%{_libdir}/libibcm*.so.*
%doc AUTHORS COPYING ChangeLog README

%files devel
%defattr(-,root,root,-)
%{_libdir}/lib*.so
%{_libdir}/*.a
%{_includedir}/*
