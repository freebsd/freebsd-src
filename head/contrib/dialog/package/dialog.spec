Summary: dialog - display dialog boxes from shell scripts
%define AppProgram dialog
%define AppVersion 1.1
%define AppRelease 20110707
%define ActualProg c%{AppProgram}
# $XTermId: dialog.spec,v 1.24 2011/07/07 22:10:02 tom Exp $
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

strip $RPM_BUILD_ROOT%{_bindir}/%{ActualProg}

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

* Thu Dec 30 2010 Thomas Dickey
- initial version
