package User::pwent;
use strict;

BEGIN { 
    use Exporter   ();
    use vars       qw(@EXPORT @EXPORT_OK %EXPORT_TAGS);
    @EXPORT      = qw(getpwent getpwuid getpwnam getpw);
    @EXPORT_OK   = qw(
			$pw_name   $pw_passwd 	$pw_uid	 
			$pw_gid	   $pw_quota    $pw_comment
			$pw_gecos  $pw_dir	$pw_shell
		   );
    %EXPORT_TAGS = ( FIELDS => [ @EXPORT_OK, @EXPORT ] );
}
use vars      @EXPORT_OK;

# Class::Struct forbids use of @ISA
sub import { goto &Exporter::import }

use Class::Struct qw(struct);
struct 'User::pwent' => [
    name    => '$',
    passwd  => '$',
    uid	    => '$',
    gid	    => '$',
    quota   => '$',
    comment => '$',
    gecos   => '$',
    dir	    => '$',
    shell   => '$',
];

sub populate (@) {
    return unless @_;
    my $pwob = new();

    ( $pw_name,   $pw_passwd,   $pw_uid,  
      $pw_gid,    $pw_quota,    $pw_comment,
      $pw_gecos,  $pw_dir,      $pw_shell,   ) 	= @$pwob = @_;

    return $pwob;
} 

sub getpwent ( ) { populate(CORE::getpwent()) } 
sub getpwnam ($) { populate(CORE::getpwnam(shift)) } 
sub getpwuid ($) { populate(CORE::getpwuid(shift)) } 
sub getpw    ($) { ($_[0] =~ /^\d+/) ? &getpwuid : &getpwnam } 

1;
__END__

=head1 NAME

User::pwent - by-name interface to Perl's built-in getpw*() functions

=head1 SYNOPSIS

 use User::pwent;
 $pw = getpwnam('daemon') or die "No daemon user";
 if ( $pw->uid == 1 && $pw->dir =~ m#^/(bin|tmp)?$# ) {
     print "gid 1 on root dir";
 } 

 use User::pwent qw(:FIELDS);
 getpwnam('daemon') or die "No daemon user";
 if ( $pw_uid == 1 && $pw_dir =~ m#^/(bin|tmp)?$# ) {
     print "gid 1 on root dir";
 } 

 $pw = getpw($whoever);

=head1 DESCRIPTION

This module's default exports override the core getpwent(), getpwuid(),
and getpwnam() functions, replacing them with versions that return
"User::pwent" objects.  This object has methods that return the similarly
named structure field name from the C's passwd structure from F<pwd.h>; 
namely name, passwd, uid, gid, quota, comment, gecos, dir, and shell.

You may also import all the structure fields directly into your namespace
as regular variables using the :FIELDS import tag.  (Note that this still
overrides your core functions.)  Access these fields as
variables named with a preceding C<pw_> in front their method names.
Thus, C<$passwd_obj-E<gt>shell()> corresponds to $pw_shell if you import
the fields.

The getpw() function is a simple front-end that forwards
a numeric argument to getpwuid() and the rest to getpwnam().

To access this functionality without the core overrides,
pass the C<use> an empty import list, and then access
function functions with their full qualified names.
On the other hand, the built-ins are still available
via the C<CORE::> pseudo-package.

=head1 NOTE

While this class is currently implemented using the Class::Struct
module to build a struct-like class, you shouldn't rely upon this.

=head1 AUTHOR

Tom Christiansen
