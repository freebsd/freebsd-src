;# Usage: &flush(FILEHANDLE)
;# flushes the named filehandle

;# Usage: &printflush(FILEHANDLE, "prompt: ")
;# prints arguments and flushes filehandle

sub flush {
    local($old) = select(shift);
    $| = 1;
    print "";
    $| = 0;
    select($old);
}

sub printflush {
    local($old) = select(shift);
    $| = 1;
    print @_;
    $| = 0;
    select($old);
}

1;
