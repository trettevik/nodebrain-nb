#!/usr/bin/perl
# File: tutorial/Ignorance/illnessConsult.pl
$state{"cough"}='!!';
$state{"soreThroat"}='!!';
$state{"fever"}='!';
$state{"achy"}='!!';
$state{"upsetStomach"}='!';

$symptom=shift;
if(exists($state{$symptom})){print("$state{$symptom}\n");}
else{print("?\n");}
