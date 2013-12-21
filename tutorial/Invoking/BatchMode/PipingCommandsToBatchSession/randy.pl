#!/usr/bin/perl
for($i=1;$i<10;$i++){
  $r=int(rand()*4);
  if($r%2){print("assert a=$r;\n");}
  else{print("assert b=$r;\n");}
  }
