program x(output);
var
	a: set of char;
	b: Boolean;
	c: (red, green, blue);
	p: ^ integer;
	A: alfa;
	B: packed array [1..5] of char;
begin
	b := true;
	c := red;
	new(p);
	a := [];
	A := 'Hello, yellow';
	b := a and b;
	a := a * 3;
	if input < 2 then writeln('boo');
	if p <= 2 then writeln('sure nuff');
	if A = B then writeln('same');
	if c = true then writeln('hue''s and color''s')
end.
