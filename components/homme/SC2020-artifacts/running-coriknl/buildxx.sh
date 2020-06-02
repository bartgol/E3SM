#!/bin/bash


#for build without mpi on device
#suffix=""
#for build with mpi
suffix=""
#this is to distinguish between 2 builds for ne3072
suffix2=""

wdir=`pwd`             # run directory
HOMME=${HOME}/acme-3km-runs-xxbranch/components/homme                # HOMME svn checkout     
echo $HOMME
bld=$wdir/bldxx${suffix}${suffix2}                    # cmake/build directory

MACH=$HOMME/cmake/machineFiles/cori-knl.cmake
nlev=128
qsize=10

mkdir -p $bld
cd $bld
conf=1
rm $bld/CMakeCache.txt    # remove this file to force re-configure

if [ $conf ]; then
   rm -rf CMakeFiles CMakeCache.txt src
   echo "running CMAKE to configure the model"

   cmake -C $MACH \
   -DQSIZE_D=$qsize -DPREQX_PLEV=$nlev -DPREQX_NP=4  \
   -DBUILD_HOMME_SWEQX=FALSE \
   -DPREQX_USE_ENERGY=FALSE  \
   -DKokkos_ARCH_KNL=ON \
   -DKokkos_ENABLE_SERIAL=TRUE \
   -DKokkos_ENABLE_OPENMP=TRUE \
   -DBUILD_HOMME_WITHOUT_PIOLIBRARY=TRUE \
   -DBUILD_HOMME_THETA_KOKKOS=TRUE \
   -DHOMME_ENABLE_COMPOSE=TRUE \
   -DHOMMEXX_GB_CONFIG=TRUE \
   -DENABLE_OPENMP=TRUE \
   -DENABLE_COLUMN_OPENMP=FALSE \
   -DENABLE_HORIZ_OPENMP=TRUE \
   -DAVX_VERSION="0"	   \
   -DVECTOR_SIZE="8" \
   $HOMME

   make -j4 clean
   make VERBOSE=1 -j32 theta-nlev128 theta-nlev128-kokkos


   #now save build info
   cd ${wdir}
   rm -f gitstat-* CMakeCache.txt-* KokkosCore_config.h-* env-*

   gitstat=gitstat

   cd ${HOMME}
   pwd

   echo " running stats on clone ${which}"
   echo " status is ------------- " > ${gitstat}
   git status >> ${gitstat}
   echo " branch is ------------- " >> ${gitstat}
   git branch >> ${gitstat}
   echo " diffs are ------------- " >> ${gitstat}
   git diff >> ${gitstat}
   echo " last 10 commits are ------------- " >> ${gitstat}
   git log --first-parent  --pretty=oneline  HEAD~10..HEAD  >> ${gitstat}
   cd $wdir
   pwd

   tt=`date +%s`
   mv ${HOMME}/${gitstat} gitstat${suffix2}-${tt}

   cp ${bld}/CMakeCache.txt CMakeCache.txt${suffix2}-${tt}
   cp ${bld}/kokkos/build/KokkosCore_config.h KokkosCore_config.h${suffix2}-${tt}
   env > env${suffix2}-${tt}

fi


