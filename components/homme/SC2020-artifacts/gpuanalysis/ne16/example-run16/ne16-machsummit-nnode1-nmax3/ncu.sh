#!/bin/bash
if [[ "$OMPI_COMM_WORLD_RANK" == "0" ]] ; then
     #exec ${HOME}/Downloads/private/Rebel-Release-public-test/nv-nsight-cu-cli \
     #--nvtx -f -o ncu_${LSB_JOBID}_%p  --kernel-regex-base demangled --kernel-id "::regex:Caar|Euler|Dirk|Boundary:1" --set full --target-processes all $*
     exec ${HOME}/Downloads/private/Rebel-Release-public-test/nv-nsight-cu-cli \
     --nvtx -f -o ncu_${LSB_JOBID}_%p  --kernel-regex-base demangled --kernel-id "::regex:Caar|Euler|Dirk|Boundary:1" --section-folder $PWD --section ManualRoofline --target-processes all $*
else
  exec $*
fi

