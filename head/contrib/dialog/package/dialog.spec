Summary: dialog - display dialog boxes from shell scripts
%define AppProgram dialog
%define AppVersion 1.2
%define AppRelease 20130523
%define ActualProg c%{AppProgram}
# $XTermId: dialog.spec,v 1.50 2013/05/24 00:13:31 tom Exp $
Name: %{ActualProg}
Version: %{AppVersion}
Release: %{AppRelease}
License: LGPL
Group: Applications/Development
URL: ftp://invisible-island.net/%{AppProgram}
Source0: %{AppProgram}-%{AppVersion}-%{AppRelease}.tgz
Packager: Thomas Dickey <dickey@invisible-island.net>

%description
Dialog is a program that will let you to present a variety of questions
or display messages using dialog boxes  from  a  shell  script.   These
types  of  dialog boxes are implemented (though not all are necessarily
compiled into dialog):

      calendar, checklist, dselect, editbox, form, fselect, gauge,
      infobox, inputbox, inputmenu, menu, mixedform, mixedgauge,
      msgbox (message), passwordbox, passwordform, pause, progressbox,
      radiolist, tailbox, tailboxbg, textbox, timebox, and yesno
      (yes/no).

This package installs as "cdialog" to avoid conflict with other packages.
%prep

%define debug_package %{nil}

%setup -q -n %{AppProgram}-%{AppVersion}-%{AppRelease}

%build

INSTALL_PROGRAM='${INSTALL}' \
	./configure \
		--target %{_target_platform} \
		--prefix=%{_prefix} \
		--bindir=%{_bindir} \
		--libdir=%{_libdir} \
		--mandir=%{_mandir} \
		--with-package=%{ActualProg} \
		--enable-header-subdir \
		--enable-nls \
		--enable-widec \
		--with-libtool \
		--with-ncursesw \
		--disable-rpath-hack

make

%install
[ "$RPM_BUILD_ROOT" != "/" ] && rm -rf $RPM_BUILD_ROOT

make install-full               DESTDIR=$RPM_BUILD_ROOT
libtool --finish %{_libdir} 

strip $RPM_BUILD_ROOT%{_bindir}/%{ActualProg}
chmod 755 $RPM_BUILD_ROOT%{_libdir}/lib%{ActualProg}.so.*.*.*
rm -f $RPM_BUILD_ROOT%{_libdir}/lib%{ActualProg}.la

%clean
[ "$RPM_BUILD_ROOT" != "/" ] && rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root)
%{_bindir}/%{ActualProg}
%{_bindir}/%{ActualProg}-config
%{_mandir}/man1/%{ActualProg}.*
%{_mandir}/man3/%{ActualProg}.*
%{_includedir}/%{ActualProg}.h
%{_includedir}/%{ActualProg}/dlg_colors.h
%{_includedir}/%{ActualProg}/dlg_config.h
%{_includedir}/%{ActualProg}/dlg_keys.h
%{_libdir}/lib%{ActualProg}.*
%{_datadir}/locale/*/LC_MESSAGES/%{ActualProg}.mo 

%changelog
# each patch should add its ChangeLog entries here

* Tue Oct 18 2011 Thomas Dickey
- add executable permissions for shared libraries, discard ".la" file.

* Thu Dec 30 2010 Thomas Dickey
- initial version
