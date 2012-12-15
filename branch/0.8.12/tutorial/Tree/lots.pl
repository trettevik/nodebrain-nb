#!/usr/bin/perl
open(NB,"|nb =")||die;
print(NB "define things node tree;\n");
for(my $i=1;$i<1000;$i++){
  for(my $j=2;$j<1000;$j++){
    my $v=$i*$j;
    print(NB "assert things(\"a$i\",\"b$j\")=$v;\n");
    }
  }
print(NB "dkfjdkj\n");
print(NB "assert x=things(\"a50\",\"b50\");\n");
print(NB "show x;\n");
print(NB "dkfjdkj\n");
print(NB "assert x=things(\"a500\",\"b500\");\n");
print(NB "show x;\n");
close(NB);
