/********************************************************************************
 * HOMMEXX 1.0: Copyright of Sandia Corporation
 * This software is released under the BSD license
 * See the file 'COPYRIGHT' in the HOMMEXX/src/share/cxx directory
 *******************************************************************************/

#ifndef MULTI_ELEM_KERNEL_VARIABLES_HPP
#define MULTI_ELEM_KERNEL_VARIABLES_HPP

#include "KernelVariables.hpp"

namespace Homme {

struct MultiElemKernelVariables : public KernelVariables {
private:
  struct TeamInfo {

    template<typename ExecSpaceType>
    static
    KOKKOS_INLINE_FUNCTION
    typename std::enable_if<!std::is_same<ExecSpaceType,Hommexx_Cuda>::value &&
                            !std::is_same<ExecSpaceType,Hommexx_OpenMP>::value,
                            int
                           >::type
    get_team_idx (const int /*team_size*/, const int /*league_rank*/, const int /* leage_size */, const int /* ibuf */) {
      return 0;
    }

#ifdef KOKKOS_ENABLE_CUDA
#ifdef __CUDA_ARCH__
    template <typename ExecSpaceType>
    static KOKKOS_INLINE_FUNCTION typename std::enable_if<
        OnGpu<ExecSpaceType>::value, int>::type
    get_team_idx(const int /*team_size*/, const int league_rank, const int league_size, const int ibuf) {
      return league_rank + ibuf*league_size;
    }
#else
    template <typename ExecSpaceType>
    static KOKKOS_INLINE_FUNCTION typename std::enable_if<
        OnGpu<ExecSpaceType>::value, int>::type
    get_team_idx (const int /*team_size*/, const int /*league_rank*/, const int /* leage_size */, const int /* ibuf */) {
      assert(false); // should never happen
      return -1;
    }
#endif // __CUDA_ARCH__
#endif // KOKKOS_ENABLE_CUDA

#ifdef KOKKOS_ENABLE_OPENMP
    template<typename ExecSpaceType>
    static
    KOKKOS_INLINE_FUNCTION
    typename std::enable_if<std::is_same<ExecSpaceType,Kokkos::OpenMP>::value,int>::type
    get_team_idx (const int team_size, const int /*league_rank*/, const int /* leage_size */, const int /* ibuf */) {
      // Kokkos puts consecutive threads into the same team.
      return omp_get_thread_num() / team_size;
    }
#endif
  };

public:

  KOKKOS_INLINE_FUNCTION
  MultiElemKernelVariables(const TeamMember &team_in)
    : KernelVariables (team_in)
  {
    int team_blk = (NUM_IE*team_in.team_rank()) / team_in.team_size();
    ie = team_in.league_rank() + team_blk*team_in.league_size();
    team_idx = TeamInfo::template get_team_idx<ExecSpace>(team_in.team_size(),team_in.league_rank(),team_in.league_size(),team_blk);
  }

  enum : int { NUM_IE = 2 };

}; // MultiElemKernelVariables

} // Homme

#endif // MULTI_ELEM_KERNEL_VARIABLES_HPP
