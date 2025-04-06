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

begin

    print_color(RED);
    print_color(GREEN);
    print_color(BLUE);

end.