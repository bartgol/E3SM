#ifndef HOMME_GENERIC_UTILS_HPP
#define HOMME_GENERIC_UTILS_HPP

#include <Kokkos_Core.hpp>
#include <string>

namespace Homme {

std::string upper_case (const std::string& s);

template<typename V>
typename V::HostMirror
create_mirror_view_and_copy (V v) {
  auto vh = Kokkos::create_mirror_view(v);
  Kokkos::deep_copy(vh,v);
  return vh;
}

} // namespace Homme

#endif // HOMME_GENERIC_UTILS_HPP
