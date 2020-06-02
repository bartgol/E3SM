#!/bin/bash -f
#BSUB -P cli115
#BSUB -nnodes NNODE
#BSUB -W 00:60  # who knows how much....
#BSUB -J NUMEeNNODE # format to shrink job name
#BSUB -e bsub-NNAME.%J # do not use custom output file?
#BSUB -o bsub-NNAME.%J
#BSUB -alloc_flags smt1

######################## GPU


wdir=`pwd`
source ${wdir}/../../load-modules-gpu
module list

echo "PAMI vars now "
env | grep PAMI 

xxdirN=${wdir}/../../bldxx-gpumpi-nocudashare/test_execs/theta-nlev128-kokkos
xxdirY=${wdir}/../../bldxx-gpumpi-yescudashare/test_execs/theta-nlev128-kokkos
exexxN=${xxdirN}/theta-nlev128-kokkos
exexxY=${xxdirY}/theta-nlev128-kokkos

howmany=NNODE
#logic for which exec to use
if [[ $howmany -le 3500 ]] ; then
xxdir=${xxdirY}
exexx=$exexxY
else
xxdir=${xxdirN}
exexx=$exexxN
fi


#THETA NH
#xx run
jsrun -n NRES -r RESPERNODE -l gpu-gpu \
     -b packed:1 -d plane:1 \
     -a1 \
     -c7 \
     -g1 --smpiargs "-gpu" \
     $exexx \
     < xxinputNH.nl |& grep -v "IEEE_UNDERFLOW_FLAG"

ff=time-thetaNH-NNAME.${LSB_JOBID}

cp job.sh $ff
echo " " >> $ff
cat xxinputNH.nl >> $ff
echo " " >> $ff
grep define ${xxdir}/../../kokkos/build/KokkosCore_config.h >> $ff
grep COMMIT ${xxdir}/../../CMakeCache.txt >> $ff
grep HOMME ${xxdir}/../../CMakeCache.txt >> $ff
echo " " >> $ff
echo " job id is ${LSB_JOBID}" >> $ff
head -n 50 HommeTime_stats >> $ff

rm HommeTime_stats





