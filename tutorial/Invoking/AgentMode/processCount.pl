#!/usr/bin/perl
# File: tutorial/AgentMode/processCount.pl
chomp($processes=`ps -e|wc -l`);
print("assert processes=$processes;\n");
