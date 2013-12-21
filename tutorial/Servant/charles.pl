#!/usr/bin/perl
# File: tutorial/Servant/charles.pl
my $gas=3.25;
my $bread=2.10;
$|=1;
while(<>){
  chomp($_);
  if(/gas/){print("assert gas=$gas;\n");$gas+=.50;}
  elsif(/bread/){print("assert bread=$bread;\n");$bread+=.25}
  else{print("alert msg=\"item '$_' not recognized\";\n");}
  }
