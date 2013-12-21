#!/bin/sh
read foo
while test $? = 0; do
  echo $foo
  read foo
  done
