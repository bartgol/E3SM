#ifndef HOMME_STATIC_ARRAY_HPP
#define HOMME_STATIC_ARRAY_HPP

#include <type_traits>

#include <impl/Kokkos_ViewMapping.hpp>

namespace Homme {

template<typename DataType>
struct StaticArray {
private:
  using array_analysis = Kokkos::Impl::ViewArrayAnalysis<DataType>;
  using data_analysis  = Kokkos::Impl::ViewDataAnalysis<DataType,Kokkos::LayoutRight,typename array_analysis::value_type>;

  enum : int { rank_dynamic = array_analysis::dynamic_dimension::rank };
  static_assert (rank_dynamic==0, "Error! No dynamic dimensions allowed for a static array.\n");

public:

  enum : int { Rank = array_analysis::dimension::rank };
  static_assert (Rank>0, "Error! Cannot use StaticArray for rank-0 arrays.\n");

  using data_type             = typename data_analysis::type;
  using const_data_type       = typename data_analysis::const_type;
  using nonconst_data_type    = typename data_analysis::non_const_type;

  using value_type            = typename data_analysis::value_type;
  using const_value_type      = typename data_analysis::const_value_type;
  using nonconst_value_type   = typename data_analysis::non_const_value_type;

  using reference_type        = value_type&;
  using const_reference_type  = const_value_type&;

  using pointer_type          = value_type*;
  using pointer_to_const_type = const_value_type*;

  // Default some common methods
  StaticArray () = default;
  StaticArray (const StaticArray&) = default;
  StaticArray& operator= (const StaticArray&) = default;

  StaticArray (const_data_type& data) : m_data(data) {}
  StaticArray& operator= (const_data_type& data_in) { *this = StaticArray(data_in); }

  // Allow implicit conversion from non-const to const
  operator StaticArray<const_data_type> () { return StaticArray<const_data_type>(m_data); }

  data_type&            array ()       { return m_data; }
  const_data_type&      array () const { return m_data; }
  pointer_type          data  ()       { return data_impl<Rank>(); }
  pointer_to_const_type data  () const { return data_impl<Rank>(); }

  int size () const { return size_impl<DataType>(); }

  // ================= Access opertor's ================ //

  // Rank 1
  template<typename I0>
  KOKKOS_FORCEINLINE_FUNCTION
  typename std::enable_if<std::is_integral<I0>::value &&
                          ( 1 == Rank ),
                          reference_type >::type
  operator()(const I0 i0) {
    return m_data[i0];
  } 
  template<typename I0>
  KOKKOS_FORCEINLINE_FUNCTION
  typename std::enable_if<std::is_integral<I0>::value &&
                          ( 1 == Rank ),
                          const_reference_type >::type
  operator()(const I0 i0) const {
    return m_data[i0];
  } 

  // Rank 2
  template<typename I0, typename I1>
  KOKKOS_FORCEINLINE_FUNCTION
  typename std::enable_if<std::is_integral<I0>::value &&
                          std::is_integral<I1>::value &&
                          ( 2 == Rank ),
                          reference_type >::type
  operator()(const I0 i0, const I1 i1) {
    return m_data[i0][i1];
  } 
  template<typename I0, typename I1>
  KOKKOS_FORCEINLINE_FUNCTION
  typename std::enable_if<std::is_integral<I0>::value &&
                          std::is_integral<I1>::value &&
                         ( 2 == Rank ),
                          const_reference_type >::type
  operator()(const I0 i0, const I1 i1) const {
    return m_data[i0][i1];
  } 

  // Rank 3
  template<typename I0, typename I1, typename I2>
  KOKKOS_FORCEINLINE_FUNCTION
  typename std::enable_if<std::is_integral<I0>::value &&
                          std::is_integral<I1>::value &&
                          std::is_integral<I2>::value &&
                          ( 3 == Rank ),
                          reference_type >::type
  operator()(const I0 i0, const I1 i1, const I2 i2) {
    return m_data[i0][i1][i2];
  } 
  template<typename I0, typename I1, typename I2>
  KOKKOS_FORCEINLINE_FUNCTION
  typename std::enable_if<std::is_integral<I0>::value &&
                          std::is_integral<I1>::value &&
                          std::is_integral<I2>::value &&
                          ( 3 == Rank ),
                          const_reference_type >::type
  operator()(const I0 i0, const I1 i1, const I2 i2) const {
    return m_data[i0][i1][i2];
  } 

  // Rank 4
  template<typename I0, typename I1, typename I2, typename I3>
  KOKKOS_FORCEINLINE_FUNCTION
  typename std::enable_if<std::is_integral<I0>::value &&
                          std::is_integral<I1>::value &&
                          std::is_integral<I2>::value &&
                          std::is_integral<I3>::value &&
                          ( 4 == Rank ),
                          reference_type >::type
  operator()(const I0 i0, const I1 i1, const I2 i2, const I3 i3) {
    return m_data[i0][i1][i2][i3];
  } 
  template<typename I0, typename I1, typename I2, typename I3>
  KOKKOS_FORCEINLINE_FUNCTION
  typename std::enable_if<std::is_integral<I0>::value &&
                          std::is_integral<I1>::value &&
                          std::is_integral<I2>::value &&
                          std::is_integral<I3>::value &&
                          ( 4 == Rank ),
                          const_reference_type >::type
  operator()(const I0 i0, const I1 i1, const I2 i2, const I3 i3) const {
    return m_data[i0][i1][i2][i3];
  } 

  // Rank 5
  template<typename I0, typename I1, typename I2, typename I3, typename I4>
  KOKKOS_FORCEINLINE_FUNCTION
  typename std::enable_if<std::is_integral<I0>::value &&
                          std::is_integral<I1>::value &&
                          std::is_integral<I2>::value &&
                          std::is_integral<I3>::value &&
                          std::is_integral<I4>::value &&
                          ( 5 == Rank ),
                          reference_type >::type
  operator()(const I0 i0, const I1 i1, const I2 i2, const I3 i3, const I4 i4) {
    return m_data[i0][i1][i2][i3][i4];
  } 
  template<typename I0, typename I1, typename I2, typename I3, typename I4>
  KOKKOS_FORCEINLINE_FUNCTION
  typename std::enable_if<std::is_integral<I0>::value &&
                          std::is_integral<I1>::value &&
                          std::is_integral<I2>::value &&
                          std::is_integral<I3>::value &&
                          std::is_integral<I4>::value &&
                          ( 5 == Rank ),
                          const_reference_type >::type
  operator()(const I0 i0, const I1 i1, const I2 i2, const I3 i3, const I4 i4) const {
    return m_data[i0][i1][i2][i3][i4];
  } 
private:

  DataType m_data;

  template<int RankIn>
  typename std::enable_if<RankIn==1,pointer_type>::type
  data_impl () { return &(*this)(0); }

  template<int RankIn>
  typename std::enable_if<RankIn==1,pointer_to_const_type>::type
  data_impl () const { return &(*this)(0); }

  template<int RankIn>
  typename std::enable_if<RankIn==2,pointer_type>::type
  data_impl () { return &(*this)(0,0); }

  template<int RankIn>
  typename std::enable_if<RankIn==2,pointer_to_const_type>::type
  data_impl () const { return &(*this)(0,0); }

  template<int RankIn>
  typename std::enable_if<RankIn==3,pointer_type>::type
  data_impl () { return &(*this)(0,0,0); }

  template<int RankIn>
  typename std::enable_if<RankIn==3,pointer_to_const_type>::type
  data_impl () const { return &(*this)(0,0,0); }

  template<int RankIn>
  typename std::enable_if<RankIn==4,pointer_type>::type
  data_impl () { return &(*this)(0,0,0,0); }

  template<int RankIn>
  typename std::enable_if<RankIn==4,pointer_to_const_type>::type
  data_impl () const { return &(*this)(0,0,0,0); }

  template<int RankIn>
  typename std::enable_if<RankIn==5,pointer_type>::type
  data_impl () { return &(*this)(0,0,0,0,0); }

  template<int RankIn>
  typename std::enable_if<RankIn==5,pointer_to_const_type>::type
  data_impl () const { return &(*this)(0,0,0,0,0); }

  template<typename DataTypeIn>
  typename std::enable_if<std::rank<DataTypeIn>::value==1,int>::type
  size_impl () const { return std::extent<DataTypeIn,0>::value; }

  template<typename DataTypeIn>
  typename std::enable_if<std::rank<DataTypeIn>::value>=2,int>::type
  size_impl () const { return std::extent<DataTypeIn,0>::value *
                              size_impl<std::remove_extent<DataTypeIn>::type>(); }
};

} // namespace Homme

#endif // HOMME_STATIC_ARRAY_HPP
