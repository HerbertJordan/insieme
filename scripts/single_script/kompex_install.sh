# setup environment variables
. ../environment.setup

VERSION=1.7.9

########################################################################
##                                                      KOMPEX 
########################################################################
echo "#### Downloading KOMPEX library ####"
wget http://sqlitewrapper.kompex-online.com/counter/download.php?dl=KompexSQLiteWrapper-Source_$VERSION.tar.gz --output-document=KompexSQLiteWrapper-Source_$VERSION.tar.gz
tar -xzf KompexSQLiteWrapper-Source_$VERSION.tar.gz
cd KompexSQLiteWrapper-Source_$VERSION/Kompex\ SQLite\ Wrapper
echo "#### Building KOMPEX library ####"
make CXX="g++ -fPIC" CC="gcc -fPIC" -j$SLOTS

cd ..
echo "#### Installing KOMPEX library ####"

rm -Rf $PREFIX/kompex-$VERSION
rm $PREFIX/kompex-latest
mkdir $PREFIX/kompex-$VERSION
cp -R ./lib $PREFIX/kompex-$VERSION
cp -R ./inc $PREFIX/kompex-$VERSION

ln -s $PREFIX/kompex-$VERSION/lib/debug/KompexSQLiteWrapper_Static_d.a $PREFIX/kompex-$VERSION/lib/libKompexSQLiteWrapper_Static_d.a
ln -s $PREFIX/kompex-$VERSION $PREFIX/kompex-latest

echo "#### Cleaning up environment ####"
cd ..
rm -R KompexSQLiteWrapper-Source_$VERSION*