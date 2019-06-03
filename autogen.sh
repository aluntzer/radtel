#!/bin/bash

touch NEWS
if [[ $(uname) == "Darwin" ]]; then
	glibtoolize
elif [[ $(uname) != "MINGW"* ]]; then
	libtoolize
fi

aclocal
autoconf
automake --add-missing
