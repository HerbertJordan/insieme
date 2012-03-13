# setup environment variables
. ../environment.setup

VERSION=3.1.0

########################################################################
##								MPFR
########################################################################
rm -Rf $PREFIX/mpfr-$VERSION
echo "#### Downloading MPFR library ####"
wget http://mpfr.loria.fr/mpfr-current/mpfr-$VERSION.tar.gz
tar -xzf mpfr-$VERSION.tar.gz
cd mpfr-$VERSION

echo "#### Building MPFR library ####"
./configure --prefix=$PREFIX/mpfr-$VERSION --with-gmp=$PREFIX/gmp-latest 
make -j $SLOTS
make check

echo "#### Installing MPFR library ####"
make install 

rm $PREFIX/mpfr-latest
ln -s $PREFIX/mpfr-$VERSION $PREFIX/mpfr-latest

echo "#### Cleaning up environment ####"
cd ..
rm -R mpfr-$VERSION*
