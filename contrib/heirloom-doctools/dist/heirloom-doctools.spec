#
# Sccsid @(#)heirloom-doctools.spec	1.18 (gritter) 3/20/07
#
Summary: The Heirloom Documentation Tools.
Name: heirloom-doctools
Version: 000000
Release: 1
License: Other
Source: %{name}-%{version}.tar.bz2
Group: System Environment/Base
Vendor: Gunnar Ritter <gunnarr@acm.org>
URL: <http://heirloom.sourceforge.net>
BuildRoot: %{_tmppath}/%{name}-root
BuildRequires: heirloom-devtools

%define	bindir		/usr/ucb
%define	mandir		/usr/share/man/5man
%define	libdir		/usr/ucblib
%define	docdir		%{libdir}/doctools
%define	macdir		%{docdir}/tmac
%define	fntdir		%{docdir}/font
%define	tabdir		%{docdir}/nterm
%define	hypdir		%{docdir}/hyphen
%define	pstdir		%{docdir}/font/devpost/postscript
%define	refdir		%{libdir}/reftools
%define	pubdir		/usr/pub

%define	xcc		gcc
%define	ccc		g++
%define	cflags		'-O -fomit-frame-pointer'
%define	cppflags	'-D__NO_STRING_INLINES -D_GNU_SOURCE'
%define	yacc		/usr/ccs/bin/yacc
%define	lex		/usr/ccs/bin/lex

#
# Combine the settings defined above.
#
%define	makeflags	ROOT=%{buildroot} INSTALL=install YACC=%{yacc} LEX=%{lex} MACDIR=%{macdir} FNTDIR=%{fntdir} TABDIR=%{tabdir} HYPDIR=%{hypdir} PUBDIR=%{pubdir} BINDIR=%{bindir} PSTDIR=%{pstdir} LIBDIR=%{libdir} REFDIR=%{refdir} MANDIR=%{mandir} CC=%{xcc} CCC=%{ccc} CFLAGS=%{cflags} CPPFLAGS=%{cppflags}

%description
The Heirloom Documentation Tools provide troff, nroff, and related
utilities to format manual pages and other documents for output on
terminals and printers. They are portable and enhanced versions of
the respective OpenSolaris utilities, which descend to ditroff and
the historical Unix troff. troff provides advanced typographical
features such as kerning, tracking, and hanging characters. It can
access PostScript Type 1, OpenType, and TrueType fonts directly.
Internationalized hyphenation, international paper sizes, and UTF-8
input are supported.

%prep
rm -rf %{buildroot}
%setup

%build
make %{makeflags}

%install
make %{makeflags} install

rm -f filelist.rpm
for f in %{bindir} %{macdir} %{fntdir} %{tabdir} %{hypdir} %{pstdir} %{pubdir} \
	%{libdir} %{refdir}
do
	if test -d %{buildroot}/$f
	then
		(cd %{buildroot}/$f; find * -type f -o -type l) | sed "s:^:$f/:"
	else
		echo $f
	fi
done | sort -u | sed '
	1i\
%defattr(-,root,root)\
%{mandir}\
%doc README CHANGES LICENSE/*
' >filelist.rpm

%clean
cd .. && rm -rf %{_builddir}/%{name}-%{version}
rm -rf %{buildroot}

%files -f filelist.rpm
