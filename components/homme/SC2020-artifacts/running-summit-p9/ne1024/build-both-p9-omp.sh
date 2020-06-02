#!/bin/bash

#for build without mpi on device
#suffix=""
#for build with mpi

source load-modules-p9

suffix="-p9-omp"


wdir=`pwd`             # run directory
HOMME=${HOME}/acme-BRANCH-GB/components/homme                # HOMME svn checkout     
echo $HOMME
bld=$wdir/bldxx${suffix}                     # cmake/build directory

MACH=$HOMME/cmake/machineFiles/summit${suffix}.cmake
nlev=128
qsize=10

mkdir -p $bld
cd $bld
build=1  # set to 1 to force build
conf=1
rm $bld/CMakeCache.txt    # remove this file to force re-configure

if [ $conf ]; then
   rm -rf CMakeFiles CMakeCache.txt src
   echo "running CMAKE to configure the model"

   cmake -C $MACH \
   -DQSIZE_D=$qsize -DPREQX_PLEV=$nlev -DPREQX_NP=4  \
   -DBUILD_HOMME_SWEQX=FALSE \
   -DPREQX_USE_ENERGY=FALSE  $HOMME

   make -j4 clean
   make -j32 theta-nlev128 theta-nlev128-kokkos
   exit
fi

