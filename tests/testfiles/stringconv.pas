program stringconv;
uses strings;
var
    strval : string;
    teststr : string = '222311';
    negval : string = '-225';
    r : int64;
    code : Word;

begin

    strval := Str(12223);

    writeln(strval);
    Val(teststr,r,code);
    writeln('r = ',r);

    Val(negval,r,code);
    writeln('r = ',r);
end.