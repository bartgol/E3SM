/********************************************************************************
 * HOMMEXX 1.0: Copyright of Sandia Corporation
 * This software is released under the BSD license
 * See the file 'COPYRIGHT' in the HOMMEXX/src/share/cxx directory
 *******************************************************************************/

#ifndef HOMMEXX_LOOPS_UTILS_HPP
#define HOMMEXX_LOOPS_UTILS_HPP

#include "Types.hpp"

namespace Homme {

// Source: https://stackoverflow.com/a/7185723
// Used for iterating over a range of integers
// With this, you can write
// for(int i : int_range(start, end))
template <typename ordered_iterable> class Loop_Range {

public:
  class iterator {
    friend class Loop_Range;

  public:
    KOKKOS_INLINE_FUNCTION
    ordered_iterable operator*() const { return i_; }

    KOKKOS_INLINE_FUNCTION
    const iterator &operator++() {
      ++i_;
      return *this;
    }

    KOKKOS_INLINE_FUNCTION
    iterator operator++(int) {
      iterator copy(*this);
      ++i_;
      return copy;
    }

    KOKKOS_INLINE_FUNCTION
    bool operator==(const iterator &other) const { return i_ == other.i_; }
    KOKKOS_INLINE_FUNCTION
    bool operator!=(const iterator &other) const { return i_ != other.i_; }

  protected:
    KOKKOS_INLINE_FUNCTION
    constexpr iterator(ordered_iterable start) : i_(start) {}

  private:
    ordered_iterable i_;
  };

  KOKKOS_INLINE_FUNCTION
  constexpr iterator begin() const { return begin_; }

  KOKKOS_INLINE_FUNCTION
  constexpr iterator end() const { return end_; }

  KOKKOS_INLINE_FUNCTION
  constexpr int iterations() const { return *end_ - *begin_; }

  KOKKOS_INLINE_FUNCTION
  constexpr Loop_Range(ordered_iterable begin, ordered_iterable end)
      : begin_(begin), end_(end) {}

private:
  iterator begin_;
  iterator end_;
};

// =================== Compute deltas ==================== //
template<typename T, int N>
struct ComputeDiff {
  static_assert (N>1, "Error! Computing vector diffs requires at least 2 entries.\n");
  static constexpr int num_packs = N / VECTOR_SIZE;

  KOKKOS_INLINE_FUNCTION
  static void run (const Homme::TeamMember& team,
                   ExecViewUnmanaged<const T[N]> input,
                   ExecViewUnmanaged<      T[N-1]> output) {
    Kokkos::parallel_for(Kokkos::ThreadVectorRange(team,N-1),
                         [&](const int k) {

      output(k) = input(k+1) - input(k);
    });
  }

  template<typename InputProvider, typename ExecSpaceType, typename... Args>
  KOKKOS_INLINE_FUNCTION
  static typename std::enable_if<!Homme::OnGpu<ExecSpaceType>::value,void>::type
  run (const Homme::TeamMember& team,
       ExecViewUnmanaged<T[N]> input,
       ExecViewUnmanaged<T[N-1]> output,
       const InputProvider& inputProvider,
       const Args... args) {
    Kokkos::parallel_for(Kokkos::ThreadVectorRange(team,N),
                         [&](const int k) {
      inputProvider(input,k,args...);
    });

    run (team,input,output);
  }

  template<typename InputProvider, typename ExecSpaceType, typename... Args>
  KOKKOS_INLINE_FUNCTION
  static typename std::enable_if<Homme::OnGpu<ExecSpaceType>::value,void>::type
  run (const Homme::TeamMember& team,
       ExecViewUnmanaged<T[N]> input,
       ExecViewUnmanaged<T[N-1]> output,
       const InputProvider& inputProvider,
       const Args... args) {
    Kokkos::parallel_for(Kokkos::ThreadVectorRange(team,N-1),
                         [&](const int k) {
      inputProvider(input,k,args...);
      inputProvider(input,k+1,args...);
      output(k) = input(k+1) - input(k);
    });
  }
};

} // namespace Homme

#endif // HOMMEXX_LOOPS_UTILS_HPP
