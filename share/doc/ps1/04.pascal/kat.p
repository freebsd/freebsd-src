program kat(input, output);
var
    ch: char;
begin
    while not eof do begin
	while not eoln do begin
	    read(ch);
	    write(ch)
	end;
	readln;
	writeln
    end
end { kat }.
