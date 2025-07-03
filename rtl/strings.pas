{
    Null-terminated string (PChar) routines.
}
unit strings;

interface
uses system;

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

    {
    Allocate memory for a new null-terminated string on the heap
    @returns( pointer to the new string)
    }
    function stralloc(L : int64) : pchar;


    {

    Concatenate 2 null-terminated strings.
    @param( dest destination string)
    @param( src source string)
    @returns( pointer to the destination string)
    }
    function strcat(
        dest : pchar;
        src : pchar
    ): pchar; external 'c' name 'strcat';

    {
    Compare 2 null-terminated strings, case sensitive.
    @param( str1 first string to compare)
    @param( str2 second string to compare)
    @returns( 0 if equal, positive if str1 > str2, negative if str1 < str2)
    }
    function strcomp(
        str1 : pchar;
        str2 : pchar
    ): integer; external 'c' name 'strcmp';

    {
    Copy a null-terminated string
    @returns( pointer to the destination string)
    }
    function strcopy(
        dest : pchar;
        src : pchar
    ): pchar; external 'c' name 'strcpy';

    {
    disposes of a null-terminated string on the heap
    }
    function strdispose(
        str : pchar
    ): integer; external 'c' name 'free';

    {
    Copy a null-terminated string, return a pointer to the end.
    }
    function strecopy(
        dest : pchar;
        src : pchar
    ): pchar; external 'c' name 'strcpy';

    {


    {
    Return a pointer to the end of a null-terminated string

    }
    function strend(
        str : pchar
    ): pchar; external 'c' name 'strend';

    {
    Compare 2 null-terminated strings, case insensitive.
    }
    function tricomp(
        str1 : pchar;
        str2 : pchar
    ): integer; external 'c' name 'strcmp';

    {
    Return the position of a substring in a string, case insensitive.
    }
    function stripos(
        str1 : pchar;
        str2 : pchar
    ): integer; external 'c' name 'stristr';

    {
    Scan a string for a character, case-insensitive
    }
    function striscan(
        str : pchar;
        ch : char
    ): pchar; external 'c' name 'strchr';


    {
    Concatenate 2 null-terminated strings, with length boundary.
    }
    function strlcat(
        dest : pchar;
        src : pchar;
        maxlen : integer
    ): pchar; external 'c' name 'strncat';

    {
    Compare limited number of characters of 2 null-terminated strings

    }
//    function strlcomp(
//      str1: pchar;
//      str2: pchar;
//      l: integer
//    ):integer;

    {
    Copy a null-terminated string, limited in length.
    }

    function strlcopy(
        dest : pchar;
        src : pchar;
        maxlen : integer
    ): pchar; external 'c' name 'strncpy';

    {
    Length of a null-terminated string.
    }
    function strlen(
        str : pchar
    ): int64; external 'c' name 'strlen';

    {
    Compare limited number of characters in 2 null-terminated strings, ignoring case.
    }
    function strlicomp(
      str1: pchar;
      str2: pchar;
      l: integer
    ):integer; external 'c' name 'strncasecmp';

    {
    Convert null-terminated string to all-lowercase.
    }
    function strlower(
        str : pchar
    ): pchar; external 'c' name 'strlwr';

    {
    Move a null-terminated string to new location.
    }
    function strmove(
      dest: pchar;
      source: pchar;
      l: int64
    ):pchar; external 'c' name 'memcpy';


    {
    Allocate room for new null-terminated string.
    }
    function strnew(
        str : pchar
    ): pchar; external 'c' name 'malloc';

    {
    Convert a null-terminated string to a shortstring.
    }
    function strpas(
        str : pchar
    ): string; external 'c' name 'strdup';

    {
    Copy a pascal string to a null-terminated string
    }
    function strpcopy(
        dest : pchar;
        src : string
    ): pchar; external 'c' name 'strcpy';

    {
    Search for a null-terminated substring in a null-terminated string
    }
    function strpos(
        str1 : pchar;
        str2 : pchar
    ): pchar; external 'c' name 'strstr';

    {
    Scan a string reversely for a character, case-insensitive
    }
    function strriscan(
        str : pchar;
        ch : char
    ): pchar; external 'c' name 'strrchr';

    {
    Find last occurrence of a character in a null-terminated string.
    }
    function strrscan(
        str : pchar;
        ch : char
    ): pchar; external 'c' name 'strchr';

    {
    Find first occurrence of a character in a null-terminated string.
    }
    function strscan(
        str : pchar;
        ch : char
    ): pchar; external 'c' name 'strchr';

    {
    Convert null-terminated string to all-uppercase
    }
    function strupper(
        str : pchar
    ): pchar; external 'c' name 'strupr';




implementation
    function malloc(L : int64) : pointer; external 'c' name 'malloc';



    function stralloc(L : int64) : pchar; inline;
    var
        size : int64;
    begin
        size := L; // +1 for null-terminator
        if L < 0 then
            size := 0;
        stralloc := malloc(size + 1); //TODO fix typecast
        if stralloc <> nil then
            stralloc[L] := #0; // Null-terminate the string
    end;

    function AnsiCompareStr(
      S1: string;
      S2: string
    ):Integer;
    begin
        result := CompareStr(S1, S2);
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