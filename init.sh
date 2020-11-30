#!/bin/bash

WORKSPACE=`pwd`
cd $WORKSPACE/3rd_party

VERSION=4.1.0

# libosip2
tar xvf libosip2-$VERSION.tar.gz
cd libosip2-$VERSION

./configure --prefix=$WORKSPACE
make install

cd $WORKSPACE/3rd_party
rm -rf libosip2-$VERSION

# libexosip2
tar xvf libeXosip2-$VERSION.tar.gz
cd libeXosip2-$VERSION

./configure --prefix=$WORKSPACE LDFLAGS="-L$WORKSPACE/lib" CFLAGS="-L$WORKSPACE/include"
make install

cd $WORKSPACE/3rd_party
rm -rf libeXosip2-$VERSION

# rm unused files
cd $WORKSPACE

rm -rf share
rm -rf lib/pkgconfig
rm lib/*.dylib || true
rm lib/*.so* || true
rm lib/*.la || true