#!/bin/bash -f
#SBATCH --account=e3sm
#SBATCH -p regular # NOTE for debug que chaining (script to submit a script) is not allowed
#SBATCH --error=slurm-NNAME.%j # do not use custom output file?
#SBATCH --output=slurm-NNAME.%j
#SBATCH --nodes=NNODE
#SBATCH -C knl
#SBATCH -t 00:120:00  # who knows how much....
#SBATCH --job-name=NUMEkNNODE # format to shrink job name


wdir=`pwd`

xxdir=${wdir}/../../bldxx/test_execs
exexx=${xxdir}/theta-nlev128-kokkos/theta-nlev128-kokkos
exeff=${xxdir}/theta-nlev128/theta-nlev128


export OMP_NUM_THREADS=NTHR
export KMP_AFFINITY=balanced
export OMP_STACKSIZE=16M


#ff SL run
nname=ff-sl-nh
nnl=${nname}.nl
srun -N NNODE \
     -n NRANK \
     -c CFLAG \
     --cpu_bind=cores \
     $exeff \
     < $nnl |& grep -v "KMP_AFFINITY:"
     #|& grep -v "IEEE_UNDERFLOW_FLAG"

ff=time-${nname}-NNAME.${SLURM_JOB_ID}

cp job.sh $ff
echo " " >> $ff
cat $nnl >> $ff
echo " " >> $ff
#grep define ${xxdir}/../kokkos/build/KokkosCore_config.h >> $ff
#grep COMMIT ${xxdir}/../CMakeCache.txt >> $ff
#grep HOMME ${xxdir}/../CMakeCache.txt >> $ff
echo " " >> $ff
echo " job id is ${SLURM_JOB_ID}" >> $ff
head -n 50 HommeTime_stats >> $ff

rm HommeTime_stats



#ff EUL run
nname=ff-eul-nh
nnl=${nname}.nl
srun -N NNODE \
     -n NRANK \
     -c CFLAG \
     --cpu_bind=cores \
     $exeff \
     < $nnl |& grep -v "KMP_AFFINITY:"
     #|& grep -v "IEEE_UNDERFLOW_FLAG"

ff=time-${nname}-NNAME.${SLURM_JOB_ID}

cp job.sh $ff
echo " " >> $ff
cat $nnl >> $ff
echo " " >> $ff
#grep define ${xxdir}/../kokkos/build/KokkosCore_config.h >> $ff
#grep COMMIT ${xxdir}/../CMakeCache.txt >> $ff
#grep HOMME ${xxdir}/../CMakeCache.txt >> $ff
echo " " >> $ff
echo " job id is ${SLURM_JOB_ID}" >> $ff
head -n 50 HommeTime_stats >> $ff

rm HommeTime_stats



#THETA NH
#xx EUL run
nname=xx-eul-nh
nnl=${nname}.nl
srun -N NNODE \
     -n NRANK \
     -c CFLAG \
     --cpu_bind=cores \
     $exexx \
     < $nnl |& grep -v "KMP_AFFINITY:"
     #|& grep -v "IEEE_UNDERFLOW_FLAG"

ff=time-${nname}-NNAME.${SLURM_JOB_ID}

cp job.sh $ff
echo " " >> $ff
cat $nnl >> $ff
echo " " >> $ff
grep define ${xxdir}/../../kokkos/build/KokkosCore_config.h >> $ff
grep COMMIT ${xxdir}/../../CMakeCache.txt >> $ff
grep HOMME ${xxdir}/../../CMakeCache.txt >> $ff
echo " " >> $ff
echo " job id is ${SLURM_JOB_ID}" >> $ff
head -n 50 HommeTime_stats >> $ff

rm HommeTime_stats







