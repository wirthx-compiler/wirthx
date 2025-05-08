program forintest;

type
    TColor = (RED,GREEN,BLUE);
    TColorList = array of TColor;
    TFixedColorList = array[0..2] of TColor;

var
    mycolor : TColor;
    mycolorlist : TColorList;
    myfixedcolorlist : TFixedColorList;

    mystring : string = ' this is a string';
    mychar : char;
begin

    mycolor := RED;
    myfixedcolorlist[0] := RED;
    myfixedcolorlist[1] := GREEN;
    myfixedcolorlist[2] := BLUE;


    setlength(mycolorlist, 3);
    mycolorlist[0] := RED;
    mycolorlist[1] := GREEN;
    mycolorlist[2] := BLUE;


    for mycolor in myfixedcolorlist do
    begin
        case mycolor of
            RED: writeln('red');
            GREEN: writeln('green');
            BLUE: writeln('blue');
        else
        begin
            writeln('unknown color');
            halt(1);
        end;
    end;

    for mycolor in mycolorlist do
    begin
        case mycolor of
            RED: writeln('red');
            GREEN: writeln('green');
            BLUE: writeln('blue');
        else
        begin
            writeln('unknown color');
            halt(1);
        end;
    end;

    writeln('lower bound: ', low(mystring));
    writeln('upper bound: ', high(mystring));

    for mychar in mystring do
        write(mychar);
    writeln();

end.