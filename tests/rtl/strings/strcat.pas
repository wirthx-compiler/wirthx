Program strcat;

Uses strings,system;

{ Program to demonstrate the StrCat function. }

Const P1 : PChar = 'This is a PChar String.';

Var P2 : PChar;

begin
  Writeln ('P1 : ',P1);
  P2:=StrAlloc (StrLen(P1)*2+1);

  StrMove (P2,P1,StrLen(P1)+1); { P2=P1 }
  StrCat (P2,P1);               { Append P2 once more }
  Writeln ('P2 : ',P2);
  StrDispose(P2);
end.