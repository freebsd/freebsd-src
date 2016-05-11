Summary:        Library to create and read several different archive formats
Name:           libarchive
Version:        3.1.2
Release:        1
License:        BSD
Group:          Libraries
Source0:	http://libarchive.org/downloads/%{name}-%{version}.tar.gz
URL:            http:/libarchive.org/
Requires:       glibc
Requires:       zlib
Requires:       bzip2
BuildRequires:  gcc
BuildRequires:  gcc-c++
BuildRequires:  gawk
BuildRequires:  zlib-devel
BuildRequires:  bzip2
BuildRoot:      %{_tmppath}/%{name}-%{version}-build

%description
Libarchive is a programming library that can create and read several
different streaming archive formats, including most popular TAR
variants and several CPIO formats. It can also write SHAR archives.

%package devel
Summary:        Header files for libarchive library
Group:          Development/Libraries
Requires:       %{name} = %{version}-%{release}

%description devel
Header files for libarchive library.

%package static
Summary:        Static libarchive library
Group:          Development/Libraries
Requires:       %{name}-devel = %{version}-%{release}

%description static
Static libarchive library.

%package -n bsdtar
Summary:        bsdtar - tar(1) implementation based on libarchive
Group:          Applications/Archiving
Requires:       %{name} = %{version}-%{release}

%description -n bsdtar
bsdtar - tar(1) implementation based on libarchive.

%package -n bsdcpio
Summary:	bsdcpio - cpio(1) implementation based on libarchive
Group:		Applications/Archiving
Requires:	%{name} = %{version}-%{release}

%description -n bsdcpio
bsdcpio - cpio(1) implementation based on libarchive

%prep
%setup -q

%build
mkdir -p %{buildroot}
./configure \
--prefix=%{_prefix} \
--libexecdir=%{_libexecdir} \
--libdir=%{_libdir} \
--mandir=%{_mandir} \
--infodir=%{_infodir} \
--enable-shared=yes \
--enable-static=yes \
| tee %{buildroot}/config.log
make | tee %{buildroot}/make.log

%install
[ "%buildroot" != "/" ] && [ -d %buildroot ] && rm -rf %buildroot;
make DESTDIR=%buildroot install

%clean
rm -fr %buildroot

%files
%{_libdir}/libarchive.so*

%files static
%{_libdir}/libarchive.a

%files devel
%{_libdir}/pkgconfig/libarchive.pc
%{_libdir}/libarchive.la
%{_includedir}/*.h
%doc %{_mandir}/man3/*
%doc %{_mandir}/man5/*

%files -n bsdtar
%attr(755,root,root) %{_bindir}/bsdtar
%doc %{_mandir}/man1/bsdtar.1*

%files -n bsdcpio
%attr(755,root,root) %{_bindir}/bsdcpio
%doc %{_mandir}/man1/bsdcpio.1*

%changelog
* Wed May 01 2013 Nikolai Lifanov <lifanov@mail.lifanov.com> - 3.1.2-1
- Initial package
- contrib/libarchive.spec by PLD team overhaul
- Added "bsdcpio" package
- Fixed build on x86_64 platform
