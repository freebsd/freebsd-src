require 5.002;			# For (defined ref)
package dumpvar;

# Needed for PrettyPrinter only:

# require 5.001;  # Well, it coredumps anyway undef DB in 5.000 (not now)

# translate control chars to ^X - Randal Schwartz
# Modifications to print types by Peter Gordon v1.0

# Ilya Zakharevich -- patches after 5.001 (and some before ;-)

# Won't dump symbol tables and contents of debugged files by default

$winsize = 80 unless defined $winsize;


# Defaults

# $globPrint = 1;
$printUndef = 1 unless defined $printUndef;
$tick = "auto" unless defined $tick;
$unctrl = 'quote' unless defined $unctrl;
$subdump = 1;
$dumpReused = 0 unless defined $dumpReused;
$bareStringify = 1 unless defined $bareStringify;

sub main::dumpValue {
  local %address;
  local $^W=0;
  (print "undef\n"), return unless defined $_[0];
  (print &stringify($_[0]), "\n"), return unless ref $_[0];
  dumpvar::unwrap($_[0],0);
}

# This one is good for variable names:

sub unctrl {
	local($_) = @_;
	local($v) ; 

	return \$_ if ref \$_ eq "GLOB";
	s/([\001-\037\177])/'^'.pack('c',ord($1)^64)/eg;
	$_;
}

sub stringify {
	local($_,$noticks) = @_;
	local($v) ; 
	my $tick = $tick;

	return 'undef' unless defined $_ or not $printUndef;
	return $_ . "" if ref \$_ eq 'GLOB';
	$_ = &{'overload::StrVal'}($_) 
	  if $bareStringify and ref $_ 
	    and %overload:: and defined &{'overload::StrVal'};
	
	if ($tick eq 'auto') {
	  if (/[\000-\011\013-\037\177]/) {
	    $tick = '"';
	  }else {
	    $tick = "'";
	  }
	}
	if ($tick eq "'") {
	  s/([\'\\])/\\$1/g;
	} elsif ($unctrl eq 'unctrl') {
	  s/([\"\\])/\\$1/g ;
	  s/([\000-\037\177])/'^'.pack('c',ord($1)^64)/eg;
	  s/([\200-\377])/'\\0x'.sprintf('%2X',ord($1))/eg 
	    if $quoteHighBit;
	} elsif ($unctrl eq 'quote') {
	  s/([\"\\\$\@])/\\$1/g if $tick eq '"';
	  s/\033/\\e/g;
	  s/([\000-\037\177])/'\\c'.chr(ord($1)^64)/eg;
	}
	s/([\200-\377])/'\\'.sprintf('%3o',ord($1))/eg if $quoteHighBit;
	($noticks || /^\d+(\.\d*)?\Z/) 
	  ? $_ 
	  : $tick . $_ . $tick;
}

sub ShortArray {
  my $tArrayDepth = $#{$_[0]} ; 
  $tArrayDepth = $#{$_[0]} < $arrayDepth-1 ? $#{$_[0]} : $arrayDepth-1 
    unless  $arrayDepth eq '' ; 
  my $shortmore = "";
  $shortmore = " ..." if $tArrayDepth < $#{$_[0]} ;
  if (!grep(ref $_, @{$_[0]})) {
    $short = "0..$#{$_[0]}  '" . 
      join("' '", @{$_[0]}[0..$tArrayDepth]) . "'$shortmore";
    return $short if length $short <= $compactDump;
  }
  undef;
}

sub DumpElem {
  my $short = &stringify($_[0], ref $_[0]);
  if ($veryCompact && ref $_[0]
      && (ref $_[0] eq 'ARRAY' and !grep(ref $_, @{$_[0]}) )) {
    my $end = "0..$#{$v}  '" . 
      join("' '", @{$_[0]}[0..$tArrayDepth]) . "'$shortmore";
  } elsif ($veryCompact && ref $_[0]
      && (ref $_[0] eq 'HASH') and !grep(ref $_, values %{$_[0]})) {
    my $end = 1;
	  $short = $sp . "0..$#{$v}  '" . 
	    join("' '", @{$v}[0..$tArrayDepth]) . "'$shortmore";
  } else {
    print "$short\n";
    unwrap($_[0],$_[1]);
  }
}

sub unwrap {
    return if $DB::signal;
    local($v) = shift ; 
    local($s) = shift ; # extra no of spaces
    local(%v,@v,$sp,$value,$key,@sortKeys,$more,$shortmore,$short) ;
    local($tHashDepth,$tArrayDepth) ;

    $sp = " " x $s ;
    $s += 3 ; 

    # Check for reused addresses
    if (ref $v) { 
      my $val = $v;
      $val = &{'overload::StrVal'}($v) 
	if %overload:: and defined &{'overload::StrVal'};
      ($address) = $val =~ /(0x[0-9a-f]+)\)$/ ; 
      if (!$dumpReused && defined $address) { 
	$address{$address}++ ;
	if ( $address{$address} > 1 ) { 
	  print "${sp}-> REUSED_ADDRESS\n" ; 
	  return ; 
	} 
      }
    } elsif (ref \$v eq 'GLOB') {
      $address = "$v" . "";	# To avoid a bug with globs
      $address{$address}++ ;
      if ( $address{$address} > 1 ) { 
	print "${sp}*DUMPED_GLOB*\n" ; 
	return ; 
      } 
    }

    if (ref $v eq 'Regexp') {
      my $re = "$v";
      $re =~ s,/,\\/,g;
      print "$sp-> qr/$re/\n";
      return;
    }

    if ( UNIVERSAL::isa($v, 'HASH') ) { 
	@sortKeys = sort keys(%$v) ;
	undef $more ; 
	$tHashDepth = $#sortKeys ; 
	$tHashDepth = $#sortKeys < $hashDepth-1 ? $#sortKeys : $hashDepth-1
	  unless $hashDepth eq '' ; 
	$more = "....\n" if $tHashDepth < $#sortKeys ; 
	$shortmore = "";
	$shortmore = ", ..." if $tHashDepth < $#sortKeys ; 
	$#sortKeys = $tHashDepth ; 
	if ($compactDump && !grep(ref $_, values %{$v})) {
	  #$short = $sp . 
	  #  (join ', ', 
# Next row core dumps during require from DB on 5.000, even with map {"_"}
	  #   map {&stringify($_) . " => " . &stringify($v->{$_})} 
	  #   @sortKeys) . "'$shortmore";
	  $short = $sp;
	  my @keys;
	  for (@sortKeys) {
	    push @keys, &stringify($_) . " => " . &stringify($v->{$_});
	  }
	  $short .= join ', ', @keys;
	  $short .= $shortmore;
	  (print "$short\n"), return if length $short <= $compactDump;
	}
	for $key (@sortKeys) {
	    return if $DB::signal;
	    $value = $ {$v}{$key} ;
	    print "$sp", &stringify($key), " => ";
	    DumpElem $value, $s;
	}
	print "$sp  empty hash\n" unless @sortKeys;
	print "$sp$more" if defined $more ;
    } elsif ( UNIVERSAL::isa($v, 'ARRAY') ) { 
	$tArrayDepth = $#{$v} ; 
	undef $more ; 
	$tArrayDepth = $#{$v} < $arrayDepth-1 ? $#{$v} : $arrayDepth-1 
	  unless  $arrayDepth eq '' ; 
	$more = "....\n" if $tArrayDepth < $#{$v} ; 
	$shortmore = "";
	$shortmore = " ..." if $tArrayDepth < $#{$v} ;
	if ($compactDump && !grep(ref $_, @{$v})) {
	  if ($#$v >= 0) {
	    $short = $sp . "0..$#{$v}  " . 
	      join(" ", 
		   map {exists $v->[$_] ? stringify $v->[$_] : "empty"} ($[..$tArrayDepth)
		  ) . "$shortmore";
	  } else {
	    $short = $sp . "empty array";
	  }
	  (print "$short\n"), return if length $short <= $compactDump;
	}
	#if ($compactDump && $short = ShortArray($v)) {
	#  print "$short\n";
	#  return;
	#}
	for $num ($[ .. $tArrayDepth) {
	    return if $DB::signal;
	    print "$sp$num  ";
	    if (exists $v->[$num]) {
	        DumpElem $v->[$num], $s;
	    } else {
	    	print "empty slot\n";
	    }
	}
	print "$sp  empty array\n" unless @$v;
	print "$sp$more" if defined $more ;  
    } elsif (  UNIVERSAL::isa($v, 'SCALAR') or ref $v eq 'REF' ) { 
	    print "$sp-> ";
	    DumpElem $$v, $s;
    } elsif ( UNIVERSAL::isa($v, 'CODE') ) { 
	    print "$sp-> ";
	    dumpsub (0, $v);
    } elsif ( UNIVERSAL::isa($v, 'GLOB') ) {
      print "$sp-> ",&stringify($$v,1),"\n";
      if ($globPrint) {
	$s += 3;
	dumpglob($s, "{$$v}", $$v, 1);
      } elsif (defined ($fileno = fileno($v))) {
	print( (' ' x ($s+3)) .  "FileHandle({$$v}) => fileno($fileno)\n" );
      }
    } elsif (ref \$v eq 'GLOB') {
      if ($globPrint) {
	dumpglob($s, "{$v}", $v, 1) if $globPrint;
      } elsif (defined ($fileno = fileno(\$v))) {
	print( (' ' x $s) .  "FileHandle({$v}) => fileno($fileno)\n" );
      }
    }
}

sub matchvar {
  $_[0] eq $_[1] or 
    ($_[1] =~ /^([!~])(.)([\x00-\xff]*)/) and 
      ($1 eq '!') ^ (eval {($_[2] . "::" . $_[0]) =~ /$2$3/});
}

sub compactDump {
  $compactDump = shift if @_;
  $compactDump = 6*80-1 if $compactDump and $compactDump < 2;
  $compactDump;
}

sub veryCompact {
  $veryCompact = shift if @_;
  compactDump(1) if !$compactDump and $veryCompact;
  $veryCompact;
}

sub unctrlSet {
  if (@_) {
    my $in = shift;
    if ($in eq 'unctrl' or $in eq 'quote') {
      $unctrl = $in;
    } else {
      print "Unknown value for `unctrl'.\n";
    }
  }
  $unctrl;
}

sub quote {
  if (@_ and $_[0] eq '"') {
    $tick = '"';
    $unctrl = 'quote';
  } elsif (@_ and $_[0] eq 'auto') {
    $tick = 'auto';
    $unctrl = 'quote';
  } elsif (@_) {		# Need to set
    $tick = "'";
    $unctrl = 'unctrl';
  }
  $tick;
}

sub dumpglob {
    return if $DB::signal;
    my ($off,$key, $val, $all) = @_;
    local(*entry) = $val;
    my $fileno;
    if (($key !~ /^_</ or $dumpDBFiles) and defined $entry) {
      print( (' ' x $off) . "\$", &unctrl($key), " = " );
      DumpElem $entry, 3+$off;
    }
    if (($key !~ /^_</ or $dumpDBFiles) and @entry) {
      print( (' ' x $off) . "\@$key = (\n" );
      unwrap(\@entry,3+$off) ;
      print( (' ' x $off) .  ")\n" );
    }
    if ($key ne "main::" && $key ne "DB::" && %entry
	&& ($dumpPackages or $key !~ /::$/)
	&& ($key !~ /^_</ or $dumpDBFiles)
	&& !($package eq "dumpvar" and $key eq "stab")) {
      print( (' ' x $off) . "\%$key = (\n" );
      unwrap(\%entry,3+$off) ;
      print( (' ' x $off) .  ")\n" );
    }
    if (defined ($fileno = fileno(*entry))) {
      print( (' ' x $off) .  "FileHandle($key) => fileno($fileno)\n" );
    }
    if ($all) {
      if (defined &entry) {
	dumpsub($off, $key);
      }
    }
}

sub CvGV_name_or_bust {
  my $in = shift;
  return if $skipCvGV;		# Backdoor to avoid problems if XS broken...
  $in = \&$in;			# Hard reference...
  eval {require Devel::Peek; 1} or return;
  my $gv = Devel::Peek::CvGV($in) or return;
  *$gv{PACKAGE} . '::' . *$gv{NAME};
}

sub dumpsub {
    my ($off,$sub) = @_;
    my $ini = $sub;
    my $s;
    $sub = $1 if $sub =~ /^\{\*(.*)\}$/;
    my $subref = defined $1 ? \&$sub : \&$ini;
    my $place = $DB::sub{$sub} || (($s = $subs{"$subref"}) && $DB::sub{$s})
      || (($s = CvGV_name_or_bust($subref)) && $DB::sub{$s})
      || ($subdump && ($s = findsubs("$subref")) && $DB::sub{$s});
    $place = '???' unless defined $place;
    $s = $sub unless defined $s;
    print( (' ' x $off) .  "&$s in $place\n" );
}

sub findsubs {
  return undef unless %DB::sub;
  my ($addr, $name, $loc);
  while (($name, $loc) = each %DB::sub) {
    $addr = \&$name;
    $subs{"$addr"} = $name;
  }
  $subdump = 0;
  $subs{ shift() };
}

sub main::dumpvar {
    my ($package,@vars) = @_;
    local(%address,$key,$val,$^W);
    $package .= "::" unless $package =~ /::$/;
    *stab = *{"main::"};
    while ($package =~ /(\w+?::)/g){
      *stab = $ {stab}{$1};
    }
    local $TotalStrings = 0;
    local $Strings = 0;
    local $CompleteTotal = 0;
    while (($key,$val) = each(%stab)) {
      return if $DB::signal;
      next if @vars && !grep( matchvar($key, $_), @vars );
      if ($usageOnly) {
	globUsage(\$val, $key)
	  if ($package ne 'dumpvar' or $key ne 'stab')
	     and ref(\$val) eq 'GLOB';
      } else {
	dumpglob(0,$key, $val);
      }
    }
    if ($usageOnly) {
      print "String space: $TotalStrings bytes in $Strings strings.\n";
      $CompleteTotal += $TotalStrings;
      print "Grand total = $CompleteTotal bytes (1 level deep) + overhead.\n";
    }
}

sub scalarUsage {
  my $size = length($_[0]);
  $TotalStrings += $size;
  $Strings++;
  $size;
}

sub arrayUsage {		# array ref, name
  my $size = 0;
  map {$size += scalarUsage($_)} @{$_[0]};
  my $len = @{$_[0]};
  print "\@$_[1] = $len item", ($len > 1 ? "s" : ""),
    " (data: $size bytes)\n"
      if defined $_[1];
  $CompleteTotal +=  $size;
  $size;
}

sub hashUsage {		# hash ref, name
  my @keys = keys %{$_[0]};
  my @values = values %{$_[0]};
  my $keys = arrayUsage \@keys;
  my $values = arrayUsage \@values;
  my $len = @keys;
  my $total = $keys + $values;
  print "\%$_[1] = $len item", ($len > 1 ? "s" : ""),
    " (keys: $keys; values: $values; total: $total bytes)\n"
      if defined $_[1];
  $total;
}

sub globUsage {			# glob ref, name
  local *name = *{$_[0]};
  $total = 0;
  $total += scalarUsage $name if defined $name;
  $total += arrayUsage \@name, $_[1] if @name;
  $total += hashUsage \%name, $_[1] if %name and $_[1] ne "main::" 
    and $_[1] ne "DB::";   #and !($package eq "dumpvar" and $key eq "stab"));
  $total;
}

sub packageUsage {
  my ($package,@vars) = @_;
  $package .= "::" unless $package =~ /::$/;
  local *stab = *{"main::"};
  while ($package =~ /(\w+?::)/g){
    *stab = $ {stab}{$1};
  }
  local $TotalStrings = 0;
  local $CompleteTotal = 0;
  my ($key,$val);
  while (($key,$val) = each(%stab)) {
    next if @vars && !grep($key eq $_,@vars);
    globUsage \$val, $key unless $package eq 'dumpvar' and $key eq 'stab';
  }
  print "String space: $TotalStrings.\n";
  $CompleteTotal += $TotalStrings;
  print "\nGrand total = $CompleteTotal bytes\n";
}

1;

