program exittest;

    function myfunction(arg: integer) : boolean;
    begin
        if arg > 10 then
            exit(true);
        exit(false);
    end;


    function double10(arg: integer) : double;
    begin
        if arg >= 10 then
            exit(20.0);
        exit(0.0);
    end;

    procedure impliciteRet(test: integer);
    begin

        if test = 12 then
        begin
            writeln('test is 12');
            exit();
        end;
            writeln('test is not 12');
    end;
var

begin 
    if myfunction(11) then
        writeln('11 is greater then 10');

    if not myfunction(9) then
        writeln('9 is not greater then 10');

    writeln('Test 1: ',double10(10));
    writeln('Test 2: ',double10(5));

    impliciteRet(12);
    impliciteRet(11);
end.