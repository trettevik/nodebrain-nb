#!/usr/bin/perl
$id=shift();
$msg=shift();
$me=getpwuid($<);
system("mailx -s \"$id: $msg\" $me < note$id.txt");
