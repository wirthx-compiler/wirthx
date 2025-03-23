program enumtest;

type
    TColor = (RED,GREEN,BLUE);

var
    initcolor : TColor = RED;
    color : TColor;
begin 

    color := RED;

    if color = initcolor then
    begin
        color := GREEN;
        writeln('color = GREEN');
    end;
    writeln('low(color) = ',low(color));
    writeln('high(color) = ',high(color));
end.