#!/usr/bin/perl
open(NB,"|nb roger.nb =")||die;
for($i=1;$i<10;$i++){
  $r=int(rand()*4);
  if($r%2){print(NB "assert a=$r;\n");}
  else{print(NB "assert b=$r;\n");}
  }
close(NB);
