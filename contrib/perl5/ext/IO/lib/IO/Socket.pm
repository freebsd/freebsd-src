# IO::Socket.pm
#
# Copyright (c) 1996 Graham Barr <Graham.Barr@tiuk.ti.com>. All rights
# reserved. This program is free software; you can redistribute it and/or
# modify it under the same terms as Perl itself.

package IO::Socket;

=head1 NAME

IO::Socket - Object interface to socket communications

=head1 SYNOPSIS

    use IO::Socket;

=head1 DESCRIPTION

C<IO::Socket> provides an object interface to creating and using sockets. It
is built upon the L<IO::Handle> interface and inherits all the methods defined
by L<IO::Handle>.

C<IO::Socket> only defines methods for those operations which are common to all
types of socket. Operations which are specified to a socket in a particular 
domain have methods defined in sub classes of C<IO::Socket>

C<IO::Socket> will export all functions (and constants) defined by L<Socket>.

=head1 CONSTRUCTOR

=over 4

=item new ( [ARGS] )

Creates an C<IO::Socket>, which is a reference to a
newly created symbol (see the C<Symbol> package). C<new>
optionally takes arguments, these arguments are in key-value pairs.
C<new> only looks for one key C<Domain> which tells new which domain
the socket will be in. All other arguments will be passed to the
configuration method of the package for that domain, See below.

C<IO::Socket>s will be in autoflush mode after creation.  Note that
versions of IO::Socket prior to 1.1603 (as shipped with Perl 5.004_04)
did not do this.   So if you need backward compatibility, you should
set autoflush explicitly.

=back

=head1 METHODS

See L<perlfunc> for complete descriptions of each of the following
supported C<IO::Socket> methods, which are just front ends for the
corresponding built-in functions:

    socket
    socketpair
    bind
    listen
    accept
    send
    recv
    peername (getpeername)
    sockname (getsockname)

Some methods take slightly different arguments to those defined in L<perlfunc>
in attempt to make the interface more flexible. These are

=over 4

=item accept([PKG])

perform the system call C<accept> on the socket and return a new object. The
new object will be created in the same class as the listen socket, unless
C<PKG> is specified. This object can be used to communicate with the client
that was trying to connect. In a scalar context the new socket is returned,
or undef upon failure. In an array context a two-element array is returned
containing the new socket and the peer address, the list will
be empty upon failure.

Additional methods that are provided are

=item timeout([VAL])

Set or get the timeout value associated with this socket. If called without
any arguments then the current setting is returned. If called with an argument
the current setting is changed and the previous value returned.

=item sockopt(OPT [, VAL])

Unified method to both set and get options in the SOL_SOCKET level. If called
with one argument then getsockopt is called, otherwise setsockopt is called.

=item sockdomain

Returns the numerical number for the socket domain type. For example, for
a AF_INET socket the value of &AF_INET will be returned.

=item socktype

Returns the numerical number for the socket type. For example, for
a SOCK_STREAM socket the value of &SOCK_STREAM will be returned.

=item protocol

Returns the numerical number for the protocol being used on the socket, if
known. If the protocol is unknown, as with an AF_UNIX socket, zero
is returned.

=back

=cut


require 5.000;

use Config;
use IO::Handle;
use Socket 1.3;
use Carp;
use strict;
use vars qw(@ISA $VERSION);
use Exporter;

@ISA = qw(IO::Handle);

$VERSION = "1.1603";

sub import {
    my $pkg = shift;
    my $callpkg = caller;
    Exporter::export 'Socket', $callpkg, @_;
}

sub new {
    my($class,%arg) = @_;
    my $fh = $class->SUPER::new();
    $fh->autoflush;

    ${*$fh}{'io_socket_timeout'} = delete $arg{Timeout};

    return scalar(%arg) ? $fh->configure(\%arg)
			: $fh;
}

my @domain2pkg = ();

sub register_domain {
    my($p,$d) = @_;
    $domain2pkg[$d] = $p;
}

sub configure {
    my($fh,$arg) = @_;
    my $domain = delete $arg->{Domain};

    croak 'IO::Socket: Cannot configure a generic socket'
	unless defined $domain;

    croak "IO::Socket: Unsupported socket domain"
	unless defined $domain2pkg[$domain];

    croak "IO::Socket: Cannot configure socket in domain '$domain'"
	unless ref($fh) eq "IO::Socket";

    bless($fh, $domain2pkg[$domain]);
    $fh->configure($arg);
}

sub socket {
    @_ == 4 or croak 'usage: $fh->socket(DOMAIN, TYPE, PROTOCOL)';
    my($fh,$domain,$type,$protocol) = @_;

    socket($fh,$domain,$type,$protocol) or
    	return undef;

    ${*$fh}{'io_socket_domain'} = $domain;
    ${*$fh}{'io_socket_type'}   = $type;
    ${*$fh}{'io_socket_proto'}  = $protocol;

    $fh;
}

sub socketpair {
    @_ == 4 || croak 'usage: IO::Socket->pair(DOMAIN, TYPE, PROTOCOL)';
    my($class,$domain,$type,$protocol) = @_;
    my $fh1 = $class->new();
    my $fh2 = $class->new();

    socketpair($fh1,$fh2,$domain,$type,$protocol) or
    	return ();

    ${*$fh1}{'io_socket_type'}  = ${*$fh2}{'io_socket_type'}  = $type;
    ${*$fh1}{'io_socket_proto'} = ${*$fh2}{'io_socket_proto'} = $protocol;

    ($fh1,$fh2);
}

sub connect {
    @_ == 2 || @_ == 3 or croak 'usage: $fh->connect(NAME) or $fh->connect(PORT, ADDR)';
    my $fh = shift;
    my $addr = @_ == 1 ? shift : sockaddr_in(@_);
    my $timeout = ${*$fh}{'io_socket_timeout'};
    local($SIG{ALRM}) = $timeout ? sub { undef $fh; }
				 : $SIG{ALRM} || 'DEFAULT';

     eval {
    	croak 'connect: Bad address'
    	    if(@_ == 2 && !defined $_[1]);

    	if($timeout) {
    	    defined $Config{d_alarm} && defined alarm($timeout) or
    	    	$timeout = 0;
    	}

	my $ok = connect($fh, $addr);

    	alarm(0)
    	    if($timeout);

	croak "connect: timeout"
	    unless defined $fh;

	undef $fh unless $ok;
    };

    $fh;
}

sub bind {
    @_ == 2 || @_ == 3 or croak 'usage: $fh->bind(NAME) or $fh->bind(PORT, ADDR)';
    my $fh = shift;
    my $addr = @_ == 1 ? shift : sockaddr_in(@_);

    return bind($fh, $addr) ? $fh
			    : undef;
}

sub listen {
    @_ >= 1 && @_ <= 2 or croak 'usage: $fh->listen([QUEUE])';
    my($fh,$queue) = @_;
    $queue = 5
	unless $queue && $queue > 0;

    return listen($fh, $queue) ? $fh
			       : undef;
}

sub accept {
    @_ == 1 || @_ == 2 or croak 'usage $fh->accept([PKG])';
    my $fh = shift;
    my $pkg = shift || $fh;
    my $timeout = ${*$fh}{'io_socket_timeout'};
    my $new = $pkg->new(Timeout => $timeout);
    my $peer = undef;

    eval {
    	if($timeout) {
    	    my $fdset = "";
    	    vec($fdset, $fh->fileno,1) = 1;
    	    croak "accept: timeout"
    	    	unless select($fdset,undef,undef,$timeout);
    	}
    	$peer = accept($new,$fh);
    };

    return wantarray ? defined $peer ? ($new, $peer)
    	    	    	    	     : () 
    	      	     : defined $peer ? $new
    	    	    	    	     : undef;
}

sub sockname {
    @_ == 1 or croak 'usage: $fh->sockname()';
    getsockname($_[0]);
}

sub peername {
    @_ == 1 or croak 'usage: $fh->peername()';
    my($fh) = @_;
    getpeername($fh)
      || ${*$fh}{'io_socket_peername'}
      || undef;
}

sub send {
    @_ >= 2 && @_ <= 4 or croak 'usage: $fh->send(BUF, [FLAGS, [TO]])';
    my $fh    = $_[0];
    my $flags = $_[2] || 0;
    my $peer  = $_[3] || $fh->peername;

    croak 'send: Cannot determine peer address'
	 unless($peer);

    my $r = defined(getpeername($fh))
	? send($fh, $_[1], $flags)
	: send($fh, $_[1], $flags, $peer);

    # remember who we send to, if it was sucessful
    ${*$fh}{'io_socket_peername'} = $peer
	if(@_ == 4 && defined $r);

    $r;
}

sub recv {
    @_ == 3 || @_ == 4 or croak 'usage: $fh->recv(BUF, LEN [, FLAGS])';
    my $sock  = $_[0];
    my $len   = $_[2];
    my $flags = $_[3] || 0;

    # remember who we recv'd from
    ${*$sock}{'io_socket_peername'} = recv($sock, $_[1]='', $len, $flags);
}


sub setsockopt {
    @_ == 4 or croak '$fh->setsockopt(LEVEL, OPTNAME)';
    setsockopt($_[0],$_[1],$_[2],$_[3]);
}

my $intsize = length(pack("i",0));

sub getsockopt {
    @_ == 3 or croak '$fh->getsockopt(LEVEL, OPTNAME)';
    my $r = getsockopt($_[0],$_[1],$_[2]);
    # Just a guess
    $r = unpack("i", $r)
	if(defined $r && length($r) == $intsize);
    $r;
}

sub sockopt {
    my $fh = shift;
    @_ == 1 ? $fh->getsockopt(SOL_SOCKET,@_)
	    : $fh->setsockopt(SOL_SOCKET,@_);
}

sub timeout {
    @_ == 1 || @_ == 2 or croak 'usage: $fh->timeout([VALUE])';
    my($fh,$val) = @_;
    my $r = ${*$fh}{'io_socket_timeout'} || undef;

    ${*$fh}{'io_socket_timeout'} = 0 + $val
	if(@_ == 2);

    $r;
}

sub sockdomain {
    @_ == 1 or croak 'usage: $fh->sockdomain()';
    my $fh = shift;
    ${*$fh}{'io_socket_domain'};
}

sub socktype {
    @_ == 1 or croak 'usage: $fh->socktype()';
    my $fh = shift;
    ${*$fh}{'io_socket_type'}
}

sub protocol {
    @_ == 1 or croak 'usage: $fh->protocol()';
    my($fh) = @_;
    ${*$fh}{'io_socket_protocol'};
}

=head1 SUB-CLASSES

=cut

##
## AF_INET
##

package IO::Socket::INET;

use strict;
use vars qw(@ISA);
use Socket;
use Carp;
use Exporter;

@ISA = qw(IO::Socket);

IO::Socket::INET->register_domain( AF_INET );

my %socket_type = ( tcp => SOCK_STREAM,
		    udp => SOCK_DGRAM,
		    icmp => SOCK_RAW,
		  );

=head2 IO::Socket::INET

C<IO::Socket::INET> provides a constructor to create an AF_INET domain socket
and some related methods. The constructor can take the following options

    PeerAddr	Remote host address          <hostname>[:<port>]
    PeerPort	Remote port or service       <service>[(<no>)] | <no>
    LocalAddr	Local host bind	address      hostname[:port]
    LocalPort	Local host bind	port         <service>[(<no>)] | <no>
    Proto	Protocol name (or number)    "tcp" | "udp" | ...
    Type	Socket type                  SOCK_STREAM | SOCK_DGRAM | ...
    Listen	Queue size for listen
    Reuse	Set SO_REUSEADDR before binding
    Timeout	Timeout	value for various operations


If C<Listen> is defined then a listen socket is created, else if the
socket type, which is derived from the protocol, is SOCK_STREAM then
connect() is called.

The C<PeerAddr> can be a hostname or the IP-address on the
"xx.xx.xx.xx" form.  The C<PeerPort> can be a number or a symbolic
service name.  The service name might be followed by a number in
parenthesis which is used if the service is not known by the system.
The C<PeerPort> specification can also be embedded in the C<PeerAddr>
by preceding it with a ":".

If C<Proto> is not given and you specify a symbolic C<PeerPort> port,
then the constructor will try to derive C<Proto> from the service
name.  As a last resort C<Proto> "tcp" is assumed.  The C<Type>
parameter will be deduced from C<Proto> if not specified.

If the constructor is only passed a single argument, it is assumed to
be a C<PeerAddr> specification.

Examples:

   $sock = IO::Socket::INET->new(PeerAddr => 'www.perl.org',
                                 PeerPort => 'http(80)',
                                 Proto    => 'tcp');

   $sock = IO::Socket::INET->new(PeerAddr => 'localhost:smtp(25)');

   $sock = IO::Socket::INET->new(Listen    => 5,
                                 LocalAddr => 'localhost',
                                 LocalPort => 9000,
                                 Proto     => 'tcp');

   $sock = IO::Socket::INET->new('127.0.0.1:25');


=head2 METHODS

=over 4

=item sockaddr ()

Return the address part of the sockaddr structure for the socket

=item sockport ()

Return the port number that the socket is using on the local host

=item sockhost ()

Return the address part of the sockaddr structure for the socket in a
text form xx.xx.xx.xx

=item peeraddr ()

Return the address part of the sockaddr structure for the socket on
the peer host

=item peerport ()

Return the port number for the socket on the peer host.

=item peerhost ()

Return the address part of the sockaddr structure for the socket on the
peer host in a text form xx.xx.xx.xx

=back

=cut

sub new
{
  my $class = shift;
  unshift(@_, "PeerAddr") if @_ == 1;
  return $class->SUPER::new(@_);
}

sub _sock_info {
  my($addr,$port,$proto) = @_;
  my @proto = ();
  my @serv = ();

  $port = $1
	if(defined $addr && $addr =~ s,:([\w\(\)/]+)$,,);

  if(defined $proto) {
    @proto = $proto =~ m,\D, ? getprotobyname($proto)
			     : getprotobynumber($proto);

    $proto = $proto[2] || undef;
  }

  if(defined $port) {
    $port =~ s,\((\d+)\)$,,;

    my $defport = $1 || undef;
    my $pnum = ($port =~ m,^(\d+)$,)[0];

    @serv= getservbyname($port, $proto[0] || "")
	if($port =~ m,\D,);

    $port = $pnum || $serv[2] || $defport || undef;

    $proto = (getprotobyname($serv[3]))[2] || undef
	if @serv && !$proto;
  }

 return ($addr || undef,
	 $port || undef,
	 $proto || undef
	);
}

sub _error {
    my $fh = shift;
    $@ = join("",ref($fh),": ",@_);
    carp $@ if $^W;
    close($fh)
	if(defined fileno($fh));
    return undef;
}

sub configure {
    my($fh,$arg) = @_;
    my($lport,$rport,$laddr,$raddr,$proto,$type);


    ($laddr,$lport,$proto) = _sock_info($arg->{LocalAddr},
					$arg->{LocalPort},
					$arg->{Proto});

    $laddr = defined $laddr ? inet_aton($laddr)
			    : INADDR_ANY;

    return _error($fh,"Bad hostname '",$arg->{LocalAddr},"'")
	unless(defined $laddr);

    unless(exists $arg->{Listen}) {
	($raddr,$rport,$proto) = _sock_info($arg->{PeerAddr},
					    $arg->{PeerPort},
					    $proto);
    }

    if(defined $raddr) {
	$raddr = inet_aton($raddr);
	return _error($fh,"Bad hostname '",$arg->{PeerAddr},"'")
		unless(defined $raddr);
    }

    $proto ||= (getprotobyname "tcp")[2];
    return _error($fh,'Cannot determine protocol')
	unless($proto);

    my $pname = (getprotobynumber($proto))[0];
    $type = $arg->{Type} || $socket_type{$pname};

    $fh->socket(AF_INET, $type, $proto) or
	return _error($fh,"$!");

    if ($arg->{Reuse}) {
	$fh->sockopt(SO_REUSEADDR,1) or
		return _error($fh);
    }

    $fh->bind($lport || 0, $laddr) or
	return _error($fh,"$!");

    if(exists $arg->{Listen}) {
	$fh->listen($arg->{Listen} || 5) or
	    return _error($fh,"$!");
    }
    else {
	return _error($fh,'Cannot determine remote port')
		unless($rport || $type == SOCK_DGRAM || $type == SOCK_RAW);

	if($type == SOCK_STREAM || defined $raddr) {
	    return _error($fh,'Bad peer address')
	    	unless(defined $raddr);

	    $fh->connect($rport,$raddr) or
		return _error($fh,"$!");
	}
    }

    $fh;
}

sub sockaddr {
    @_ == 1 or croak 'usage: $fh->sockaddr()';
    my($fh) = @_;
    (sockaddr_in($fh->sockname))[1];
}

sub sockport {
    @_ == 1 or croak 'usage: $fh->sockport()';
    my($fh) = @_;
    (sockaddr_in($fh->sockname))[0];
}

sub sockhost {
    @_ == 1 or croak 'usage: $fh->sockhost()';
    my($fh) = @_;
    inet_ntoa($fh->sockaddr);
}

sub peeraddr {
    @_ == 1 or croak 'usage: $fh->peeraddr()';
    my($fh) = @_;
    (sockaddr_in($fh->peername))[1];
}

sub peerport {
    @_ == 1 or croak 'usage: $fh->peerport()';
    my($fh) = @_;
    (sockaddr_in($fh->peername))[0];
}

sub peerhost {
    @_ == 1 or croak 'usage: $fh->peerhost()';
    my($fh) = @_;
    inet_ntoa($fh->peeraddr);
}

##
## AF_UNIX
##

package IO::Socket::UNIX;

use strict;
use vars qw(@ISA $VERSION);
use Socket;
use Carp;
use Exporter;

@ISA = qw(IO::Socket);

IO::Socket::UNIX->register_domain( AF_UNIX );

=head2 IO::Socket::UNIX

C<IO::Socket::UNIX> provides a constructor to create an AF_UNIX domain socket
and some related methods. The constructor can take the following options

    Type    	Type of socket (eg SOCK_STREAM or SOCK_DGRAM)
    Local   	Path to local fifo
    Peer    	Path to peer fifo
    Listen  	Create a listen socket

=head2 METHODS

=over 4

=item hostpath()

Returns the pathname to the fifo at the local end

=item peerpath()

Returns the pathname to the fifo at the peer end

=back

=cut

sub configure {
    my($fh,$arg) = @_;
    my($bport,$cport);

    my $type = $arg->{Type} || SOCK_STREAM;

    $fh->socket(AF_UNIX, $type, 0) or
	return undef;

    if(exists $arg->{Local}) {
	my $addr = sockaddr_un($arg->{Local});
	$fh->bind($addr) or
	    return undef;
    }
    if(exists $arg->{Listen}) {
	$fh->listen($arg->{Listen} || 5) or
	    return undef;
    }
    elsif(exists $arg->{Peer}) {
	my $addr = sockaddr_un($arg->{Peer});
	$fh->connect($addr) or
	    return undef;
    }

    $fh;
}

sub hostpath {
    @_ == 1 or croak 'usage: $fh->hostpath()';
    my $n = $_[0]->sockname || return undef;
    (sockaddr_un($n))[0];
}

sub peerpath {
    @_ == 1 or croak 'usage: $fh->peerpath()';
    my $n = $_[0]->peername || return undef;
    (sockaddr_un($n))[0];
}

=head1 SEE ALSO

L<Socket>, L<IO::Handle>

=head1 AUTHOR

Graham Barr E<lt>F<Graham.Barr@tiuk.ti.com>E<gt>

=head1 COPYRIGHT

Copyright (c) 1996 Graham Barr. All rights reserved. This program is free
software; you can redistribute it and/or modify it under the same terms
as Perl itself.

=cut

1; # Keep require happy
