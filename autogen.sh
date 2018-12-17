#!/bin/sh

autoreconf -fvi
./configure $@

echo
echo "Source code configured. Please compile it with"
echo "$ make -j`nproc`"
