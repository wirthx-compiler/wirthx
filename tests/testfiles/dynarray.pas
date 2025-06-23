program dynarray;

var
  aOldString: Array of Char ;
  idx :integer;
begin
  SetLength(aOldString, 4);
  aOldString[0] := 'a';
  aOldString[1] := 'b';
  aOldString[2] := 'c';
  aOldString[3] := 'd';
  for idx := low(aOldString) to high(aOldString) do
  begin
        writeln(aOldString[idx]);
  end;

  SetLength(aOldString,7);
    aOldString[4] := 'e';
    aOldString[5] := 'f';
    aOldString[6] := 'g';

     for idx := low(aOldString) to high(aOldString) do
     begin
           write(aOldString[idx]);
     end;
        writeln();
end.