program copydata(data, output);
var
    ch: char;
    data: text;
begin
    reset(data);
    while not eof(data) do begin
	while not eoln(data) do begin
	    read(data, ch);
	    write(ch)
	end;
	readln(data);
	writeln
    end
end { copydata }.
