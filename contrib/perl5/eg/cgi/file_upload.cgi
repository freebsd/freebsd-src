#!/usr/local/bin/perl -w

use strict 'refs';
use lib '..';
use CGI qw(:standard);
use CGI::Carp qw/fatalsToBrowser/;

print header();
print start_html("File Upload Example");
print strong("Version "),$CGI::VERSION,p;

print h1("File Upload Example"),
    'This example demonstrates how to prompt the remote user to
    select a remote file for uploading. ',
    strong("This feature only works with Netscape 2.0 or greater, or IE 4.0 or greater."),
    p,
    'Select the ',cite('browser'),' button to choose a text file
    to upload.  When you press the submit button, this script
    will count the number of lines, words, and characters in
    the file.';

my @types = ('count lines','count words','count characters');

# Start a multipart form.
print start_multipart_form(),
    "Enter the file to process:",
    filefield('filename','',45),
    br,
    checkbox_group('count',\@types,\@types),
    p,
    reset,submit('submit','Process File'),
    endform;

# Process the form if there is a file name entered
if (my $file = param('filename')) {
    my %stats;
    my $tmpfile=tmpFileName($file);
    my $mimetype = uploadInfo($file)->{'Content-Type'} || '';
    print hr(),
          h2($file),
          h3($tmpfile),
          h4("MIME Type:",em($mimetype));

    my($lines,$words,$characters,@words) = (0,0,0,0);
    while (<$file>) {
	$lines++;
	$words += @words=split(/\s+/);
	$characters += length($_);
    }
    close $file;
    grep($stats{$_}++,param('count'));
    if (%stats) {
	print strong("Lines: "),$lines,br if $stats{'count lines'};
	print strong("Words: "),$words,br if $stats{'count words'};
	print strong("Characters: "),$characters,br if $stats{'count characters'};
    } else {
	print strong("No statistics selected.");
    }
}

# print cite("URL parameters: "),url_param();

print hr(),
    a({href=>"../cgi_docs.html"},"CGI documentation"),
    hr,
    address(
	    a({href=>'/~lstein'},"Lincoln D. Stein")),
    br,
    'Last modified July 17, 1996',
    end_html;

