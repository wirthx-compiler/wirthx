# wirthx

Wirthx is an experimental pascal compiler.
The language is named after Niklaus Wirth the creator of pascal.

## Compiler

The compiler is based on llvm and will generate a native binary for the target plattform.
For now only `linux-x86-64` and `win64` are supported.

## Current Restrictions

> [!WARNING]
> - only ascii characters are allowed in the source code
> - no support for `set` types
> - no support for `file` types with a sub type
> - no support for `packed` types
> - no support for `class` or `object` types

### Options

| **Option** 	   | **Values** 	 | **Description**                                  	                        |
|----------------|--------------|---------------------------------------------------------------------------|
| --run <br>-r 	 | 	            | Runs the compiled program                        	                        |
| --debug    	   | 	            | Creates a debug build                            	                        |
| --release  	   | 	            | Creates a release build                          	                        |
| --rtl      	   | path       	 | sets the path for the rtl (run time library)     	                        |
| --output   	   | path       	 | sets the output / build directory                	                        |
| --llvm-ir  	   | 	            | Outputs the LLVM-IR to the standard error output 	                        |
| --help         |              | Outputs the program help                                                  |
| --version      |              | Prints the current version of the compiler                                |
| --lsp          |              | runs the compiler in the language server mode                           	 |

# Usage

```sh
wirthx testfiles/hello.pas
```

## Executing the compiler

The compiler will generate a native executable based on the program name defined in the program unit.

```sh
wirthx -c testfiles/hello.pas
```

# Examples

## Hello World

```pascal
program test;

begin
    Writeln('Hello World');
end.
```

## Functions

```pascal
program test;

    function addx(a : integer;b :integer): integer;
    begin
        addx := a + b;
    end;
var
    my_var : integer;
begin
    my_var := addx(1,2);
    Writeln(my_var);
end.
```

## Procedures

```pascal
program test;

    procedure my_inc(var value: integer);
    begin
        value := value + 1;
    end;
var
    my_var : integer := 10;
begin
    my_inc(my_var);
    Writeln(my_var);
end.
```

## Records

```pascal
program test;

    type Vec2 = record
        x : int64;
        y : int64;
    end;

    // pass the vector as a reference 
    procedure vec2_inc(var t : Vec2); 
    begin
        t.x := t.x + 1;
        t.y := t.y + 1;
    end;
var
    myvec : Vec2;
begin
    myvec.x := 2;
    myvec.y := 3;
    vec2_inc(myvec);
end.
```

## Conditions

```pascal
program test;
var
    test : integer = 20;
begin
    if test mod 2 then
        writeln('20 is divisible by 2 without a reminder.');
end.
```

## For - Loops

```pascal
program test;
var
    i : integer;
begin
    for i:= 1 to 20 do
        writeln(i);
end.
```

## While Loops

```pascal
program test;
var
    loop_var : integer = 20;
begin
    while loop_var > 0 do
    begin
        writeln(loop_var);
        loop_var := loop_var - 1;
    end;
end.
```