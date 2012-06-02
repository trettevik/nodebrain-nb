#!/usr/bin/perl
chomp($processes=`ps -e|wc -l`);
print("assert processes=$processes;\n");
