#!/bin/sh

#/bin/sh

VER='4.11'
#specify where to install by PREFIX or COMMAND-LINE ARG1
PREFIX="$PWD/libev-${VER}fixed"
if [ -n "$1" ]; then
        PREFIX=$1
fi
SRCDIR=libev-${VER}
LIBNAME="libev-${VER}"
if [ -d "$SRCDIR" ]; then
        rm -rf $SRCDIR
fi
file=$LIBNAME.tar.gz
if [ ! -f "$file" ]; then
        echo "error: no $file"
        exit 1
fi
tar xvzf $file
if [ ! -d "$SRCDIR" ]; then
        echo "error: no $SRCDIR"
        exit 1
fi

#cp libev_auxten.patch $SRCDIR/libev_auxten.patch
cd $SRCDIR
#patch -p1 < libev_auxten.patch

./configure --prefix=$PREFIX --enable-shared=no  --enable-static

make; make install
cd ..

rm -rf libev
ln -s libev-${VER}fixed libev
#rm -rf $SRCDIR

echo "done!"


