package bytes;

sub length ($) {
    BEGIN { bytes::import() }
    return CORE::length($_[0]);
}

1;
