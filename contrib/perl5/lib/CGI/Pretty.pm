package CGI::Pretty;

# See the bottom of this file for the POD documentation.  Search for the
# string '=head'.

# You can run this file through either pod2man or pod2html to produce pretty
# documentation in manual or html file format (these utilities are part of the
# Perl 5 distribution).

use strict;
use CGI ();

$CGI::Pretty::VERSION = '1.03';
$CGI::DefaultClass = __PACKAGE__;
$CGI::Pretty::AutoloadClass = 'CGI';
@CGI::Pretty::ISA = qw( CGI );

initialize_globals();

sub _prettyPrint {
    my $input = shift;

    foreach my $i ( @CGI::Pretty::AS_IS ) {
	if ( $$input =~ /<\/$i>/si ) {
	    my ( $a, $b, $c, $d, $e ) = $$input =~ /(.*)<$i(\s?)(.*?)>(.*?)<\/$i>(.*)/si;
	    _prettyPrint( \$a );
	    _prettyPrint( \$e );
	    
	    $$input = "$a<$i$b$c>$d</$i>$e";
	    return;
	}
    }
    $$input =~ s/$CGI::Pretty::LINEBREAK/$CGI::Pretty::LINEBREAK$CGI::Pretty::INDENT/g;
}

sub comment {
    my($self,@p) = CGI::self_or_CGI(@_);

    my $s = "@p";
    $s =~ s/$CGI::Pretty::LINEBREAK/$CGI::Pretty::LINEBREAK$CGI::Pretty::INDENT/g; 
    
    return $self->SUPER::comment( "$CGI::Pretty::LINEBREAK$CGI::Pretty::INDENT$s$CGI::Pretty::LINEBREAK" ) . $CGI::Pretty::LINEBREAK;
}

sub _make_tag_func {
    my ($self,$tagname) = @_;
    return $self->SUPER::_make_tag_func($tagname) if $tagname=~/^(start|end)_/;

    # As Lincoln as noted, the last else clause is VERY hairy, and it
    # took me a while to figure out what I was trying to do.
    # What it does is look for tags that shouldn't be indented (e.g. PRE)
    # and makes sure that when we nest tags, those tags don't get
    # indented.
    # For an example, try print td( pre( "hello\nworld" ) );
    # If we didn't care about stuff like that, the code would be
    # MUCH simpler.  BTW: I won't claim to be a regular expression
    # guru, so if anybody wants to contribute something that would
    # be quicker, easier to read, etc, I would be more than
    # willing to put it in - Brian
    
    return qq{
	sub $tagname { 
	    # handle various cases in which we're called
	    # most of this bizarre stuff is to avoid -w errors
	    shift if \$_[0] && 
		(!ref(\$_[0]) && \$_[0] eq \$CGI::DefaultClass) ||
		    (ref(\$_[0]) &&
		     (substr(ref(\$_[0]),0,3) eq 'CGI' ||
		    UNIVERSAL::isa(\$_[0],'CGI')));
	    
	    my(\$attr) = '';
	    if (ref(\$_[0]) && ref(\$_[0]) eq 'HASH') {
		my(\@attr) = make_attributes('',shift);
		\$attr = " \@attr" if \@attr;
	    }

	    my(\$tag,\$untag) = ("\U<$tagname\E\$attr>","\U</$tagname>\E");
	    return \$tag unless \@_;

	    my \@result;
	    my \$NON_PRETTIFY_ENDTAGS =  join "", map { "</\$_>" } \@CGI::Pretty::AS_IS;

	    if ( \$NON_PRETTIFY_ENDTAGS =~ /\$untag/ ) {
		\@result = map { "\$tag\$_\$untag\$CGI::Pretty::LINEBREAK" } 
		 (ref(\$_[0]) eq 'ARRAY') ? \@{\$_[0]} : "\@_";
	    }
	    else {
		\@result = map { 
		    chomp; 
		    if ( \$_ !~ /<\\// ) {
			s/\$CGI::Pretty::LINEBREAK/\$CGI::Pretty::LINEBREAK\$CGI::Pretty::INDENT/g; 
		    } 
		    else {
			my \$tmp = \$_;
			CGI::Pretty::_prettyPrint( \\\$tmp );
			\$_ = \$tmp;
		    }
		    "\$tag\$CGI::Pretty::LINEBREAK\$CGI::Pretty::INDENT\$_\$CGI::Pretty::LINEBREAK\$untag\$CGI::Pretty::LINEBREAK" } 
		(ref(\$_[0]) eq 'ARRAY') ? \@{\$_[0]} : "\@_";
	    }
	    local \$" = "";
	    return "\@result";
	}
    };
}

sub start_html {
    return CGI::start_html( @_ ) . $CGI::Pretty::LINEBREAK;
}

sub end_html {
    return CGI::end_html( @_ ) . $CGI::Pretty::LINEBREAK;
}

sub new {
    my $class = shift;
    my $this = $class->SUPER::new( @_ );

    Apache->request->register_cleanup(\&CGI::Pretty::_reset_globals) if ($CGI::MOD_PERL);
    $class->_reset_globals if $CGI::PERLEX;

    return bless $this, $class;
}

sub initialize_globals {
    # This is the string used for indentation of tags
    $CGI::Pretty::INDENT = "\t";
    
    # This is the string used for seperation between tags
    $CGI::Pretty::LINEBREAK = "\n";

    # These tags are not prettify'd.
    @CGI::Pretty::AS_IS = qw( A PRE CODE SCRIPT TEXTAREA );

    1;
}
sub _reset_globals { initialize_globals(); }

1;

=head1 NAME

CGI::Pretty - module to produce nicely formatted HTML code

=head1 SYNOPSIS

    use CGI::Pretty qw( :html3 );

    # Print a table with a single data element
    print table( TR( td( "foo" ) ) );

=head1 DESCRIPTION

CGI::Pretty is a module that derives from CGI.  It's sole function is to
allow users of CGI to output nicely formatted HTML code.

When using the CGI module, the following code:
    print table( TR( td( "foo" ) ) );

produces the following output:
    <TABLE><TR><TD>foo</TD></TR></TABLE>

If a user were to create a table consisting of many rows and many columns,
the resultant HTML code would be quite difficult to read since it has no
carriage returns or indentation.

CGI::Pretty fixes this problem.  What it does is add a carriage
return and indentation to the HTML code so that one can easily read
it.

    print table( TR( td( "foo" ) ) );

now produces the following output:
    <TABLE>
       <TR>
          <TD>
             foo
          </TD>
       </TR>
    </TABLE>


=head2 Tags that won't be formatted

The <A> and <PRE> tags are not formatted.  If these tags were formatted, the
user would see the extra indentation on the web browser causing the page to
look different than what would be expected.  If you wish to add more tags to
the list of tags that are not to be touched, push them onto the C<@AS_IS> array:

    push @CGI::Pretty::AS_IS,qw(CODE XMP);

=head2 Customizing the Indenting

If you wish to have your own personal style of indenting, you can change the
C<$INDENT> variable:

    $CGI::Pretty::INDENT = "\t\t";

would cause the indents to be two tabs.

Similarly, if you wish to have more space between lines, you may change the
C<$LINEBREAK> variable:

    $CGI::Pretty::LINEBREAK = "\n\n";

would create two carriage returns between lines.

If you decide you want to use the regular CGI indenting, you can easily do 
the following:

    $CGI::Pretty::INDENT = $CGI::Pretty::LINEBREAK = "";

=head1 BUGS

This section intentionally left blank.

=head1 AUTHOR

Brian Paulsen <Brian@ThePaulsens.com>, with minor modifications by
Lincoln Stein <lstein@cshl.org> for incorporation into the CGI.pm
distribution.

Copyright 1999, Brian Paulsen.  All rights reserved.

This library is free software; you can redistribute it and/or modify
it under the same terms as Perl itself.

Bug reports and comments to Brian@ThePaulsens.com.  You can also write
to lstein@cshl.org, but this code looks pretty hairy to me and I'm not
sure I understand it!

=head1 SEE ALSO

L<CGI>

=cut
