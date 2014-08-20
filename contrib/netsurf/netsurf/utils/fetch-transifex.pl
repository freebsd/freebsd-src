#!/usr/bin/perl
#
# Copyright © 2013 Vincent Sanders <vince@netsurf-browser.org>
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

retrive resource from transifex service

=cut

use strict;
use Getopt::Long ();
use LWP::UserAgent;
use JSON qw( decode_json );
use Data::Dumper;
use Fcntl qw( O_CREAT O_EXCL O_WRONLY O_APPEND O_RDONLY O_WRONLY );

use constant GETOPT_OPTS => qw( auto_abbrev no_getopt_compat bundling );
use constant GETOPT_SPEC =>
  qw( output|o=s
      lang|l=s
      resource|res|r=s
      project|prj|p=s
      user|u=s
      password|w=s
      help|h|? );

# ensure no locale translation is applied and leave it all in UTF-8
use bytes;

# default option values:
my %opt = qw( resource messagesany project netsurf user netsurf );

sub output_stream ();
sub usage         ();

sub main ()
{
    my $output;
    my $opt_ok;

    # option parsing:
    Getopt::Long::Configure( GETOPT_OPTS );
    $opt_ok = Getopt::Long::GetOptions( \%opt, GETOPT_SPEC );

    if( $opt_ok )
    {
        $output = output_stream();
    }

    # double check the options are sane (and we weren't asked for the help)
    if( !$opt_ok || $opt{help} || $opt{lang} !~ /^[a-z]{2}$/ )
    {
        usage();
    }

    my $transifexurl = "https://www.transifex.com/api/2/project/" . $opt{project} . "/resource/" . $opt{resource} . "/translation/" . $opt{lang} . "/";

    my $ua = LWP::UserAgent->new;
    $ua->credentials(
	'www.transifex.com:443',
	'Transifex API',
	$opt{user} => $opt{password}
	);

    my $response = $ua->get( $transifexurl );
    if (!$response->is_success) {
	die $response->status_line . " When fetching " . $transifexurl;
    }

    # Decode the entire JSON
    my $decoded_json = decode_json( $response->decoded_content );

    print ( $output $decoded_json->{'content'} ); 
}

main();

sub usage ()
{
    print(STDERR <<TXT );
usage:
     $0 -l lang-code \
           [-o output-file] [-r resource] [-p project] [-u user] [-w password] 

     lang-code  : en fr ko ...  (no default)
     project    : transifex project (default 'netsurf')
     resource   : transifex resource (default 'messagesany')
     user       : transifex resource (default 'netsurf')
     password   : transifex resource (no default)
     output-file: defaults to standard output
TXT
    exit(1);
}

sub output_stream ()
{
    if( $opt{output} )
    {
        my $ofh;

        sysopen( $ofh, $opt{output}, O_CREAT|O_EXCL|O_APPEND|O_WRONLY ) ||
          die( "$0: Failed to open output file $opt{output}: $!\n" );

        return $ofh;
    }

    return \*STDOUT;
}
