#ifndef HOMMEXX_RANGED_VIEW_HPP
#define HOMMEXX_RANGED_VIEW_HPP

#include "Types.hpp"

namespace Homme
{

template<typename ValueTypeIn, typename ValueTypeOut, int SizeIn>
struct ValueTypesProps {
  static constexpr bool is_in_pack  = std::is_same<ValueTypeIn,Scalar>::value;
  static constexpr bool is_out_pack = std::is_same<ValueTypeOut,Scalar>::value;

  static_assert(is_out_pack || std::is_same<ValueTypeOut,Real>::value,
                "Error! Only 'Real' and 'Scalar' acceppted for 'ReinterpretValueType'.\n");
  static_assert(is_in_pack || std::is_same<ValueTypeIn,Real>::value,
                "Error! Only 'Real' and 'Scalar' acceppted for 'ReinterpretValueType'.\n");
  static constexpr int size_out = is_in_pack ? (is_out_pack ? SizeIn : SizeIn * VECTOR_SIZE)
                                             : (is_out_pack ? (SizeIn+VECTOR_SIZE-1)/VECTOR_SIZE : SizeIn);
};

template<typename DataTypeIn, typename ValueTypeOut, bool is_last_dim = true>
struct ReinterpretDataType {
private:
  using view_helper = ViewType<DataTypeIn>;
public:
  using value_type_in = typename view_helper::traits::non_const_data_type;

  static_assert (!is_last_dim, "Error! View's data type must be at least 1-d.\n");
  using type = ValueTypeOut;
};

template<typename DataTypeIn, typename ValueTypeOut, size_t N, bool is_last_dim>
struct ReinterpretDataType<DataTypeIn[N],ValueTypeOut,is_last_dim> {
private:
  using view_helper = ViewType<DataTypeIn>;
  using value_type_in = typename view_helper::traits::non_const_data_type;
  using helper = ValueTypesProps<value_type_in,ValueTypeOut,N>;
public:

  static constexpr int extent = is_last_dim ? helper::size_out : N;

  using type = typename ReinterpretDataType<DataTypeIn,ValueTypeOut,false>::type[extent];
};

template<typename DataTypeIn, typename ValueTypeOut, bool is_last_dim>
struct ReinterpretDataType<DataTypeIn*,ValueTypeOut,is_last_dim> {
private:
  using view_helper = ViewType<DataTypeIn>;
  using value_type_in = typename view_helper::traits::non_const_data_type;
public:

  using type = typename ReinterpretDataType<DataTypeIn,ValueTypeOut,false>::type*;
};

template<typename ViewIn, typename ValueTypeOut>
struct ReinterpretViewHelper
{
private:
  using data_type_in  = typename ViewIn::traits::data_type;
  using mem_space_in  = typename ViewIn::traits::memory_space;
  using mem_traits_in = typename ViewIn::traits::memory_traits;
  using data_type_out = typename ReinterpretDataType<data_type_in,ValueTypeOut,true>::type;
public:
  using ViewOut = ViewType<data_type_out,mem_space_in,mem_traits_in>;
};

template<typename ValueTypeOut, typename ViewIn>
typename ReinterpretViewHelper<ViewIn,ValueTypeOut>::ViewOut
reinterpretView (ViewIn view_in) {
  using ViewOut = typename ReinterpretViewHelper<ViewIn,ValueTypeOut>::ViewOut;
  static_assert (ViewIn::traits::rank_dynamic<2, "Error! Function not implemented if rank_dynamic>=2.\n");
  static_assert (std::is_same<ValueTypeOut,typename ViewOut::traits::non_const_value_type>::value,
                 "Error! Something is wrong with the 'ReinterpretViewHelper' meta-class logic.\n");

typename MyDebug<typename ViewOut::traits::non_const_value_type>::ciao a;
  ViewOut view_out;
  switch (ViewIn::traits::rank_dynamic) {
    case 0:
      view_out = ViewOut(view_in.data());
      break;
    case 1:
      view_out = ViewOut(view_in.data(),view_in.extent(0));
      break;
  }
  return view_out;
}

// Helper struct to pack indices in a single template parameter (to avoid multiple parameter packs in RangedView)
template<int... Ints>
using FirstIndexStruct = Kokkos::Impl::integer_sequence<int,Ints...>;

// Note: I do not have time to think about a clever and clean n-d implementation.
template<typename DataType, class FirstIndices, typename... Props>
class RangedView;

template<typename DataType, int I0, typename... Props>
class RangedView<DataType,FirstIndexStruct<I0>,Props...>
{
public:

  using base_type = ViewType<DataType,Props...>;
  using reference_type = typename base_type::reference_type;

  enum { Rank = base_type::Rank };
  static_assert (Rank==1, "Error! Rank incompatible with number of offsets.\n");

  template<typename... Args>
  explicit RangedView (Args... args)
   : m_view(args...) {}

  explicit RangedView (base_type view)
   : m_view(view) {}

  template<typename DataTypeIn>
  RangedView (const RangedView<DataTypeIn,FirstIndexStruct<I0>,Props...>& src)
   : m_view(src.view()) {}

  // Operators
  // Note: subtract the first index I0, rather than adding it. E.g., consider an array
  //       with indices from -2 to 3. It has length 6, and must be declared as
  //       RangedView<Real[6],-2>. The element with index 0 is the 3rd in memory,
  //       so we need to do 0 - (-2) = 2.

  reference_type
  operator()( const int& i) const 
  {   
    return m_view(i - I0);
  }

  base_type view () const { return m_view; }

protected:

  base_type               m_view;
};

template<typename DataType, int I0, int J0, typename... Props>
class RangedView<DataType,FirstIndexStruct<I0,J0>,Props...>
{
public:

  using base_type = ViewType<DataType,Props...>;
  using non_const_data_type = typename base_type::traits::non_const_data_type;
  using const_data_type = typename base_type::traits::const_data_type;
  using reference_type = typename base_type::reference_type;

  enum { Rank = base_type::Rank };
  static_assert (Rank==2, "Error! Rank incompatible with number of offsets.\n");

  template<typename... Args>
  explicit RangedView (Args... args)
   : m_view(args...) {}

  explicit RangedView (base_type view)
   : m_view(view) {}

  template<typename DataTypeIn>
  RangedView (const RangedView<DataTypeIn,FirstIndexStruct<I0,J0>,Props...>& src)
   : m_view(src.view()) {}

  KOKKOS_FORCEINLINE_FUNCTION
  reference_type
  operator()( const int& i, 
              const int& j) const
  {
    return m_view(i - I0, j - J0);
  }

  base_type view () const { return m_view; }

protected:

  base_type               m_view;
};

// More verbose names (and faster to type their template args)
template<typename DataType, int I0, typename... Props>
using RangedView1D = RangedView<DataType,FirstIndexStruct<I0>,Props...>;
template<typename DataType, int I0, int J0, typename... Props>
using RangedView2D = RangedView<DataType,FirstIndexStruct<I0,J0>,Props...>;

// Get the type of a ranged view from a given view
template<typename View, int... FirstIndices>
using Ranged = RangedView<typename View::traits::data_type,
                          FirstIndexStruct<FirstIndices...>,
                          typename View::traits::memory_space,
                          typename View::traits::memory_traits>;

template<int... FirstIndices, typename View>
Ranged<View,FirstIndices...>
viewAsRanged (View view_in) {
  return Ranged<View,FirstIndices...>(view_in);
}

} // namespace Homme

#endif // HOMMEXX_RANGED_VIEW_HPP
