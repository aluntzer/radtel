#!/bin/sh

touch NEWS
if [[ $(uname) == "Darwin" ]]; then
 glibtoolize
else
 libtoolize
fi
aclocal
autoconf
automake --add-missing
