
# memalloc

This is a simple library that hooks into modern malloc implementations
and allows the programmer to tag each allocation with a string.
This is useful in core file analysis. With a simple script, you can
easily identify which part of the system is responsible for the majority
of allocations that were in the process.

The overhead to allocation should be small. Because it hooks malloc,
all allocations that occur with a library also use the library.
At this time, we have only tested it on 32 bit builds.

We developed and used this in production at Brightroll.com. 

## license

see the file COPYING (basically MIT license)


## installation

put instructions here

## usage

put usage here

