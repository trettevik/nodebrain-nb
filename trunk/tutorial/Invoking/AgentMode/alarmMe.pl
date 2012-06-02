#!/usr/bin/perl
# File: tutorial/AgentMode/alarmMe.pl
$id=shift();
$msg=shift();
$me=getpwuid($<);
system("mailx -s \"$id: $msg\" $me < note$id.txt");
