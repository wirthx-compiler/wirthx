program rangetypetest;

type
    TDays = (1..31);
    TMonths = (1..12);
    TYears = (0..2999);

    TDate = record
        day : TDays;
        month: TMonths;
        year : TYears;
    end;

    procedure printdate(value: TDate);
    begin
        writeln(value.year,'-',value.month,'-',value.day);
    end;
var
    testdate :TDate;
begin
    testdate.day := 10;
    testdate.month := 1;
    testdate.year := 2025;

    printdate(testdate);
    writeln('first day: ',low(testdate.day));
    writeln('last month: ',high(testdate.month));
end.