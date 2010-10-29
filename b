#!/bin/bash

# This is now a rake script please run it according or update the rake script on changes
# see lib/tasks/build.rake

OS=`uname -s`

case $OS in
  Linux)
    echo "Compiling for Linux..."
  ;;
  *)
    echo "We do not support other OSs"
    exit 1
  ;;
esac

gcc -g -O0 -c memalloc.c

