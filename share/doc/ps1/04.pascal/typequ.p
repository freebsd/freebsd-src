program x(output);
var
	x: ^ integer;
	y: ^ integer;
begin
	new(y);
	x := y;
	x^ := 1;
	x := x;
end.
