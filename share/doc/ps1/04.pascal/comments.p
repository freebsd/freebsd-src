{ This is a left marginal comment. }
program hello(output);
var i : integer; {This is a trailing comment}
j : integer;	{This is a right marginal comment}
k : array [ 1..10] of array [1..10] of integer;	{Marginal, but past the margin}
{
  An aligned, multi-line comment
  which explains what this program is
  all about
}
begin
i := 1; {Trailing i comment}
{A left marginal comment}
 {An aligned comment}
j := 1;		{Right marginal comment}
k[1] := 1;
writeln(i, j, k[1])
end.
