#!/usr/bin/perl
#
# Copyright © 2013 Vivek Dasmohapatra <vivek@collabora.co.uk>
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
# 
#   * The above copyright notice and this permission notice shall be included in
#     all copies or substantial portions of the Software.
# 
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.

=head1

Take a single-language messages file and merge it back in to the
NetSurf master messaged (i10n) file.

=cut

use strict;

use Getopt::Long ();
use Fcntl qw( O_CREAT O_EXCL O_WRONLY O_APPEND O_RDONLY O_WRONLY O_TRUNC );

use constant GETOPT_OPTS => qw( auto_abbrev no_getopt_compat bundling );
use constant GETOPT_SPEC =>
  qw( output|o=s
      input|i=s
      lang|l=s
      plat|platform|p=s
      format|fmt|f=s
      import|I=s
      help|h|? );

# default option values:
my %opt = qw( plat any format messages );

sub input_stream  ($;$);
sub output_stream ();
sub usage         ();
sub parser        ();

sub main ()
{
    my $input;
    my $output;
    my $import;
    my $parser;
    my $opt_ok;
    my @input;
    my %message;
    my $last_key;
    my $last_plat;

    # option parsing:
    Getopt::Long::Configure( GETOPT_OPTS );
    $opt_ok = Getopt::Long::GetOptions( \%opt, GETOPT_SPEC );

    # allow input, import & output to be specified as non-option arguments:
    if( @ARGV ) { $opt{input } ||= shift( @ARGV ) }
    if( @ARGV ) { $opt{import} ||= shift( @ARGV ) }
    if( @ARGV ) { $opt{output} ||= shift( @ARGV ) }

    # open the appropriate streams and get the formatter and headers:
    if( $opt_ok )
    {
        $input  = input_stream( $opt{input} );
        $import = input_stream( $opt{import}, 'import-file' );
        $parser = parser();
        $opt{plat} ||= 'any';
    }

    # double check the options are sane (and we weren't asked for the help)
    if( !$opt_ok || $opt{help} || $opt{lang} !~ /^[a-z]{2}$/ )
    {
        usage();
    }

    @input = <$input>;
    $output = output_stream();

    $parser->( \%message, $import );

    foreach ( @input )
    {
        use bytes;

        my( $lang, $plat, $key );

        if( /^([a-z]{2})\.([^.]+)\.([^:]+):/ )
        {
            ( $lang, $plat, $key ) = ( $1, $2, $3 );
        }

        if( $key || $message{ $last_key } )
        {
            #print( $output "## $last_key -> $key\n" );
            # the key changed but we have a message for it still pending:
            if( $last_key && $message{ $last_key } && ($key ne $last_key) )
            {
                my $plt = $last_plat;
                my $str = $message{ $last_key };
                my $msg = qq|$opt{lang}.$last_plat.$last_key:$str\n|;

                print( $output $msg );
                delete( $message{ $last_key } );

                # if the line following our new translation is not blank,
                # generate a synthetic group-separator:
                if( !/^\s*$/ ) { print( $output "\n") }
            }

            $last_key  = $key;
            $last_plat = $plat;

            if( $lang eq $opt{lang} )
            {
                my $val = $message{ $key };
                if( $val &&
                    ( $opt{plat} eq 'any' ||   # all platforms ok
                      $opt{plat} eq $plat  ) ) # specified platform matched
                {
                    print( $output qq|$1.$2.$3:$val\n| );
                    delete( $message{ $key } );
                    next;
                }
            }
        }

        print( $output $_ );
    }
}

main();

sub usage ()
{
    my @fmt = map { s/::$//; $_ } keys(%{$::{'msgfmt::'}});
    print( STDERR <<TXT );
usage:
   $0 -l lang-code \
         [-p platform] [-f format] \
         [-o output-file] [-i input-file] [-I import-file]

   $0 -l lang-code … [input-file [import-file [output-file]]]

     lang-code  : en fr ko …  (no default)
     platform   : any gtk ami   (default 'any')
     format     : @fmt (default 'messages')
     input-file : defaults to standard input
     output-file: defaults to standard output
     import-file: no default

     The input-file may be the same as the output-file, in which case
     it will be altered in place.
TXT
    exit(1);
}

sub input_stream ($;$)
{
    my $file = shift();
    my $must_exist = shift();

    if( $file )
    {
        my $ifh;

        sysopen( $ifh, $file, O_RDONLY ) ||
          die( "$0: Failed to open input file $file: $!\n" );

        return $ifh;
    }

    if( $must_exist )
    {
        print( STDERR "No file specified for $must_exist\n" );
        usage();
    }

    return \*STDIN;
}

sub output_stream ()
{
    if( $opt{output} )
    {
        my $ofh;

        sysopen( $ofh, $opt{output}, O_CREAT|O_TRUNC|O_WRONLY ) ||
          die( "$0: Failed to open output file $opt{output}: $!\n" );

        return $ofh;
    }

    return \*STDOUT;
}

sub parser ()
{
    my $name = $opt{format};
    my $func = "msgfmt::$name"->UNIVERSAL::can("parse");

    return $func || die( "No handler found for format '$name'\n" );
}

# format implementations:
{
    package msgfmt::java;

    sub unescape { $_[0] =~ s/\\([^abfnrtv])/$1/g; $_[0] }
    sub parse
    {
        my $cache  = shift();
        my $stream = shift();

        while ( <$stream> )
        {
            if( /([^#]\S+)\s*=\s?(.*)/ )
            {
                my $key = $1;
                my $val = $2;
                $cache->{ $key } = unescape( $val );
            }
        }
    }
}

{
    package msgfmt::messages; # native netsurf format

    sub parse
    {
        my $cache  = shift();
        my $stream = shift();

        while ( <$stream> )
        {
            if( /^([a-z]{2})\.([^.]+)\.([^:]+):(.*)/ )
            {
                my( $lang, $plat, $key, $val ) = ( $1, $2, $3, $4 );

                if( $lang ne $opt{lang} ) { next }
                if( $opt{plat} ne 'any' &&
                    $opt{plat} ne $plat &&
                    'all'      ne $plat ) { next }

                $cache->{ $key } = $val;
            }
        }
    }
}

{
    package msgfmt::transifex;
    use base 'msgfmt::java';

    # the differences between transifex and java properties only matter in
    # the outward direction: During import they can be treated the same way
}

{
    package msgfmt::android;

    ANDROID_XML:
    {
        package msgfmt::android::xml;

        my @stack;
        my $data;
        my $key;
        our $cache;

        sub StartDocument ($)   { @stack = (); $key = '' }
        sub Text          ($)   { if( $key ) { $data .= $_ } }
        sub PI            ($$$) { }
        sub EndDocument   ($)   { }

        sub EndTag ($$)
        {
            pop( @stack );

            if( !$key ) { return; }

            $cache->{ $key } = $data;
            $data = $key = '';
        }

        sub StartTag ($$)
        {
            push( @stack, $_[1] );

            if( "@stack" eq "resources string" )
            {
                $data = '';
                $key  = $_{ name };
            }
        }
    }

    sub parse
    {
        require XML::Parser;

        if( !$XML::Parser::VERSION )
        {
            die("XML::Parser required for android format support\n");
        }

        $msgfmt::android::xml::cache = shift();
        my $stream = shift();
        my $parser = XML::Parser->new( Style => 'Stream',
                                       Pkg   => 'msgfmt::android::xml' );
        $parser->parse( $stream );
    }
}
