#! /usr/local/bin/perl

while ($l = <>)
{
chop $l;

$l =~ s/\\xx //;
$l =~ s/}{/|/g;
$l =~ s/{//g;
$l =~ s/}//g;
@x = split(/\|/, $l);

printf ("\\xx ");
for ($i = 0; $i <= $#x; $i++)
    {
    $v = substr($x[$i], 0, 17);
    $v =~ s/\\$//;
    printf("{%s}", $v);
    }
printf ("\n");

}
