#!/usr/bin/perl
#
# Copyright (c) 1998 Nick Hibma
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#
#
#
# Reads all the files mentioned on the command line (or stdin) and combines
# them into one.
#
# The files must have the following format:
#
# ######################### (line is ignored)
# # Ttopic Dhello world (short description)
#     This is the long description and can span
# multiple lines and empty
#
# ones.
# ########################### (this line is again ignored)
# # Ttopic Ssubtopic Dagain a short description
# a subtopic is a topic that will connected under the subtree of
# topic.

use FileHandle;

# Declaration of constants
#
$SOD 	= 'D';				# Start of description character
$SLABEL = '_sdescr';	   # Short description label
$LLABEL = '_ldescr';	   # Long description label

# Global variables
#

# Read the command line parameters
#
while ( $arg = shift @ARGV ) {
	if ( $arg eq '-h' ) {
		die "$0 [-h] [files ...]\nno filenames or '-' as a filename reads stdin\n";
	} else {
		push @files, $arg;
	}
}

# No files to process? Read STDIN
#
push @files, '-'           # no files found? Process STDIN
	if $#files == -1;

# Perform processing on each file
#
foreach $filename ( @files ) {
	if ( $filename eq '-' ) {
		$file = STDIN;
	} else {
		die "Could not open file $filename, $!"
			unless $file = new FileHandle $filename;
	}

	# Process one file and add it to the hash
	#
	&add_file($file);
} 

# Print the results of our processing
#
&print_topic($topics, '#'x80 . "\n# ");
print '#'x80 . "\n";

#require 'z39.50/PerlieZ/debug.pm';
#debug::Dump($topics, '$topics');

# Make like a tree and leave.
#
exit 0;

sub add_file {
	my ($file) = @_;

	# process a file and add it to the hash

	$line = <$file>;
	while ( !eof($file) ) {
		if ( $line =~ s/^#\s+// ) {

			# the line contains a number of parts (separated by whitespace):
			# (Cl+ )+ Dd+
			# C is a character not equal to D
			# l+ is a word without spaces
			# (Cl+ )+ is a list of words preceded by C and separated by spaces
			# d+ is a description (can contain spaces)
			# D is the character in $SOD
         #
			# we split it into multiple l+ parts and one d+ part
			# after that we insert the d+ part at the right point in the tree
			# (after reading also the long descrescription in the next lines)

			@ar = ();
			while ( $line =~ s/^([^$SOD]\S+)\s+//o ) {
				$label = $1;
				$label .= ' '				# avoid conflicts with hash labels
					if $label eq $SLABEL or $label eq $LLABEL;
				push @ar, $label;
			}
			$sdescr = $line;				# short descr. is rest of line

			my $ldescr = '';				# read the long description
			$line = <$file>;
			while ( !eof($file) and $line !~ m/^#/ ) {
				$ldescr .= $line;
				$line = <$file>;
			}

			$topics = &add_to_hash($topics, $sdescr, $ldescr, @ar);
		} else {
			$line = <$file>;
		}
	}
}

sub add_to_hash {
	my ($hash, $sdescr, $ldescr, @ar) = @_;

	# bit more complicated than usual, because we want to insert
	# value into an existing tree if possible, or otherwise build it.
	# Probably could be done with references as well, but this seems neater.

	if ( $#ar == -1 ) {
		# Create a new leaf (reference to descriptions hash)

		return {$SLABEL=>$sdescr, $LLABEL=>$ldescr};
	} else {
		# Add subtree to node and if node does not exist, create an empty one 
		# (reference to a hash of subnodes)

		$hash = {}			# create a new ref to hash on the fly
			if !$hash;
		my $label = shift @ar;		# next label in tree to be used
		$hash->{$label} = &add_to_hash($hash->{$label}, $sdescr, $ldescr, @ar);
		return $hash;
	}
}

sub print_topic {
	my ($topic, $preprint) = @_;

	# print out a topic and its subtopics recursively
	# preprint is the string before the current subtopic hash
	# and is the same for all the subtopics in the current hash

   if ( $topic->{$SLABEL} or $topic->{$LLABEL} ) {
      # leaf found
      print $preprint . "$topic->{$SLABEL}$topic->{$LLABEL}";
   }

   # iterate over all the subtopics
   my ($label);
   foreach $label ( sort keys %{ $topic } ) {
      next
         if $label eq $SLABEL or $label eq $LLABEL;
      &print_topic($topic->{$label}, $preprint . $label . ' ');
	}
}
