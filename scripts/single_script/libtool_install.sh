# setup environment variables
. environment.setup

VERSION=2.4.2

########################################################################
##								MPC
########################################################################
rm -Rf $PREFIX/libtool-$VERSION
echo "#### Downloading libtool library ####"
wget http://ftpmirror.gnu.org/libtool/libtool-$VERSION.tar.gz 
tar -xzf libtool-$VERSION.tar.gz
cd libtool-$VERSION

echo "#### Building libtool library ####"
./configure --prefix=$PREFIX/libtool-$VERSION
make -j $SLOTS

echo "#### Installing libtool library ####"
make install

rm $PREFIX/libtool-latest
ln -s $PREFIX/libtool-$VERSION $PREFIX/libtool-latest

echo "#### Cleaning up environment ####"
cd ..
rm -Rf libtool-$VERSION*

