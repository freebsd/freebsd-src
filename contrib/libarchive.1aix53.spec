# $LastChangedRevision$, $LastChangedDate$
Summary:        Library to create and read several different archive formats
Summary(pl):    Biblioteka do tworzenia i odczytu ró¿nych formatów archiwów
Name:           libarchive
Version:        2.0a3
Release:        1aix53
License:        BSD
Group:          Libraries
Source0: http://people.freebsd.org/~kientzle/libarchive/src/%{name}-%{version}.tar.gz
Patch:          %{name}-0123457890.patch
URL:            http://people.freebsd.org/~kientzle/libarchive/
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

%description -l pl
Libarchive jest bibliotek± s³u¿ac± to tworzenia i odczytu wielu
ró¿nych strumieniowych formatów archiwów, w³±czaj±c w to popularne
odmiany TAR oraz wiele formatów CPIO. Biblioteka ta potrafi tak¿e
zapisywaæ archiwa SHAR.

%package devel
Summary:        Header files for libarchive library
Summary(pl):    Pliki nag³ówkowe biblioteki libarchive
Group:          Development/Libraries
Requires:       %{name} = %{version}-%{release}

%description devel
Header files for libarchive library.

%description devel -l pl
Pliki nag³ówkowe biblioteki libarchive.

%package static
Summary:        Static libarchive library
Summary(pl):    Statyczna biblioteka libarchive
Group:          Development/Libraries
Requires:       %{name}-devel = %{version}-%{release}

%description static
Static libarchive library.

%description static -l pl
Statyczna biblioteka libarchive.

%package -n bsdtar
Summary:        bsdtar - tar(1) implementation based on libarchive
Summary(pl):    bsdtar - implementacja programu tar(1) oparta na libarchive
Group:          Applications/Archiving
Requires:       %{name} = %{version}-%{release}

%description -n bsdtar
bsdtar - tar(1) implementation based on libarchive.

%description -n bsdtar -l pl
bsdtar - implementacja programu tar(1), oparta na libarchive.

%prep
%setup -q
%patch0 -p1

%build
# Specify paths to avoid use of vacpp
# -maix64 - required to use large files with aix-5.3
# -static - required for interoperability without copying libraries
# -D_BSD - required to include definition of makedev
# -X64 - required to assemble 64-bit COFF files
mkdir -p %{buildroot}
PATH=/opt/freeware/libexec:/opt/freeware/bin:/usr/local/bin:/usr/bin:/etc:/usr/sbin:/usr/ucb:/usr/bin/X11:/sbin:. \
CPATH=/opt/freeware/include:/usr/local/include \
LIBPATH=/opt/freeware/lib:/usr/local/lib:/usr/share/lib \
LD_LIBRARY_PATH=/opt/freeware/lib:/usr/local/lib:/usr/share/lib \
CFLAGS="$RPM_OPT_FLAGS -maix64 -static -D_BSD" \
CXXFLAGS="$RPM_OPT_FLAGS -maix64 -static -D_BSD" \
AR="ar -X64" \
./configure \
--prefix=%{_prefix} \
--libexecdir=%{_libexecdir} \
--mandir=%{_mandir} \
--infodir=%{_infodir} \
--enable-shared=yes \
--enable-static=yes \
| tee %{buildroot}/config.log
make | tee %{buildroot}/make.log

%install
[ "%buildroot" != "/" ] && [ -d %buildroot ] && rm -rf %buildroot;
make DESTDIR=%buildroot install
# original install builds, but does install bsdtar
cp .libs/%{name}.a %{buildroot}%{_libdir}
cp bsdtar %{buildroot}%{_bindir}
cp tar/bsdtar.1 %{buildroot}%{_mandir}/man1

%clean
rm -fr %buildroot

%files
%defattr(644,root,root,755)
%{_libdir}/libarchive.a

%files devel
%defattr(644,root,root,755)
%{_libdir}/libarchive.la
%{_includedir}/*.h
%doc %{_mandir}/man3/*
%doc %{_mandir}/man5/*

%files -n bsdtar
%defattr(644,root,root,755)
%attr(755,root,root) %{_bindir}/bsdtar
%doc %{_mandir}/man1/bsdtar.1*

%define date    %(echo `LC_ALL="C" date +"%a %b %d %Y"`)
%changelog
* %{date} PLD Team <feedback@pld-linux.org>
All persons listed below can be reached at <cvs_login>@pld-linux.org

$Log: libarchive.spec,v $
Release 1aix53  2006/12/12 rm1023@dcx.com
- tweak for aix-5.3
- added libarchive-0123457890.patch for "0123457890" error
- replaced libarchive-1.3.1.tar.gz with libarchive-2.0a3.tar.gz
- removed obsolete -CVE-2006-5680.patch and -man_progname.patch

Revision 1.6  2006/11/15 10:41:28  qboosh
- BR: acl-devel,attr-devel
- devel deps

Revision 1.5  2006/11/08 22:22:25  twittner
- up to 1.3.1
- added BR: e2fsprogs-devel
- added -CVE-2006-5680.patch against entering an infinite
loop in corrupt archives
- added bsdtar package (bsdtar is included now in libarchive
sources)
- rel. 0.1 for testing

Revision 1.4  2005/12/15 18:26:36  twittner
- up to 1.2.37
- removed -shared.patch (no longer needed)

Revision 1.3  2005/10/05 17:00:12  arekm
- up to 1.02.034

Revision 1.2  2005/07/27 20:17:21  qboosh
- typo

Revision 1.1  2005/07/27 08:36:03  adamg
- new
