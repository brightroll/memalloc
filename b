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

gcc -Wall -g -O0 -c memalloc.c

# For building a LD_PRELOAD version
# gcc -g -O0 -shared -o libmemalloc.so memalloc.c

