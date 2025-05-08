program problem5;
var
    primes : array [0..7] of integer = [2,3,5,7,11,13,17,19];
    divisor : integer;
    r : integer = 1;
    i : integer;
begin

    for i := low(primes) to high(primes) do
    begin
        divisor := primes[i];
        while divisor <= 20 do
            divisor := divisor * primes[i];
        divisor := divisor div primes[i];
        r := r * divisor;
    end;
    writeln(r);
end.