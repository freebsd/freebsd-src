;#
;# lr.pl,v 3.1 1993/07/06 01:09:08 jbj Exp
;#
;#
;# Linear Regression Package for perl
;# to be 'required' from perl
;#
;#  Copyright (c) 1992 
;#  Frank Kardel, Rainer Pruy
;#  Friedrich-Alexander Universitaet Erlangen-Nuernberg
;#
;#
;#############################################################

##
## y = A + Bx
##
## B = (n * Sum(xy) - Sum(x) * Sum(y)) / (n * Sum(x^2) - Sum(x)^2)
##
## A = (Sum(y) - B * Sum(x)) / n
##

##
## interface
##
*lr_init   = *lr'lr_init;	#';# &lr_init(tag); initialize data set for tag
*lr_sample = *lr'lr_sample;	#';# &lr_sample(x,y,tag); enter sample
*lr_Y      = *lr'lr_Y;		#';# &lr_Y(x,tag); compute y for given x 
*lr_X      = *lr'lr_X;		#';# &lr_X(y,tag); compute x for given y
*lr_r      = *lr'lr_r;		#';# &lr_r(tag);   regression coeffizient
*lr_cov    = *lr'lr_cov;	#';# &lr_cov(tag); covariance
*lr_A      = *lr'lr_A;		#';# &lr_A(tag);   
*lr_B      = *lr'lr_B;		#';# &lr_B(tag);
*lr_sigma  = *lr'lr_sigma;	#';# &lr_sigma(tag); standard deviation
*lr_mean   = *lr'lr_mean;	#';# &lr_mean(tag);
#########################

package lr;

sub tagify
{
    local($tag) = @_;
    if (defined($tag))
    {
      *lr_n   = eval "*${tag}_n";
      *lr_sx  = eval "*${tag}_sx";
      *lr_sx2 = eval "*${tag}_sx2";
      *lr_sxy = eval "*${tag}_sxy";
      *lr_sy  = eval "*${tag}_sy";
      *lr_sy2 = eval "*${tag}_sy2";
    }
}

sub lr_init
{
    &tagify($_[$[]) if defined($_[$[]);

    $lr_n   = 0;
    $lr_sx  = 0.0;
    $lr_sx2 = 0.0;
    $lr_sxy = 0.0;
    $lr_sy  = 0.0;
    $lr_sy2 = 0.0;
}

sub lr_sample
{
    local($_x, $_y) = @_;

    &tagify($_[$[+2]) if defined($_[$[+2]);

    $lr_n++;
    $lr_sx  += $_x;
    $lr_sy  += $_y;
    $lr_sxy += $_x * $_y;
    $lr_sx2 += $_x**2;
    $lr_sy2 += $_y**2;
}

sub lr_B
{
    &tagify($_[$[]) if defined($_[$[]);

    return 1 unless ($lr_n * $lr_sx2 - $lr_sx**2);
    return ($lr_n * $lr_sxy - $lr_sx * $lr_sy) / ($lr_n * $lr_sx2 - $lr_sx**2);
}

sub lr_A
{
    &tagify($_[$[]) if defined($_[$[]);

    return ($lr_sy - &lr_B * $lr_sx) / $lr_n;
}

sub lr_Y
{
    &tagify($_[$[]) if defined($_[$[]);

    return &lr_A + &lr_B * $_[$[];
}

sub lr_X
{
    &tagify($_[$[]) if defined($_[$[]);

    return ($_[$[] - &lr_A) / &lr_B;
}

sub lr_r
{
    &tagify($_[$[]) if defined($_[$[]);

    local($s) = ($lr_n * $lr_sx2 - $lr_sx**2) * ($lr_n * $lr_sy2 - $lr_sy**2);

    return 1 unless $s;
    
    return ($lr_n * $lr_sxy - $lr_sx * $lr_sy) / sqrt($s);
}

sub lr_cov
{
    &tagify($_[$[]) if defined($_[$[]);

    return ($lr_sxy - $lr_sx * $lr_sy / $lr_n) / ($lr_n - 1);
}

sub lr_sigma
{
    &tagify($_[$[]) if defined($_[$[]);

    return 0 if $lr_n <= 1;
    return sqrt(($lr_sy2 - ($lr_sy * $lr_sy) / $lr_n) / ($lr_n));
}

sub lr_mean
{
    &tagify($_[$[]) if defined($_[$[]);

    return 0 if $lr_n <= 0;
    return $lr_sy / $lr_n;
}

&lr_init();

1;
