# Stash.pm -- show what stashes are loaded
# vishalb@hotmail.com 
package B::Stash;

=pod

=head1 NAME

B::Stash - show what stashes are loaded

=cut

BEGIN { %Seen = %INC }

CHECK {
	my @arr=scan($main::{"main::"});
       @arr=map{s/\:\:$//;$_ eq "<none>"?():$_;}  @arr;
	print "-umain,-u", join (",-u",@arr) ,"\n";
}
sub scan{
	my $start=shift;
	my $prefix=shift;
	$prefix = '' unless defined $prefix;
	my @return;
	foreach my $key ( keys %{$start}){
#		print $prefix,$key,"\n";
		if ($key =~ /::$/){
			unless ($start  eq ${$start}{$key} or $key eq "B::" ){
		 		push @return, $key unless omit($prefix.$key);
				foreach my $subscan ( scan(${$start}{$key},$prefix.$key)){
		 			push @return, "$key".$subscan; 	
				}
			}
		}
	}
	return @return;
}
sub omit{
	my $module = shift;
	my %omit=("DynaLoader::" => 1 , "XSLoader::" => 1, "CORE::" => 1 ,
		"CORE::GLOBAL::" => 1, "UNIVERSAL::" => 1 );
	return 1 if $omit{$module};
	if ($module eq "IO::" or $module eq "IO::Handle::"){
		$module =~ s/::/\//g;	
		return 1 unless  $INC{$module};
	}

	return 0;
}
1;
