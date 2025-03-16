{
    Null-terminated string (PChar) routines.
}
unit strings;

interface

    {
        returns 1 if the string S2 is greater then S1, -1 if the string is smaller and 0 if both are equal
        @param( S1 first string to compare)
        @param( S2 second string to compare with)
        @returns( 0 if equal)
    }
    function CompareStr( S1,S2 : string) : integer;

    {
        AnsiCompareStr compares two strings and returns the following result:

    }
    function AnsiCompareStr(
      S1: string;
      S2: string
    ):Integer;

    {
        returns the length of the given pointer to a character
        @param(str null terminated string)
    }
    function strlen(str :  pchar) : int64; external 'c' name 'strlen';

implementation

    function CompareStr( S1,S2 : string) : integer;
    var
        idx : integer = 0;
        tmp : int64 = 0;
        length_S1 : int64;
        length_S2 : int64;
        max :int64;
    begin
        length_S1 := length(S1);
        length_S2 := length(S2);
        CompareStr := length_S1 - length_S2;
        max := length_S1 - 1;
        if CompareStr = 0 then
            for idx := 0 to max do
            begin
                tmp := S1[idx] - S2[idx];
                if tmp != 0 then
                begin
                    CompareStr := tmp;
                    break;
                end;
            end;
    end;
end.