package Tie::SubstrHash;

=head1 NAME

Tie::SubstrHash - Fixed-table-size, fixed-key-length hashing

=head1 SYNOPSIS

    require Tie::SubstrHash;

    tie %myhash, 'Tie::SubstrHash', $key_len, $value_len, $table_size;

=head1 DESCRIPTION

The B<Tie::SubstrHash> package provides a hash-table-like interface to
an array of determinate size, with constant key size and record size.

Upon tying a new hash to this package, the developer must specify the
size of the keys that will be used, the size of the value fields that the
keys will index, and the size of the overall table (in terms of key-value
pairs, not size in hard memory). I<These values will not change for the
duration of the tied hash>. The newly-allocated hash table may now have
data stored and retrieved. Efforts to store more than C<$table_size>
elements will result in a fatal error, as will efforts to store a value
not exactly C<$value_len> characters in length, or reference through a
key not exactly C<$key_len> characters in length. While these constraints
may seem excessive, the result is a hash table using much less internal
memory than an equivalent freely-allocated hash table.

=head1 CAVEATS

Because the current implementation uses the table and key sizes for the
hashing algorithm, there is no means by which to dynamically change the
value of any of the initialization parameters.

=cut

use Carp;

sub TIEHASH {
    my $pack = shift;
    my ($klen, $vlen, $tsize) = @_;
    my $rlen = 1 + $klen + $vlen;
    $tsize = findprime($tsize * 1.1);	# Allow 10% empty.
    $self = bless ["\0", $klen, $vlen, $tsize, $rlen, 0, -1];
    $$self[0] x= $rlen * $tsize;
    $self;
}

sub FETCH {
    local($self,$key) = @_;
    local($klen, $vlen, $tsize, $rlen) = @$self[1..4];
    &hashkey;
    for (;;) {
	$offset = $hash * $rlen;
	$record = substr($$self[0], $offset, $rlen);
	if (ord($record) == 0) {
	    return undef;
	}
	elsif (ord($record) == 1) {
	}
	elsif (substr($record, 1, $klen) eq $key) {
	    return substr($record, 1+$klen, $vlen);
	}
	&rehash;
    }
}

sub STORE {
    local($self,$key,$val) = @_;
    local($klen, $vlen, $tsize, $rlen) = @$self[1..4];
    croak("Table is full") if $$self[5] == $tsize;
    croak(qq/Value "$val" is not $vlen characters long./)
	if length($val) != $vlen;
    my $writeoffset;

    &hashkey;
    for (;;) {
	$offset = $hash * $rlen;
	$record = substr($$self[0], $offset, $rlen);
	if (ord($record) == 0) {
	    $record = "\2". $key . $val;
	    die "panic" unless length($record) == $rlen;
	    $writeoffset = $offset unless defined $writeoffset;
	    substr($$self[0], $writeoffset, $rlen) = $record;
	    ++$$self[5];
	    return;
	}
	elsif (ord($record) == 1) {
	    $writeoffset = $offset unless defined $writeoffset;
	}
	elsif (substr($record, 1, $klen) eq $key) {
	    $record = "\2". $key . $val;
	    die "panic" unless length($record) == $rlen;
	    substr($$self[0], $offset, $rlen) = $record;
	    return;
	}
	&rehash;
    }
}

sub DELETE {
    local($self,$key) = @_;
    local($klen, $vlen, $tsize, $rlen) = @$self[1..4];
    &hashkey;
    for (;;) {
	$offset = $hash * $rlen;
	$record = substr($$self[0], $offset, $rlen);
	if (ord($record) == 0) {
	    return undef;
	}
	elsif (ord($record) == 1) {
	}
	elsif (substr($record, 1, $klen) eq $key) {
	    substr($$self[0], $offset, 1) = "\1";
	    return substr($record, 1+$klen, $vlen);
	    --$$self[5];
	}
	&rehash;
    }
}

sub FIRSTKEY {
    local($self) = @_;
    $$self[6] = -1;
    &NEXTKEY;
}

sub NEXTKEY {
    local($self) = @_;
    local($klen, $vlen, $tsize, $rlen, $entries, $iterix) = @$self[1..6];
    for (++$iterix; $iterix < $tsize; ++$iterix) {
	next unless substr($$self[0], $iterix * $rlen, 1) eq "\2";
	$$self[6] = $iterix;
	return substr($$self[0], $iterix * $rlen + 1, $klen);
    }
    $$self[6] = -1;
    undef;
}

sub hashkey {
    croak(qq/Key "$key" is not $klen characters long.\n/)
	if length($key) != $klen;
    $hash = 2;
    for (unpack('C*', $key)) {
	$hash = $hash * 33 + $_;
	&_hashwrap if $hash >= 1e13;
    }
    &_hashwrap if $hash >= $tsize;
    $hash = 1 unless $hash;
    $hashbase = $hash;
}

sub _hashwrap {
    $hash -= int($hash / $tsize) * $tsize;
}

sub rehash {
    $hash += $hashbase;
    $hash -= $tsize if $hash >= $tsize;
}

sub findprime {
    use integer;

    my $num = shift;
    $num++ unless $num % 2;

    $max = int sqrt $num;

  NUM:
    for (;; $num += 2) {
	for ($i = 3; $i <= $max; $i += 2) {
	    next NUM unless $num % $i;
	}
	return $num;
    }
}

1;
