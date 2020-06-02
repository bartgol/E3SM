#!/bin/bash

mach=summit
PERNODE=6 

################## these need to be hardcoded for each machine sepatately
nearray=( 1024 ) ;
nodenum=1;

#86400 = 6 * 14400 and we want 6 ranks per summit node
# 14400 = 10 * 1440 = 100 * 144
#summit is 4600 nodes

#load balance 72 144 288 720 1440 2880 
#72 nodes *  6 gpus each is 200 elems per gpu

#ne120nodes=( 18 36 72 144 288 576 1152 2160 ); # 2880 );
#ne30nodes=( 10 15 20 30);

ne1024nodes=( 2048 3072 4096 4600 ); #800 1024 1536 2048 3072 4096 4600)
tstep=10 #ne1024
nuu=2.5e10 #ne1024

#ne3072nodes=( 3072 4096 )
#tstep=3.125 #ne3072
#nuu=9e8     #ne3072

#ne128nodes=( 64 )
#tstep=70
#nuu=1.3e13

########################################################################

## !!!!!!! ## should be this if we start with ne8
#         8  16  32 64 80 120
#nmax1=( 500 120 50 40 20 10 ) ;
nmax1=( 1 ); # 6 is default for ne120, 3 is for ne1024
nmaxfactor=( 4 );

#choices are 6 or 1
respernode=6
if [ $respernode -eq 1 ]; then
  #here -c 42
  acg=1 # -a -g params
fi
#if [ $respernode -eq 6 ]; then
#  acg=1 # -a -g params
#fi

echo "ACG is ${acg}"

count=0
submit=1
currfolder=`pwd`

for nume in ${nearray[@]} ; do

  echo "NE is ${nume}";

  #multinode runs
  nodearray=ne${nume}nodes ;
  eval echo \${$nodearray[@]} ;

  for NN in $(eval echo \${$nodearray[@]}) ; do

    echo "NNode is ${NN}"
    nrank=$(expr $PERNODE \* $NN)

    #multinode runs
    #nmax=$(expr ${nmax1[$count]} \* $NN)
    nmcount=${nmax1[$count]}
    nmfactor=${nmaxfactor[$count]}
    nmax=$(  echo "(( $nmcount * $NN / $nmfactor))" | bc  )

    name="ne${nume}-mach${mach}-nnode${NN}-nmax${nmax}"
    echo "Name is ${name}"
    rundir=run${nume}/${name}
    rm -rf $rundir
    mkdir $rundir
    mkdir $rundir/movies
    cp sab*128*ascii $rundir

    #create a new script

    nres=$(expr $NN \* $respernode )

    hymode=.false.
    ttype=10
    nth=1
    sed -e s/NE/${nume}/ -e s/NMAX/${nmax}/ -e s/TSTEP/${tstep}/ \
        -e s/HYMODE/${hymode}/ \
        -e s/TTYPE/${ttype}/ \
        -e s/NUU/${nuu}/ \
        -e s/NTHREADS/${nth}/ \
        xx-template.nl > $rundir/xxinputNH.nl;

    hymode=.false.
    ttype=10
    nth=-1
    sed -e s/NE/${nume}/ -e s/NMAX/${nmax}/ -e s/TSTEP/${tstep}/ \
        -e s/HYMODE/${hymode}/ \
        -e s/TTYPE/${ttype}/ \
        -e s/NUU/${nuu}/ \
        -e s/NTHREADS/${nth}/ \
        xx-template.nl > $rundir/ffinputNH.nl;


    sed -e s/NNAME/${name}/ -e s/NNODE/${NN}/  \
        -e s/NUME/${nume}/ -e s/NRES/${nres}/ \
        -e s/RESPERNODE/${respernode}/ -e s/ACG/${acg}/ \
        template.sh   >  $rundir/job.sh

    if [[ $submit == 1 ]]; then
       cd $rundir
       bsub job.sh
       cd $currfolder
    fi

  done

  count=$(( count +1));
  #echo "count = $count "
done


