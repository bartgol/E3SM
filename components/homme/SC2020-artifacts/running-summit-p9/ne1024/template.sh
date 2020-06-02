#!/bin/bash -f
#BSUB -P cli115
#BSUB -nnodes NNODE
#BSUB -W 02:00  # who knows how much....
#BSUB -J NUMEeNNODE # format to shrink job name
#BSUB -e bsub-NNAME.%J # do not use custom output file?
#BSUB -o bsub-NNAME.%J
#BSUB -alloc_flags smt4

######################## GPU


wdir=`pwd`
echo "PAMI vars before interventions"
source ${wdir}/../../load-modules-p9
module list

echo "PAMI vars after interventions"
env | grep PAMI 

xxdir=${wdir}/../../bldxx-p9-omp/test_execs/

who=theta-nlev128-kokkos
exexx=${xxdir}/${who}/${who}
who=theta-nlev128
exeff=${xxdir}/${who}/${who}

export OMP_PLACES=threads
export OMP_PROC_BIND=close
export OMP_NUM_THREADS=4


#THETA NH
#xx run
jsrun -n NRES -r RESPERNODE -l cpu-cpu \
     -b packed:1 -d plane:1 \
     -a7 \
     -c7 \
     -g0 \
     $exexx \
     < xxinputNH.nl |& grep -v "IEEE_UNDERFLOW_FLAG"

ff=time-thetaNH-xx-NNAME.${LSB_JOBID}

cp job.sh $ff
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



#THETA NH
#ff run
jsrun -n NRES -r RESPERNODE -l cpu-cpu \
     -b packed:1 -d plane:1 \
     -a7 \
     -c7 \
     -g0 \
     $exeff \
     < ffinputNH.nl |& grep -v "IEEE_UNDERFLOW_FLAG"

ff=time-thetaNH-ff-NNAME.${LSB_JOBID}

cp job.sh $ff
echo " " >> $ff
cat xxinputHY.nl >> $ff
echo " " >> $ff
grep COMMIT ${xxdir}/../../CMakeCache.txt >> $ff
grep HOMME ${xxdir}/../../CMakeCache.txt >> $ff
echo " " >> $ff
echo ' job id is ${LSB_JOBID}' >> $ff
head -n 50 HommeTime_stats >> $ff

rm HommeTime_stats






