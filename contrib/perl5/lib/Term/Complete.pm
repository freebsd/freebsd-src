package Term::Complete;
require 5.000;
require Exporter;

@ISA = qw(Exporter);
@EXPORT = qw(Complete);

#      @(#)complete.pl,v1.2            (me@anywhere.EBay.Sun.COM) 09/23/91

=head1 NAME

Term::Complete - Perl word completion module

=head1 SYNOPSIS

    $input = Complete('prompt_string', \@completion_list);
    $input = Complete('prompt_string', @completion_list);

=head1 DESCRIPTION

This routine provides word completion on the list of words in
the array (or array ref).

The tty driver is put into raw mode using the system command
C<stty raw -echo> and restored using C<stty -raw echo>.

The following command characters are defined:

=over 4

=item E<lt>tabE<gt>

Attempts word completion.
Cannot be changed.

=item ^D

Prints completion list.
Defined by I<$Term::Complete::complete>.

=item ^U

Erases the current input.
Defined by I<$Term::Complete::kill>.

=item E<lt>delE<gt>, E<lt>bsE<gt>

Erases one character.
Defined by I<$Term::Complete::erase1> and I<$Term::Complete::erase2>.

=back

=head1 DIAGNOSTICS

Bell sounds when word completion fails.

=head1 BUGS

The completion character E<lt>tabE<gt> cannot be changed.

=head1 AUTHOR

Wayne Thompson

=cut

CONFIG: {
    $complete = "\004";
    $kill     = "\025";
    $erase1 =   "\177";
    $erase2 =   "\010";
}

sub Complete {
    my($prompt, @cmp_list, $cmp, $test, $l, @match);
    my ($return, $r) = ("", 0);

    $return = "";
    $r      = 0;

    $prompt = shift;
    if (ref $_[0] || $_[0] =~ /^\*/) {
	@cmp_lst = sort @{$_[0]};
    }
    else {
	@cmp_lst = sort(@_);
    }

    system('stty raw -echo');
    LOOP: {
        print($prompt, $return);
        while (($_ = getc(STDIN)) ne "\r") {
            CASE: {
                # (TAB) attempt completion
                $_ eq "\t" && do {
                    @match = grep(/^$return/, @cmp_lst);
                    unless ($#match < 0) {
                        $l = length($test = shift(@match));
                        foreach $cmp (@match) {
                            until (substr($cmp, 0, $l) eq substr($test, 0, $l)) {
                                $l--;
                            }
                        }
                        print("\a");
                        print($test = substr($test, $r, $l - $r));
                        $r = length($return .= $test);
                    }
                    last CASE;
                };

                # (^D) completion list
                $_ eq $complete && do {
                    print(join("\r\n", '', grep(/^$return/, @cmp_lst)), "\r\n");
                    redo LOOP;
                };

                # (^U) kill
                $_ eq $kill && do {
                    if ($r) {
                        $r	= 0;
			$return	= "";
                        print("\r\n");
                        redo LOOP;
                    }
                    last CASE;
                };

                # (DEL) || (BS) erase
                ($_ eq $erase1 || $_ eq $erase2) && do {
                    if($r) {
                        print("\b \b");
                        chop($return);
                        $r--;
                    }
                    last CASE;
                };

                # printable char
                ord >= 32 && do {
                    $return .= $_;
                    $r++;
                    print;
                    last CASE;
                };
            }
        }
    }
    system('stty -raw echo');
    print("\n");
    $return;
}

1;

