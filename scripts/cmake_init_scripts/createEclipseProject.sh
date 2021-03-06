# The directory used by cmake to locate third party libraries
export INSIEME_LIBS_HOME=~/libs 

# optional: set up the CC and CXX compiler to be used
#export CXX=g++
#export CC=gcc

# to include the std-lib header files within you eclipse project you have to specify the path to them
#STD_LIB_DIR=/usr/include/c++/4.6.3

# run cmake to create the project
cmake -G "Eclipse CDT4 - Unix Makefiles" -DCMAKE_BUILD_TYPE=Debug -DGCC_INCLUDE_DIR=$STD_LIB_DIR ../code

