#!/usr/bin/perl
use FileHandle;
open(NB,"|nb jeeves.nb -s")||die;  # use -s (servant) option
NB->autoflush(1);       # force output to nb as soon as we send a command
for($i=1;$i<10;$i++){
  $r=int(rand()*4);
  if($r%2){print(NB "assert a=$r;\n");}
  else{print(NB "assert b=$r;\n");}
  sleep(5);             # pretend like we are working on something
  }
print(NB "stop\n");    # stop the servant mode nb
close(NB);
