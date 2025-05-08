program problem4;




    function is_palindrom(product : integer) : boolean;inline;
    var
        tmp : string;
        h : int64;
        i : integer;
    begin
        tmp := Str(product);
        is_palindrom := true;
        for i := 0 to length(tmp) div 2 do
        begin
            if tmp[i] != tmp[length(tmp) - 1 - i] then
            begin
                is_palindrom := false;
                break;
            end;
        end;
    end;

var
    product : integer;
    palindrom : integer = 0;
    tmp : string;
    rev : string;
    x : integer;
    y : integer;
begin

    for x := 100 to 1000 do
    begin
        for y := 100 to 1000 do
        begin
            product := x * y;
            if is_palindrom(product) and product > palindrom then
                palindrom := product;

        end;
    end;
    writeln('max palindrom = ',palindrom);
end.