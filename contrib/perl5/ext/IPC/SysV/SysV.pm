# IPC::SysV.pm
#
# Copyright (c) 1997 Graham Barr <gbarr@pobox.com>. All rights reserved.
# This program is free software; you can redistribute it and/or
# modify it under the same terms as Perl itself.

package IPC::SysV;

use strict;
use vars qw(@ISA @EXPORT @EXPORT_OK %EXPORT_TAGS $VERSION);
use Carp;
use Config;

require Exporter;
@ISA = qw(Exporter);

$VERSION = "1.03";

@EXPORT_OK = qw(
	GETALL GETNCNT GETPID GETVAL GETZCNT

	IPC_ALLOC IPC_CREAT IPC_EXCL IPC_GETACL IPC_LOCKED IPC_M
	IPC_NOERROR IPC_NOWAIT IPC_PRIVATE IPC_R IPC_RMID IPC_SET
	IPC_SETACL IPC_SETLABEL IPC_STAT IPC_W IPC_WANTED

	MSG_FWAIT MSG_LOCKED MSG_MWAIT MSG_NOERROR MSG_QWAIT
	MSG_R MSG_RWAIT MSG_STAT MSG_W MSG_WWAIT

	SEM_A SEM_ALLOC SEM_DEST SEM_ERR SEM_ORDER SEM_R SEM_UNDO

	SETALL SETVAL

	SHMLBA

	SHM_A SHM_CLEAR SHM_COPY SHM_DCACHE SHM_DEST SHM_ECACHE
	SHM_FMAP SHM_ICACHE SHM_INIT SHM_LOCK SHM_LOCKED SHM_MAP
	SHM_NOSWAP SHM_R SHM_RDONLY SHM_REMOVED SHM_RND SHM_SHARE_MMU
	SHM_SHATTR SHM_SIZE SHM_UNLOCK SHM_W

	S_IRUSR S_IWUSR S_IRWXU
	S_IRGRP S_IWGRP S_IRWXG
	S_IROTH S_IWOTH S_IRWXO

	ftok
);

BOOT_XS: {
    # If I inherit DynaLoader then I inherit AutoLoader and I DON'T WANT TO
    require DynaLoader;

    # DynaLoader calls dl_load_flags as a static method.
    *dl_load_flags = DynaLoader->can('dl_load_flags');

    do {
	__PACKAGE__->can('bootstrap') || \&DynaLoader::bootstrap
    }->(__PACKAGE__, $VERSION);
}

1;

__END__

=head1 NAME

IPC::SysV - SysV IPC constants

=head1 SYNOPSIS

    use IPC::SysV qw(IPC_STAT IPC_PRIVATE);

=head1 DESCRIPTION

C<IPC::SysV> defines and conditionally exports all the constants
defined in your system include files which are needed by the SysV
IPC calls.

=item ftok( PATH, ID )

Return a key based on PATH and ID, which can be used as a key for
C<msgget>, C<semget> and C<shmget>. See L<ftok>

=head1 SEE ALSO

L<IPC::Msg>, L<IPC::Semaphore>, L<ftok>

=head1 AUTHORS

Graham Barr <gbarr@pobox.com>
Jarkko Hietaniemi <jhi@iki.fi>

=head1 COPYRIGHT

Copyright (c) 1997 Graham Barr. All rights reserved.
This program is free software; you can redistribute it and/or modify it
under the same terms as Perl itself.

=cut

