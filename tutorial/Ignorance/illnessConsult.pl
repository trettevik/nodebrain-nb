#!/usr/bin/perl
# File: tutorial/Ignorance/illnessConsult.pl
$state{"cough"}=1;
$state{"soreThroat"}=1;
$state{"fever"}=0;
$state{"achy"}=1;
$state{"upsetStomach"}=0;

$symptom=shift;
if(exists($state{$symptom})){print("$state{$symptom}\n");}
else{print("??\n");}
