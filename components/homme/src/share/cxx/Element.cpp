/********************************************************************************
 * HOMMEXX 1.0: Copyright of Sandia Corporation
 * This software is released under the BSD license
 * See the file 'COPYRIGHT' in the HOMMEXX/src/share/cxx directory
 *******************************************************************************/

#include "Element.hpp"
#include "utilities/SubviewUtils.hpp"
#include "utilities/SyncUtils.hpp"
#include "utilities/TestUtils.hpp"
#include "HybridVCoord.hpp"

#include <limits>
#include <random>
#include <assert.h>

namespace Homme {

void Element::view_memory (Real* const memory) {
  int offset = 0;

  // Init the geometry views
  m_geo.view_memory(m_consthv,memory);
  offset += m_geo.size();

  Scalar* memory3d = reinterpret_cast<Scalar*>(memory);

  // Init the states
  m_v = ExecViewUnmanaged<Scalar [2][NP][NP][NUM_LEV]>(memory3d+offset);    offset += 2*NP*NP*NUM_LEV*VECTOR_SIZE;
}

void Element::alloc_memory () {
  m_storage = ExecViewManaged<Real*>("",size());

  view_memory(m_storage.data());
}

void Element::random_init(Real max_pressure) {
  std::random_device rd;
  HybridVCoord hv;
  hv.random_init(rd());
  random_init(max_pressure,hv);
}

void Element::random_init(Real max_pressure, const HybridVCoord& hvcoord) {
  // For random tests, we allocate tensorhv too
  m_consthv = true;
  alloc_memory();
  m_geo.random_init(true,false);

  // arbitrary minimum value to generate and minimum determinant allowed
  constexpr const Real min_value = 0.015625;
  // 1 is for const hv
  std::random_device rd;
  std::mt19937_64 engine(rd());
  std::uniform_real_distribution<Real> random_dist(min_value, 1.0 / min_value);

  genRandStaticArray(m_phis, engine, random_dist);

  genRandStaticArray(m_omega_p, engine, random_dist);
  genRandStaticArray(m_phi, engine, random_dist);
  genRandStaticArray(m_derived_vn0, engine, random_dist);

  genRandStaticArray(m_v, engine, random_dist);
  genRandStaticArray(m_t, engine, random_dist);

  // Generate ps_v so that it is >> ps0.
  // Note: make sure you init hvcoord before calling this method!
  genRandStaticArray(m_ps_v, engine, std::uniform_real_distribution<Real>(100*hvcoord.ps0,1000*hvcoord.ps0));

  // This ensures the pressure in a single column is monotonically increasing
  // and has fixed upper and lower values
  const auto make_pressure_partition = [=](
      Scalar* pt_pressure, int length) {
    assert (length==NUM_LEV);
    // Put in monotonic order
    std::sort(
        reinterpret_cast<Real *>(pt_pressure),
        reinterpret_cast<Real *>(pt_pressure + length));
    // Ensure none of the values are repeated
    for (int level = NUM_PHYSICAL_LEV - 1; level > 0; --level) {
      const int prev_ilev = (level - 1) / VECTOR_SIZE;
      const int prev_vlev = (level - 1) % VECTOR_SIZE;
      const int cur_ilev = level / VECTOR_SIZE;
      const int cur_vlev = level % VECTOR_SIZE;
      // Need to try again if these are the same or if the thickness is too
      // small
      if (pt_pressure[cur_ilev][cur_vlev] <=
          pt_pressure[prev_ilev][prev_vlev] +
              min_value * std::numeric_limits<Real>::epsilon()) {
        return false;
      }
    }
    // We know the minimum thickness of a layer is min_value * epsilon
    // (due to floating point), so set the bottom layer thickness to that,
    // and subtract that from the top layer
    // This ensures that the total sum is max_pressure
    pt_pressure[0][0] = min_value * std::numeric_limits<Real>::epsilon();
    const int top_ilev = (NUM_PHYSICAL_LEV - 1) / VECTOR_SIZE;
    const int top_vlev = (NUM_PHYSICAL_LEV - 1) % VECTOR_SIZE;
    // Note that this may not actually change the top level pressure
    // This is okay, because we only need to approximately sum to max_pressure
    pt_pressure[top_ilev][top_vlev] = max_pressure - pt_pressure[0][0];
    for (int e_vlev = top_vlev + 1; e_vlev < VECTOR_SIZE; ++e_vlev) {
      pt_pressure[top_ilev][e_vlev] = std::numeric_limits<Real>::quiet_NaN();
    }
    // Now compute the interval thicknesses
    for (int level = NUM_PHYSICAL_LEV - 1; level > 0; --level) {
      const int prev_ilev = (level - 1) / VECTOR_SIZE;
      const int prev_vlev = (level - 1) % VECTOR_SIZE;
      const int cur_ilev = level / VECTOR_SIZE;
      const int cur_vlev = level % VECTOR_SIZE;
      pt_pressure[cur_ilev][cur_vlev] -= pt_pressure[prev_ilev][prev_vlev];
    }
    return true;
  };

  std::uniform_real_distribution<Real> pressure_pdf(min_value, max_pressure);

  Real dp3d_min = std::numeric_limits<Real>::max();
  using column = Scalar[NUM_LEV];
  for (int igp = 0; igp < NP; ++igp) {
    for (int jgp = 0; jgp < NP; ++jgp) {
      for (int tl = 0; tl < NUM_TIME_LEVELS; ++tl) {
        column& pt_dp3d = m_dp3d.array()[tl][igp][jgp]; 
        genRandArray(pt_dp3d, engine, pressure_pdf, make_pressure_partition);
        auto h_dp3d = Kokkos::create_mirror_view(pt_dp3d);
        Kokkos::deep_copy(h_dp3d,pt_dp3d);
        for (int ilev=0; ilev<NUM_LEV; ++ilev) {
          for (int iv=0; iv<VECTOR_SIZE; ++iv) {
            dp3d_min = std::min(dp3d_min,h_dp3d(ilev)[iv]);
          }
        }
      }
    }
  }

  // Generate eta_dot_dpdn so that it is << dp3d
  genRandArray(m_eta_dot_dpdn, engine, std::uniform_real_distribution<Real>(0.01*dp3d_min,0.1*dp3d_min));

  Kokkos::deep_copy(m_d, h_d);
  Kokkos::deep_copy(m_dinv, h_dinv);
  return;
}

void Element::pull_from_f90_pointers(
    CF90Ptr &state_v, CF90Ptr &state_t, CF90Ptr &state_dp3d,
    CF90Ptr &derived_phi, CF90Ptr &derived_omega_p,
    CF90Ptr &derived_v, CF90Ptr &derived_eta_dot_dpdn) {
  pull_3d(derived_phi, derived_omega_p, derived_v);
  pull_4d(state_v, state_t, state_dp3d);
  pull_eta_dot(derived_eta_dot_dpdn);
}

void Element::pull_3d(CF90Ptr &derived_phi, CF90Ptr &derived_omega_p, CF90Ptr &derived_v) {
  HostViewUnmanaged<const Real *[NUM_PHYSICAL_LEV]   [NP][NP]> derived_phi_f90(derived_phi,m_num_elems);
  HostViewUnmanaged<const Real *[NUM_PHYSICAL_LEV]   [NP][NP]> derived_omega_p_f90(derived_omega_p,m_num_elems);
  HostViewUnmanaged<const Real *[NUM_PHYSICAL_LEV][2][NP][NP]> derived_v_f90(derived_v,m_num_elems);

  sync_to_device(derived_phi_f90,     m_phi);
  sync_to_device(derived_omega_p_f90, m_omega_p);
  sync_to_device(derived_v_f90,       m_derived_vn0);
}

void Element::pull_4d(CF90Ptr &state_v, CF90Ptr &state_t, CF90Ptr &state_dp3d) {
  HostViewUnmanaged<const Real *[NUM_TIME_LEVELS][NUM_PHYSICAL_LEV]   [NP][NP]> state_t_f90    (state_t,m_num_elems);
  HostViewUnmanaged<const Real *[NUM_TIME_LEVELS][NUM_PHYSICAL_LEV]   [NP][NP]> state_dp3d_f90 (state_dp3d,m_num_elems);
  HostViewUnmanaged<const Real *[NUM_TIME_LEVELS][NUM_PHYSICAL_LEV][2][NP][NP]> state_v_f90    (state_v,m_num_elems);

  sync_to_device(state_t_f90,    m_t);
  sync_to_device(state_dp3d_f90, m_dp3d);
  sync_to_device(state_v_f90,    m_v);
}

void Element::pull_eta_dot(CF90Ptr &derived_eta_dot_dpdn) {
  HostViewUnmanaged<const Real *[NUM_INTERFACE_LEV][NP][NP]> eta_dot_dpdn_f90(derived_eta_dot_dpdn,m_num_elems);
  sync_to_device_i2p(eta_dot_dpdn_f90,m_eta_dot_dpdn);
}

void Element::push_to_f90_pointers(F90Ptr &state_v, F90Ptr &state_t,
                                    F90Ptr &state_dp3d, F90Ptr &derived_phi,
                                    F90Ptr &derived_omega_p, F90Ptr &derived_v,
                                    F90Ptr &derived_eta_dot_dpdn) const {
  push_3d(derived_phi, derived_omega_p, derived_v);
  push_4d(state_v, state_t, state_dp3d);
  push_eta_dot(derived_eta_dot_dpdn);
}

void Element::push_3d(F90Ptr &derived_phi, F90Ptr &derived_omega_p, F90Ptr &derived_v) const {
  HostViewUnmanaged<Real *[NUM_PHYSICAL_LEV]   [NP][NP]> derived_phi_f90(derived_phi,m_num_elems);
  HostViewUnmanaged<Real *[NUM_PHYSICAL_LEV]   [NP][NP]> derived_omega_p_f90(derived_omega_p,m_num_elems);
  HostViewUnmanaged<Real *[NUM_PHYSICAL_LEV][2][NP][NP]> derived_v_f90(derived_v,m_num_elems);

  sync_to_host(m_phi,         derived_phi_f90);
  sync_to_host(m_omega_p,     derived_omega_p_f90);
  sync_to_host(m_derived_vn0, derived_v_f90);
}

void Element::push_4d(F90Ptr &state_v, F90Ptr &state_t, F90Ptr &state_dp3d) const {
  HostViewUnmanaged<Real *[NUM_TIME_LEVELS][NUM_PHYSICAL_LEV]   [NP][NP]> state_t_f90    (state_t,m_num_elems);
  HostViewUnmanaged<Real *[NUM_TIME_LEVELS][NUM_PHYSICAL_LEV]   [NP][NP]> state_dp3d_f90 (state_dp3d,m_num_elems);
  HostViewUnmanaged<Real *[NUM_TIME_LEVELS][NUM_PHYSICAL_LEV][2][NP][NP]> state_v_f90    (state_v,m_num_elems);

  sync_to_host(m_t,    state_t_f90);
  sync_to_host(m_dp3d, state_dp3d_f90);
  sync_to_host(m_v,    state_v_f90);
}

void Element::push_eta_dot(F90Ptr &derived_eta_dot_dpdn) const {
  HostViewUnmanaged<Real *[NUM_INTERFACE_LEV][NP][NP]> eta_dot_dpdn_f90(derived_eta_dot_dpdn,m_num_elems);
  sync_to_host_p2i(m_eta_dot_dpdn,eta_dot_dpdn_f90);
}

void Element::d(Real *d_ptr, int ie) const {
  ExecViewUnmanaged<Real[2][2][NP][NP]> d_device = Homme::subview(m_d, ie);
  decltype(d_device)::HostMirror d_host = Kokkos::create_mirror_view(d_device);
  HostViewUnmanaged<Real[2][2][NP][NP]> d_wrapper(d_ptr);
  Kokkos::deep_copy(d_host, d_device);
  Kokkos::deep_copy(d_wrapper,d_host);
}

void Element::dinv(Real *dinv_ptr, int ie) const {
  ExecViewUnmanaged<Real[2][2][NP][NP]> dinv_device = Homme::subview(m_dinv,ie);
  decltype(dinv_device)::HostMirror dinv_host = Kokkos::create_mirror_view(dinv_device);
  HostViewUnmanaged<Real[2][2][NP][NP]> dinv_wrapper(dinv_ptr);
  Kokkos::deep_copy(dinv_host, dinv_device);
  Kokkos::deep_copy(dinv_wrapper,dinv_host);
}

} // namespace Homme
