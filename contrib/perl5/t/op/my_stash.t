#!./perl

package Foo;

BEGIN {
    @INC = '../lib';
}

use Test;

plan tests => 7;

use constant MyClass => 'Foo::Bar::Biz::Baz';

{
    package Foo::Bar::Biz::Baz;
}

for (qw(Foo Foo:: MyClass __PACKAGE__)) {
    eval "sub { my $_ \$obj = shift; }";
    ok ! $@;
#    print $@ if $@;
}

use constant NoClass => 'Nope::Foo::Bar::Biz::Baz';

for (qw(Nope Nope:: NoClass)) {
    eval "sub { my $_ \$obj = shift; }";
    ok $@;
#    print $@ if $@;
}
