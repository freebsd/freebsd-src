# Add explicit link to deb.o to pick up .Perl_deb symbol which is not
# mentioned in perl.exp for earlier cc (xlc) versions in at least
# non DEBUGGING builds
#  Peter Prymmer <pvhp@best.com>

use Config;

if ($^O eq 'aix' && defined($Config{'ccversion'}) && 
    ( $Config{'ccversion'} =~ /^3\.\d/
      # needed for at least these versions:
      # $Config{'ccversion'} eq '3.6.6.0' 
      # $Config{'ccversion'} eq '3.6.4.0' 
      # $Config{'ccversion'} eq '3.1.4.0'  AIX 4.2
      # $Config{'ccversion'} eq '3.1.4.10' AIX 4.2
      # $Config{'ccversion'} eq '3.1.3.3' 
      ||
      $Config{'ccversion'} =~ /^4\.4\.0\.[0-3]/
    )
   ) {
    $self->{OBJECT} .= ' ../../deb$(OBJ_EXT)';
}

