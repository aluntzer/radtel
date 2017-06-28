#!/bin/sh

touch NEWS
aclocal
autoconf
automake --add-missing
