#!/bin/bash

mach=coriknl

################## these need to be hardcoded for each machine sepatately
nearray=( 1024 ) ;
nodenum=1;

#cori is 9688 nodes
#1024*1024*6 elems = 64* 16*1024*6 elements, so, good node numbers are
#1024 or its factors, or 1024*(factors of 16*6)


ne1024nodes=( 512 1024 2048 3072 4096 6144 8192 9216  );
tstep=10 #ne1024
nuu=2.5e10 #ne1024

#ne3072nodes=( 3072 4096 )
#tstep=3.125 #ne3072
#nuu=9e8     #ne3072

#ne128nodes=( 64 )
#tstep=70
#nuu=1.3e13

#nearray=( 50 ); #nmax 100
#ne50nodes=( 10 );
#tstep=10
#nuu=2e14

#nearray=( 48 ); #nmax 100
#ne48nodes=( 10 20 );
#tstep=10
#nuu=2e14


########################################################################

## !!!!!!! ## should be this if we start with ne8
#         8  16  32 64 80 120
#nmax1=( 500 120 50 40 20 10 ) ;
nmax1=( 1 ); # 6 is default for ne120, 3 is for ne1024
nmaxfactor=( 1 );


PERNODE=64
nthr=2
cflag=4

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


    echo $nmax



    name="ne${nume}-mach${mach}-nnode${NN}-nmax${nmax}"
    echo "Name is ${name}"
    rundir=run${nume}/${name}
    rm -rf $rundir
    mkdir $rundir
    mkdir $rundir/movies
    cp sab*128*ascii $rundir

    #create a new script

echo "before F SL nl"
    hymode=.false.
    ttype=10
    nthreads=-1
    sed -e s/NE/${nume}/ -e s/NMAX/${nmax}/ -e s/TSTEP/${tstep}/ \
        -e s/HYMODE/${hymode}/ \
        -e s/TTYPE/${ttype}/ \
        -e s/NUU/${nuu}/ \
        -e s/NTHREADS/${nthreads}/ \
        sl-template.nl > $rundir/ff-sl-nh.nl;

echo "before F EUL nl"
    hymode=.false.
    ttype=10
    nthreads=-1
    sed -e s/NE/${nume}/ -e s/NMAX/${nmax}/ -e s/TSTEP/${tstep}/ \
        -e s/HYMODE/${hymode}/ \
        -e s/TTYPE/${ttype}/ \
        -e s/NUU/${nuu}/ \
        -e s/NTHREADS/${nthreads}/ \
        euler-template.nl > $rundir/ff-eul-nh.nl;


echo "before XX EUL nl"
    hymode=.false.
    ttype=10
    nthreads=1
    sed -e s/NE/${nume}/ -e s/NMAX/${nmax}/ -e s/TSTEP/${tstep}/ \
        -e s/HYMODE/${hymode}/ \
        -e s/TTYPE/${ttype}/ \
        -e s/NUU/${nuu}/ \
        -e s/NTHREADS/${nthreads}/ \
        euler-template.nl > $rundir/xx-eul-nh.nl;


    sed -e s/NNAME/${name}/ -e s/NNODE/${NN}/  -e s/NRANK/${nrank}/ \
        -e s/NUME/${nume}/  -e s/NTHR/${nthr}/ -e s/CFLAG/${cflag}/ \
        template.sh   >  $rundir/job.sh

    if [[ $submit == 1 ]]; then
       cd $rundir
       sbatch job.sh
       cd $currfolder
    fi

  done

  count=$(( count +1));
  #echo "count = $count "
done


