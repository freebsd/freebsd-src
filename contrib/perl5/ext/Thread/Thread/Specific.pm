package Thread::Specific;

=head1 NAME

Thread::Specific - thread-specific keys

=head1 SYNOPSIS

    use Thread::Specific;
    my $k = key_create Thread::Specific;

=head1 DESCRIPTION

C<key_create> returns a unique thread-specific key.

=cut

sub import : locked : method {
    require fields;
    fields::->import(@_);
}	

sub key_create : locked : method {
    our %FIELDS;   # suppress "used only once"
    return ++$FIELDS{__MAX__};
}

1;
