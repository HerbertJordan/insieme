
PREFIX=/home/dev/insieme-deps/
SLOTS=8

VERSION=2.0.0-beta9

# using default system compiler
CC=${CC:-gcc}
CXX=${CXX:-g++}

########################################################################
##								LuaJIT
########################################################################
rm -Rf $PREFIX/luajit-$VERSION
echo "#### Downloading LuaJIT library ####"
wget http://luajit.org/download/LuaJIT-$VERSION.tar.gz
tar -xzf LuaJIT-$VERSION.tar.gz
cd LuaJIT-$VERSION

echo "#### Building LuaJIT library ####"
make PREFIX=$PREFIX/luajit-$VERSION -j $SLOTS

echo "#### Installing LuaJIT library ####"
make install PREFIX=$PREFIX/luajit-$VERSION

ln -sf $PREFIX/luajit-$VERSION $PREFIX/luajit-latest

echo "#### Cleaning up environment ####"
cd ..
rm -R LuaJIT-$CUDD_VER*




