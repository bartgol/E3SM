/********************************************************************************
 * HOMMEXX 1.0: Copyright of Sandia Corporation
 * This software is released under the BSD license
 * See the file 'COPYRIGHT' in the HOMMEXX/src/share/cxx directory
 *******************************************************************************/

#ifndef HOMMEXX_ELEMENTS_HPP
#define HOMMEXX_ELEMENTS_HPP

#include "Types.hpp"
#include "Element.hpp"

namespace Homme {

class HybridVCoord;

/* Per element data - specific velocity, temperature, pressure, etc. */
class Elements {
public:

  ExecViewManaged<Element*>             elems_d;
  ExecViewManaged<Element*>::HostMirror elems_h;

  Elements() = default;

  void init(const int num_elems);

  void random_init(int num_elems, Real max_pressure = 1.0);
  void random_init(int num_elems, Real max_pressure, const HybridVCoord& hvcoord);

  KOKKOS_INLINE_FUNCTION
  int num_elems() const { return m_num_elems; }

  // Fill the exec space views with data coming from F90 pointers
  void init_2d(const int ie, CF90Ptr &D, CF90Ptr &Dinv, CF90Ptr &fcor,
               CF90Ptr &spheremp, CF90Ptr &rspheremp,
               CF90Ptr &metdet, CF90Ptr &metinv, 
               CF90Ptr &phis,
               CF90Ptr &tensorvisc,
               CF90Ptr &vec_sph2cart);

  // Copy the element structures only once.
  void init_2d_complete ();
               
  // Fill the exec space views with data coming from F90 pointers
  void pull_from_f90_pointers(CF90Ptr &state_v, CF90Ptr &state_t,
                              CF90Ptr &state_dp3d, CF90Ptr &derived_phi,
                              CF90Ptr &derived_omega_p,
                              CF90Ptr &derived_v, CF90Ptr &derived_eta_dot_dpdn);
  void pull_3d(CF90Ptr &derived_phi,
               CF90Ptr &derived_omega_p, CF90Ptr &derived_v);
  void pull_4d(CF90Ptr &state_v, CF90Ptr &state_t, CF90Ptr &state_dp3d);
  void pull_eta_dot(CF90Ptr &derived_eta_dot_dpdn);

  // Push the results from the exec space views to the F90 pointers
  void push_to_f90_pointers(F90Ptr &state_v, F90Ptr &state_t, F90Ptr &state_dp,
                            F90Ptr &derived_phi,
                            F90Ptr &derived_omega_p, F90Ptr &derived_v,
                            F90Ptr &derived_eta_dot_dpdn) const;
  void push_3d(F90Ptr &derived_phi,
               F90Ptr &derived_omega_p, F90Ptr &derived_v) const;
  void push_4d(F90Ptr &state_v, F90Ptr &state_t, F90Ptr &state_dp3d) const;
  void push_eta_dot(F90Ptr &derived_eta_dot_dpdn) const;

private:
  int m_num_elems;
};

} // Homme

#endif // HOMMEXX_ELEMENTS_HPP
