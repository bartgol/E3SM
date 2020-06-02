#!/bin/bash -f
#BSUB -P cli115
#BSUB -nnodes NNODE
#BSUB -W 00:30  # who knows how much....
#BSUB -J NUMEeNNODE # format to shrink job name
#BSUB -e bsub-NNAME.%J # do not use custom output file?
#BSUB -o bsub-NNAME.%J
#BSUB -alloc_flags smt1

######################## GPU

me="$(basename $0)"


wdir=`pwd`
source ${wdir}/../../load-modules-gpu
module list

echo "PAMI vars after load"
env | grep PAMI 

xxdir=${wdir}/../../bldxx-gpumpi/test_execs/theta-nlev128-kokkos
exexx=${xxdir}/theta-nlev128-kokkos



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

cp ${me} $ff
echo " " >> $ff
cat xxinputNH.nl >> $ff
echo " " >> $ff
grep define ${xxdir}/../../kokkos/build/KokkosCore_config.h >> $ff
grep COMMIT ${xxdir}/../../CMakeCache.txt >> $ff
grep HOMME ${xxdir}/../../CMakeCache.txt >> $ff
echo " " >> $ff
echo ' job id is ${LSB_JOBID}' >> $ff
head -n 50 HommeTime_stats >> $ff

rm HommeTime_stats






