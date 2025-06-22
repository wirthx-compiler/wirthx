{
 Core System Unit
}
unit system;

interface
    type
        PChar = ^char;



    {
     Releases the given memory
    }
    procedure freemem(var F: PChar);
    {
        assignes the filename to the file F
        @param(F file to be asssigned)
        @param(FileName name of the file)
    }
    procedure AssignFile(var F: File;FileName: String); external;
    procedure CloseFile(var F: File); external;
    Procedure inc(var value: integer);
    Procedure inc(var value: int64);

    {
        decreases the value by one
        @param(value to be decreased)
    }
    Procedure dec(var value: integer);

    {
        decreases the value by one
        @param(value to be decreased)
    }
    Procedure dec(var value: int64);
    {
        opens the file for reading
    }
    Procedure reset(var F: file);external;

    {
        @param(F file to read)
        @param(value string to read the file into)
    }
    procedure Readln(var F: File; var value: string); external;
    {
        returns 1 if the string S2 is greater then S1, -1 if the string is smaller and 0 if both are equal
        @param( S1 first string to compare)
        @param( S2 second string to compare with)
        @returns( 0 if equal)
    }
    function CompareStr( S1,S2 : string) : integer;
    function Str(value: integer):string;

implementation
uses ctypes;


    procedure cfree(F : PChar); external 'c' name 'free';

    procedure freemem(var F: PChar);inline;
    begin
        if F != 0 then
        begin
            cfree(F);
            F := 0;
        end;
    end;



    Procedure inc(var value: integer); inline;
    begin
        value := value + 1;
    end;

    Procedure inc(var value: int64); inline;
    begin
        value := value + 1;
    end;

    Procedure dec(var value: integer); inline;
    begin
        value := value - 1;
    end;
    Procedure dec(var value: int64); inline;
    begin
        value := value - 1;
    end;


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
        result := length_S1 - length_S2;
        max := length_S1 - 1;
        if result = 0 then
            for idx := 0 to max do
            begin
                tmp := ord(S1[idx]) - ord(S2[idx]);
                if tmp != 0 then
                begin
                    result := tmp;
                    break;
                end;
            end;
    end;

    function Str(value: integer):string;inline;
    var
        tmp : integer ;
        buffer: array [0..100] of char;
        i  :int64 = 0;
        k : int64 = 0;
        j : int64 = 0;
    begin
        tmp := value;
        if tmp < 0 then
            tmp := tmp * -1;

        while tmp > 0 do
        begin
            buffer[i] := tmp mod 10 + '0';
            tmp := tmp div 10;
            inc(i);
        end;

        if value < 0 then
        begin
            buffer[i] := '-';
            inc(i);
        end;
        SetLength(Str,i+1);
        Str[high(Str)] := 0;

        for j := i - 1 downto 0 do
        begin
            Str[k] := buffer[j];
            inc(k);
        end;
    end;

    procedure Val(
       S: string;
      var V: int64;
      var Code: Word
    );
    var
        idx : integer = 0;
        base : integer = 1;
        tmp : int64 = 0;
        t2 : int64 = 0;
        i : int64 = 0;
        factor : int64 = 1;
    begin
        Code := 0;
        for i := high(S) downto low(S) do
        begin
            if S[i] = '-' then
                factor := -1
            else if (S[i] >= '0') and (S[i] <= '9') then
            begin
                t2 := ord(S[i]) - ord('0');
                tmp := tmp + base * t2;
                base:= base * 10;
            end
            else
                Code := i;
        end;
        if Code = 0 then
            V := tmp * factor;
    end;

end.