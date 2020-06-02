#!/bin/bash

source load-modules-gpu

#for build without mpi on device
#suffix=""
#for build with mpi
suffix="-gpumpi"
#this is to distinguish between 2 builds for ne3072
suffix2="-nocudashare"

wdir=`pwd`             # run directory
HOMME=${HOME}/acme-BRANCH-GB/components/homme                # HOMME svn checkout     
echo $HOMME
bld=$wdir/bldxx${suffix}${suffix2}                    # cmake/build directory

MACH=$HOMME/cmake/machineFiles/summit${suffix}.cmake
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
   -DPREQX_USE_ENERGY=FALSE  $HOMME

   make -j4 clean
   make -j32 theta-nlev128 theta-nlev128-kokkos


   #now save build info
   cd ${wdir}
   rm -f gitstat${suffix2}-* CMakeCache.txt${suffix2}-* KokkosCore_config.h${suffix2}-* env${suffix2}-*

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


