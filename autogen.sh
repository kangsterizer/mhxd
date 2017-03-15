#!/bin/sh
# Run this to generate all the initial makefiles, etc.

srcdir=`dirname $0`
test -z "$srcdir" && srcdir=.

PKG_NAME="hxd"

(test -f $srcdir/configure.in && \
 test -d $srcdir/src \
  && test -f $srcdir/src/common/hxd.c) || {
    echo -n "**Error**: Directory "\`$srcdir\'" does not look like the"
    echo " top-level $PKG_NAME directory"
    exit 1
}

echo "Running libtoolize --copy --force --ltdl"
libtoolize --copy --force --ltdl
NOCONFIGURE=1
. $srcdir/macros/autogen.sh
