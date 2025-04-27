program casetest;


type
    TColor = (RED,GREEN,BLUE);

    procedure print_color(color : TColor);
    begin
        case color of
            RED: writeln('red');
            GREEN:
                writeln('green');
            BLUE:
                writeln('blue');
        else
            writeln('unknown color');
        end;
    end;

    function name_to_color(colorname : string) : TColor;
    begin
        case colorname of
            'red': name_to_color := RED;
            'green': name_to_color := GREEN;
            'blue': name_to_color := BLUE;
        else
            name_to_color := RED; // default
        end;
    end;


    function limit_check(value : integer) : boolean;
    const
        limit = 10;
    begin
        case value of
            -limit..limit:
                limit_check := true;
        else
            limit_check := false;
        end;
    end;

begin

    print_color(RED);
    print_color(GREEN);
    print_color(BLUE);

    print_color(name_to_color('red'));
    print_color(name_to_color('green'));
    print_color(name_to_color('blue'));


    if limit_check(5) then
        writeln('5 is within limits')
    else
        writeln('5 is out of limits');

    if limit_check(15) then
        writeln('15 is within limits')
    else
        writeln('15 is out of limits');

    if limit_check(-5) then
        writeln('-5 is within limits')
    else
        writeln('-5 is out of limits');
end.