#!/bin/sh

touch NEWS
libtoolize
aclocal
autoconf
automake --add-missing
