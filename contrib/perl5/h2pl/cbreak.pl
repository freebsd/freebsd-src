$sgttyb_t   = 'C4 S';

sub cbreak {
    &set_cbreak(1);
}

sub cooked {
    &set_cbreak(0);
}

sub set_cbreak {
    local($on) = @_;

    require 'sizeof.ph';
    require 'sys/ioctl.ph';

    ioctl(STDIN,&TIOCGETP,$sgttyb)
        || die "Can't ioctl TIOCGETP: $!";

    @ary = unpack($sgttyb_t,$sgttyb);
    if ($on) {
        $ary[4] |= &CBREAK;
        $ary[4] &= ~&ECHO;
    } else {
        $ary[4] &= ~&CBREAK;
        $ary[4] |= &ECHO;
    }
    $sgttyb = pack($sgttyb_t,@ary);
    ioctl(STDIN,&TIOCSETP,$sgttyb)
            || die "Can't ioctl TIOCSETP: $!";

}

1;
