
# Some of this will need re-evaluation post-LSB.  The SVIdir is there
# because the link appeared broken.  The rest is for easy compilation,
# the tradeoff open to discussion.  (LC957)

%define	SVIdir		/etc/rc.d/init.d
%{!?_defaultdocdir:%define	_defaultdocdir	%{_prefix}/share/doc/packages}
%{!?SVIcdir:%define		SVIcdir		/etc/sysconfig/daemons}

%define _mandir		%{_prefix}/share/man/en
%define _sysconfdir	/etc/ssh
%define	_libexecdir	%{_libdir}/ssh

# Do we want to disable root_login? (1=yes 0=no)
%define no_root_login 0

#old cvs stuff.  please update before use.  may be deprecated.
%define use_stable	1
%define version 	6.2p2
%if %{use_stable}
  %define cvs		%{nil}
  %define release 	1
%else
  %define cvs		cvs20050315
  %define release 	0r1
%endif
%define xsa		x11-ssh-askpass		
%define askpass		%{xsa}-1.2.4.1

# OpenSSH privilege separation requires a user & group ID
%define sshd_uid    67
%define sshd_gid    67

Name        	: openssh
Version     	: %{version}%{cvs}
Release     	: %{release}
Group       	: System/Network

Summary     	: OpenSSH free Secure Shell (SSH) implementation.
Summary(de) 	: OpenSSH - freie Implementation der Secure Shell (SSH).
Summary(es) 	: OpenSSH implementación libre de Secure Shell (SSH).
Summary(fr) 	: Implémentation libre du shell sécurisé OpenSSH (SSH).
Summary(it) 	: Implementazione gratuita OpenSSH della Secure Shell.
Summary(pt) 	: Implementação livre OpenSSH do protocolo 'Secure Shell' (SSH).
Summary(pt_BR) 	: Implementação livre OpenSSH do protocolo Secure Shell (SSH).

Copyright   	: BSD
Packager    	: Raymund Will <ray@caldera.de>
URL         	: http://www.openssh.com/

Obsoletes   	: ssh, ssh-clients, openssh-clients

BuildRoot   	: /tmp/%{name}-%{version}
BuildRequires	: XFree86-imake

# %{use_stable}==1:	ftp://ftp.openbsd.org/pub/OpenBSD/OpenSSH/portable
# %{use_stable}==0:	:pserver:cvs@bass.directhit.com:/cvs/openssh_cvs
Source0: see-above:/.../openssh-%{version}.tar.gz
%if %{use_stable}
Source1: see-above:/.../openssh-%{version}.tar.gz.asc
%endif
Source2: http://www.jmknoble.net/software/%{xsa}/%{askpass}.tar.gz
Source3: http://www.openssh.com/faq.html

%Package server
Group       	: System/Network
Requires    	: openssh = %{version}
Obsoletes   	: ssh-server

Summary     	: OpenSSH Secure Shell protocol server (sshd).
Summary(de) 	: OpenSSH Secure Shell Protocol-Server (sshd).
Summary(es) 	: Servidor del protocolo OpenSSH Secure Shell (sshd).
Summary(fr) 	: Serveur de protocole du shell sécurisé OpenSSH (sshd).
Summary(it) 	: Server OpenSSH per il protocollo Secure Shell (sshd).
Summary(pt) 	: Servidor do protocolo 'Secure Shell' OpenSSH (sshd).
Summary(pt_BR) 	: Servidor do protocolo Secure Shell OpenSSH (sshd).


%Package askpass
Group       	: System/Network
Requires    	: openssh = %{version}
URL       	: http://www.jmknoble.net/software/x11-ssh-askpass/
Obsoletes   	: ssh-extras

Summary     	: OpenSSH X11 pass-phrase dialog.
Summary(de) 	: OpenSSH X11 Passwort-Dialog.
Summary(es) 	: Aplicación de petición de frase clave OpenSSH X11.
Summary(fr) 	: Dialogue pass-phrase X11 d'OpenSSH.
Summary(it) 	: Finestra di dialogo X11 per la frase segreta di OpenSSH.
Summary(pt) 	: Diálogo de pedido de senha para X11 do OpenSSH.
Summary(pt_BR) 	: Diálogo de pedido de senha para X11 do OpenSSH.


%Description
OpenSSH (Secure Shell) provides access to a remote system. It replaces
telnet, rlogin,  rexec, and rsh, and provides secure encrypted 
communications between two untrusted hosts over an insecure network.  
X11 connections and arbitrary TCP/IP ports can also be forwarded over 
the secure channel.

%Description -l de
OpenSSH (Secure Shell) stellt den Zugang zu anderen Rechnern her. Es ersetzt
telnet, rlogin, rexec und rsh und stellt eine sichere, verschlüsselte
Verbindung zwischen zwei nicht vertrauenswürdigen Hosts über eine unsicheres
Netzwerk her. X11 Verbindungen und beliebige andere TCP/IP Ports können ebenso
über den sicheren Channel weitergeleitet werden.

%Description -l es
OpenSSH (Secure Shell) proporciona acceso a sistemas remotos. Reemplaza a
telnet, rlogin, rexec, y rsh, y proporciona comunicaciones seguras encriptadas
entre dos equipos entre los que no se ha establecido confianza a través de una
red insegura. Las conexiones X11 y puertos TCP/IP arbitrarios también pueden
ser canalizadas sobre el canal seguro.

%Description -l fr
OpenSSH (Secure Shell) fournit un accès à un système distant. Il remplace
telnet, rlogin, rexec et rsh, tout en assurant des communications cryptées
securisées entre deux hôtes non fiabilisés sur un réseau non sécurisé. Des
connexions X11 et des ports TCP/IP arbitraires peuvent également être
transmis sur le canal sécurisé.

%Description -l it
OpenSSH (Secure Shell) fornisce l'accesso ad un sistema remoto.
Sostituisce telnet, rlogin, rexec, e rsh, e fornisce comunicazioni sicure
e crittate tra due host non fidati su una rete non sicura. Le connessioni
X11 ad una porta TCP/IP arbitraria possono essere inoltrate attraverso
un canale sicuro.

%Description -l pt
OpenSSH (Secure Shell) fornece acesso a um sistema remoto. Substitui o
telnet, rlogin, rexec, e o rsh e fornece comunicações seguras e cifradas
entre duas máquinas sem confiança mútua sobre uma rede insegura.
Ligações X11 e portos TCP/IP arbitrários também poder ser reenviados
pelo canal seguro.

%Description -l pt_BR
O OpenSSH (Secure Shell) fornece acesso a um sistema remoto. Substitui o
telnet, rlogin, rexec, e o rsh e fornece comunicações seguras e criptografadas
entre duas máquinas sem confiança mútua sobre uma rede insegura.
Ligações X11 e portas TCP/IP arbitrárias também podem ser reenviadas
pelo canal seguro.

%Description server
This package installs the sshd, the server portion of OpenSSH. 

%Description -l de server
Dieses Paket installiert den sshd, den Server-Teil der OpenSSH.

%Description -l es server
Este paquete instala sshd, la parte servidor de OpenSSH.

%Description -l fr server
Ce paquetage installe le 'sshd', partie serveur de OpenSSH.

%Description -l it server
Questo pacchetto installa sshd, il server di OpenSSH.

%Description -l pt server
Este pacote intala o sshd, o servidor do OpenSSH.

%Description -l pt_BR server
Este pacote intala o sshd, o servidor do OpenSSH.

%Description askpass
This package contains an X11-based pass-phrase dialog used per
default by ssh-add(1). It is based on %{askpass}
by Jim Knoble <jmknoble@pobox.com>.


%Prep
%setup %([ -z "%{cvs}" ] || echo "-n %{name}_cvs") -a2
%if ! %{use_stable}
  autoreconf
%endif


%Build
CFLAGS="$RPM_OPT_FLAGS" \
%configure \
            --with-pam \
            --with-tcp-wrappers \
	    --with-privsep-path=%{_var}/empty/sshd \
	    #leave this line for easy edits.

%__make

cd %{askpass}
%configure \
	    #leave this line for easy edits.

xmkmf
%__make includes
%__make


%Install
[ %{buildroot} != "/" ] && rm -rf %{buildroot}

make install DESTDIR=%{buildroot}
%makeinstall -C %{askpass} \
    BINDIR=%{_libexecdir} \
    MANPATH=%{_mandir} \
    DESTDIR=%{buildroot}

# OpenLinux specific configuration
mkdir -p %{buildroot}{/etc/pam.d,%{SVIcdir},%{SVIdir}}
mkdir -p %{buildroot}%{_var}/empty/sshd

# enabling X11 forwarding on the server is convenient and okay,
# on the client side it's a potential security risk!
%__perl -pi -e 's:#X11Forwarding no:X11Forwarding yes:g' \
    %{buildroot}%{_sysconfdir}/sshd_config

%if %{no_root_login}
%__perl -pi -e 's:#PermitRootLogin yes:PermitRootLogin no:g' \
    %{buildroot}%{_sysconfdir}/sshd_config
%endif

install -m644 contrib/caldera/sshd.pam %{buildroot}/etc/pam.d/sshd
# FIXME: disabled, find out why this doesn't work with nis
%__perl -pi -e 's:(.*pam_limits.*):#$1:' \
    %{buildroot}/etc/pam.d/sshd

install -m 0755 contrib/caldera/sshd.init %{buildroot}%{SVIdir}/sshd

# the last one is needless, but more future-proof
find %{buildroot}%{SVIdir} -type f -exec \
    %__perl -pi -e 's:\@SVIdir\@:%{SVIdir}:g;\
		    s:\@sysconfdir\@:%{_sysconfdir}:g; \
		    s:/usr/sbin:%{_sbindir}:g'\
    \{\} \;

cat <<-EoD > %{buildroot}%{SVIcdir}/sshd
	IDENT=sshd
	DESCRIPTIVE="OpenSSH secure shell daemon"
	# This service will be marked as 'skipped' on boot if there
	# is no host key. Use ssh-host-keygen to generate one
	ONBOOT="yes"
	OPTIONS=""
EoD

SKG=%{buildroot}%{_sbindir}/ssh-host-keygen
install -m 0755 contrib/caldera/ssh-host-keygen $SKG
# Fix up some path names in the keygen toy^Hol
    %__perl -pi -e 's:\@sysconfdir\@:%{_sysconfdir}:g; \
		    s:\@sshkeygen\@:%{_bindir}/ssh-keygen:g' \
	%{buildroot}%{_sbindir}/ssh-host-keygen

# This looks terrible.  Expect it to change.
# install remaining docs
DocD="%{buildroot}%{_defaultdocdir}/%{name}-%{version}"
mkdir -p $DocD/%{askpass}
cp -a CREDITS ChangeLog LICENCE OVERVIEW README* TODO PROTOCOL* $DocD
install -p -m 0444 %{SOURCE3}  $DocD/faq.html
cp -a %{askpass}/{README,ChangeLog,TODO,SshAskpass*.ad}  $DocD/%{askpass}
%if %{use_stable}
  cp -p %{askpass}/%{xsa}.man $DocD/%{askpass}/%{xsa}.1
%else
  cp -p %{askpass}/%{xsa}.man %{buildroot}%{_mandir}man1/%{xsa}.1
  ln -s  %{xsa}.1 %{buildroot}%{_mandir}man1/ssh-askpass.1
%endif

find %{buildroot}%{_mandir} -type f -not -name	'*.gz' -print0 | xargs -0r %__gzip -9nf
rm %{buildroot}%{_mandir}/man1/slogin.1 && \
    ln -s %{_mandir}/man1/ssh.1.gz \
    %{buildroot}%{_mandir}/man1/slogin.1.gz


%Clean
#%{rmDESTDIR}
[ %{buildroot} != "/" ] && rm -rf %{buildroot}

%Post
# Generate host key when none is present to get up and running,
# both client and server require this for host-based auth!
# ssh-host-keygen checks for existing keys.
/usr/sbin/ssh-host-keygen
: # to protect the rpm database

%pre server
%{_sbindir}/groupadd -g %{sshd_gid} sshd 2>/dev/null || :
%{_sbindir}/useradd -d /var/empty/sshd -s /bin/false -u %{sshd_uid} \
	-c "SSH Daemon virtual user" -g sshd sshd 2>/dev/null || :
: # to protect the rpm database

%Post server
if [ -x %{LSBinit}-install ]; then
  %{LSBinit}-install sshd
else
  lisa --SysV-init install sshd S55 2:3:4:5 K45 0:1:6
fi

! %{SVIdir}/sshd status || %{SVIdir}/sshd restart
: # to protect the rpm database


%PreUn server
[ "$1" = 0 ] || exit 0
! %{SVIdir}/sshd status || %{SVIdir}/sshd stop
if [ -x %{LSBinit}-remove ]; then
  %{LSBinit}-remove sshd
else
  lisa --SysV-init remove sshd $1
fi
: # to protect the rpm database

%Files 
%defattr(-,root,root)
%dir %{_sysconfdir}
%config %{_sysconfdir}/ssh_config
%{_bindir}/scp
%{_bindir}/sftp
%{_bindir}/ssh
%{_bindir}/slogin
%{_bindir}/ssh-add
%attr(2755,root,nobody) %{_bindir}/ssh-agent
%{_bindir}/ssh-keygen
%{_bindir}/ssh-keyscan
%dir %{_libexecdir}
%attr(4711,root,root) %{_libexecdir}/ssh-keysign
%{_libexecdir}/ssh-pkcs11-helper
%{_sbindir}/ssh-host-keygen
%dir %{_defaultdocdir}/%{name}-%{version}
%{_defaultdocdir}/%{name}-%{version}/CREDITS
%{_defaultdocdir}/%{name}-%{version}/ChangeLog
%{_defaultdocdir}/%{name}-%{version}/LICENCE
%{_defaultdocdir}/%{name}-%{version}/OVERVIEW
%{_defaultdocdir}/%{name}-%{version}/README*
%{_defaultdocdir}/%{name}-%{version}/TODO
%{_defaultdocdir}/%{name}-%{version}/faq.html
%{_mandir}/man1/*
%{_mandir}/man8/ssh-keysign.8.gz
%{_mandir}/man8/ssh-pkcs11-helper.8.gz
%{_mandir}/man5/ssh_config.5.gz
 
%Files server
%defattr(-,root,root)
%dir %{_var}/empty/sshd
%config %{SVIdir}/sshd
%config /etc/pam.d/sshd
%config %{_sysconfdir}/moduli
%config %{_sysconfdir}/sshd_config
%config %{SVIcdir}/sshd
%{_libexecdir}/sftp-server
%{_sbindir}/sshd
%{_mandir}/man5/moduli.5.gz
%{_mandir}/man5/sshd_config.5.gz
%{_mandir}/man8/sftp-server.8.gz
%{_mandir}/man8/sshd.8.gz
 
%Files askpass
%defattr(-,root,root)
%{_libexecdir}/ssh-askpass
%{_libexecdir}/x11-ssh-askpass
%{_defaultdocdir}/%{name}-%{version}/%{askpass}
 

%ChangeLog
* Tue Jan 18 2011 Tim Rice <tim@multitalents.net>
- Use CFLAGS from Makefile instead of RPM so build completes.
- Signatures were changed to .asc since 4.1p1.

* Mon Jan 01 1998 ...
Template Version: 1.31

$Id: openssh.spec,v 1.79.2.1 2013/05/10 06:02:21 djm Exp $
