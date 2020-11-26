#!/bin/bash

WORKSPACE=`pwd`
cd $WORKSPACE/3rd_party

# libosip2
tar xvf libosip2-5.2.0.tar.gz
cd libosip2-5.2.0

./configure --prefix=$WORKSPACE
make install

cd $WORKSPACE/3rd_party
rm -rf libosip2-5.2.0

# libexosip2
tar xvf libexosip2-5.2.0.tar.gz
cd libexosip2-5.2.0

./configure --prefix=$WORKSPACE
make install

cd $WORKSPACE/3rd_party
rm -rf libexosip2-5.2.0

# rm unused files
cd $WORKSPACE

rm -rf share
rm -rf lib/pkgconfig
rm lib/*.dylib || true
rm lib/*.so || true
rm lib/*.la || true