# IPC::Msg.pm
#
# Copyright (c) 1997 Graham Barr <gbarr@pobox.com>. All rights reserved.
# This program is free software; you can redistribute it and/or
# modify it under the same terms as Perl itself.

package IPC::Msg;

use IPC::SysV qw(IPC_STAT IPC_SET IPC_RMID);
use strict;
use vars qw($VERSION);
use Carp;

$VERSION = "1.00";

{
    package IPC::Msg::stat;

    use Class::Struct qw(struct);

    struct 'IPC::Msg::stat' => [
	uid	=> '$',
	gid	=> '$',
	cuid	=> '$',
	cgid	=> '$',
	mode	=> '$',
	qnum	=> '$',
	qbytes	=> '$',
	lspid	=> '$',
	lrpid	=> '$',
	stime	=> '$',
	rtime	=> '$',
	ctime	=> '$',
    ];
}

sub new {
    @_ == 3 || croak 'new IPC::Msg ( KEY , FLAGS )';
    my $class = shift;

    my $id = msgget($_[0],$_[1]);

    defined($id)
	? bless \$id, $class
	: undef;
}

sub id {
    my $self = shift;
    $$self;
}

sub stat {
    my $self = shift;
    my $data = "";
    msgctl($$self,IPC_STAT,$data) or
	return undef;
    IPC::Msg::stat->new->unpack($data);
}

sub set {
    my $self = shift;
    my $ds;

    if(@_ == 1) {
	$ds = shift;
    }
    else {
	croak 'Bad arg count' if @_ % 2;
	my %arg = @_;
	my $ds = $self->stat
		or return undef;
	my($key,$val);
	$ds->$key($val)
	    while(($key,$val) = each %arg);
    }

    msgctl($$self,IPC_SET,$ds->pack);
}

sub remove {
    my $self = shift;
    (msgctl($$self,IPC_RMID,0), undef $$self)[0];
}

sub rcv {
    @_ <= 5 && @_ >= 3 or croak '$msg->rcv( BUF, LEN, TYPE, FLAGS )';
    my $self = shift;
    my $buf = "";
    msgrcv($$self,$buf,$_[1],$_[2] || 0, $_[3] || 0) or
	return;
    my $type;
    ($type,$_[0]) = unpack("L a*",$buf);
    $type;
}

sub snd {
    @_ <= 4 && @_ >= 3 or  croak '$msg->snd( TYPE, BUF, FLAGS )';
    my $self = shift;
    msgsnd($$self,pack("L a*",$_[0],$_[1]), $_[2] || 0);
}


1;

__END__

=head1 NAME

IPC::Msg - SysV Msg IPC object class

=head1 SYNOPSIS

    use IPC::SysV qw(IPC_PRIVATE S_IRWXU S_IRWXG S_IRWXO);
    use IPC::Msg;
    
    $msg = new IPC::Msg(IPC_PRIVATE, S_IRWXU | S_IRWXG | S_IRWXO);
    
    $msg->snd(pack("L a*",$msgtype,$msg));
    
    $msg->rcv($buf,256);
    
    $ds = $msg->stat;
    
    $msg->remove;

=head1 DESCRIPTION

=head1 METHODS

=over 4

=item new ( KEY , FLAGS )

Creates a new message queue associated with C<KEY>. A new queue is
created if

=over 4

=item *

C<KEY> is equal to C<IPC_PRIVATE>

=item *

C<KEY> does not already  have  a  message queue
associated with it, and C<I<FLAGS> & IPC_CREAT> is true.

=back

On creation of a new message queue C<FLAGS> is used to set the
permissions.

=item id

Returns the system message queue identifier.

=item rcv ( BUF, LEN [, TYPE [, FLAGS ]] )

Read a message from the queue. Returns the type of the message read. See
L<msgrcv>

=item remove

Remove and destroy the message queue from the system.

=item set ( STAT )

=item set ( NAME => VALUE [, NAME => VALUE ...] )

C<set> will set the following values of the C<stat> structure associated
with the message queue.

    uid
    gid
    mode (oly the permission bits)
    qbytes

C<set> accepts either a stat object, as returned by the C<stat> method,
or a list of I<name>-I<value> pairs.

=item snd ( TYPE, MSG [, FLAGS ] )

Place a message on the queue with the data from C<MSG> and with type C<TYPE>.
See L<msgsnd>.

=item stat

Returns an object of type C<IPC::Msg::stat> which is a sub-class of
C<Class::Struct>. It provides the following fields. For a description
of these fields see you system documentation.

    uid
    gid
    cuid
    cgid
    mode
    qnum
    qbytes
    lspid
    lrpid
    stime
    rtime
    ctime

=back

=head1 SEE ALSO

L<IPC::SysV> L<Class::Struct>

=head1 AUTHOR

Graham Barr <gbarr@pobox.com>

=head1 COPYRIGHT

Copyright (c) 1997 Graham Barr. All rights reserved.
This program is free software; you can redistribute it and/or modify it
under the same terms as Perl itself.

=cut

