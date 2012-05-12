#!/bin/sh

NAME='libev'
VER='4.11'
TYPE='tar.gz'
CONFIGURE_FLAGS=''

#specify where to install by PREFIX or COMMAND-LINE ARG1
if [ -n "$1" ]; then
        PREFIX=$1
else
		PREFIX="$PWD/${NAME}-${VER}built"
fi
SRCDIR=${NAME}-${VER}
LIBNAME="${NAME}-${VER}"
if [ -d "${SRCDIR}" ]; then
        rm -rf ${SRCDIR}
fi
FILE=${LIBNAME}.${TYPE}
if [ ! -f "${FILE}" ]; then
        echo "error: no ${FILE}"
        exit 1
fi
tar xvzf ${FILE}
if [ ! -d "${SRCDIR}" ]; then
        echo "error: no ${SRCDIR}"
        exit 1
fi

#cp ${NAME}_auxten.patch ${SRCDIR}/${NAME}_auxten.patch
cd ${SRCDIR}
#patch -p1 < ${NAME}_auxten.patch

./configure --prefix=${PREFIX} --enable-shared=no  --enable-static ${CONFIGURE_FLAGS}

make; make install
cd ..

rm -rf ${NAME}
ln -s ${NAME}-${VER}built ${NAME}
#rm -rf ${SRCDIR}

echo "done!"


