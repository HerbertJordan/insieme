# setup environment variables
. ./environment.setup

########################################################################
##								GMP
########################################################################

VERSION=5.0.5
PACKAGE=gmp-$VERSION
FILE=$PACKAGE.tar.bz2

if [ -d $PREFIX/gmp-$VERSION ]; then
  echo "GMP version $VERSION already installed"
  exit 0
fi

echo "#### Downloading GMP library ####"
wget -nc ftp://ftp.gmplib.org/pub/gmp-$VERSION/$FILE

RET=$?
if [ $RET -ne 0 ]; then
	exit $RET
fi

tar -xf $FILE
cd $PACKAGE

echo "#### Building GMP library ####"
CFLAGS="-mtune=native -O3" ./configure --prefix=$PREFIX/gmp-$VERSION --enable-cxx
make -j $SLOTS
make check

# Check for failure
RET=$?
if [ $RET -ne 0 ]; then
	exit $RET
fi

# Remove any previous installation dir
rm -Rf $PREFIX/$PACKAGE

echo "#### Installing GMP library ####"
make install 

rm $PREFIX/gmp-latest
ln -s $PREFIX/$PACKAGE $PREFIX/gmp-latest

echo "#### Cleaning up environment ####"
cd ..
rm -R $PACKAGE*

exit 0

