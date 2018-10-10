/********************************************************************************
 * HOMMEXX 1.0: Copyright of Sandia Corporation
 * This software is released under the BSD license
 * See the file 'COPYRIGHT' in the HOMMEXX/src/share/cxx directory
 *******************************************************************************/

#include "Elements.hpp"
#include "utilities/SubviewUtils.hpp"
#include "utilities/SyncUtils.hpp"
#include "utilities/TestUtils.hpp"
#include "HybridVCoord.hpp"

#include <limits>
#include <random>
#include <assert.h>

namespace Homme {

void Elements::init(const int num_elems) {
  m_num_elems = num_elems;

  elems_d = ExecViewManaged<Element*>("",m_num_elems);
  elems_h = Kokkos::create_mirror_view(elems_d);
}

void Elements::init_2d(const int ie, CF90Ptr &D, CF90Ptr &Dinv, CF90Ptr &fcor,
                       CF90Ptr &spheremp, CF90Ptr &rspheremp,
                       CF90Ptr &metdet, CF90Ptr &metinv, CF90Ptr &phis,
                       CF90Ptr &tensorvisc, CF90Ptr &vec_sph2cart) {
  
  ElementGeometry& geo = elems_h(ie).m_geo;
  geo.fill(D,Dinv,fcor,spheremp,rspheremp,metdet,metinv,vec_sph2cart);

  using ScalarViewF90   = HostViewUnmanaged<const Real [NP][NP]>;
  using TensorViewF90   = HostViewUnmanaged<const Real [2][2][NP][NP]>;

  std::copy_n(phis,NP*NP,elems_h(ie).m_phis.data());
  std::copy_n(tensorvisc,NP*NP*2*2,elems_h(ie).m_tensorvisc.data());
}

//test for tensor hv is needed
void Elements::random_init(int num_elems, Real max_pressure) {
  std::random_device rd;
  HybridVCoord hv;
  hv.random_init(rd());
  random_init(num_elems,max_pressure,hv);
}

void Elements::random_init(int num_elems, Real max_pressure, const HybridVCoord& hvcoord) {
  init(num_elems);

  for (int ie=0; ie<num_elems; ++ie) {
    elems_h(ie).random_init(max_pressure,hvcoord);
  }
  Kokkos::deep_copy(elems_d,elems_h);
}

void Elements::pull_from_f90_pointers(
    CF90Ptr &state_v, CF90Ptr &state_t, CF90Ptr &state_dp3d,
    CF90Ptr &derived_phi, CF90Ptr &derived_omega_p,
    CF90Ptr &derived_v, CF90Ptr &derived_eta_dot_dpdn) {
  pull_3d(derived_phi, derived_omega_p, derived_v);
  pull_4d(state_v, state_t, state_dp3d);
  pull_eta_dot(derived_eta_dot_dpdn);
}

void Elements::pull_3d(CF90Ptr &derived_phi, CF90Ptr &derived_omega_p, CF90Ptr &derived_v) {
  HostViewUnmanaged<const Real *[NUM_PHYSICAL_LEV]   [NP][NP]> derived_phi_f90(derived_phi,m_num_elems);
  HostViewUnmanaged<const Real *[NUM_PHYSICAL_LEV]   [NP][NP]> derived_omega_p_f90(derived_omega_p,m_num_elems);
  HostViewUnmanaged<const Real *[NUM_PHYSICAL_LEV][2][NP][NP]> derived_v_f90(derived_v,m_num_elems);

  sync_to_device(derived_phi_f90,     m_phi);
  sync_to_device(derived_omega_p_f90, m_omega_p);
  sync_to_device(derived_v_f90,       m_derived_vn0);
}

void Elements::pull_4d(CF90Ptr &state_v, CF90Ptr &state_t, CF90Ptr &state_dp3d) {
  HostViewUnmanaged<const Real *[NUM_TIME_LEVELS][NUM_PHYSICAL_LEV]   [NP][NP]> state_t_f90    (state_t,m_num_elems);
  HostViewUnmanaged<const Real *[NUM_TIME_LEVELS][NUM_PHYSICAL_LEV]   [NP][NP]> state_dp3d_f90 (state_dp3d,m_num_elems);
  HostViewUnmanaged<const Real *[NUM_TIME_LEVELS][NUM_PHYSICAL_LEV][2][NP][NP]> state_v_f90    (state_v,m_num_elems);

  sync_to_device(state_t_f90,    m_t);
  sync_to_device(state_dp3d_f90, m_dp3d);
  sync_to_device(state_v_f90,    m_v);
}

void Elements::pull_eta_dot(CF90Ptr &derived_eta_dot_dpdn) {
  HostViewUnmanaged<const Real *[NUM_INTERFACE_LEV][NP][NP]> eta_dot_dpdn_f90(derived_eta_dot_dpdn,m_num_elems);
  sync_to_device_i2p(eta_dot_dpdn_f90,m_eta_dot_dpdn);
}

void Elements::push_to_f90_pointers(F90Ptr &state_v, F90Ptr &state_t,
                                    F90Ptr &state_dp3d, F90Ptr &derived_phi,
                                    F90Ptr &derived_omega_p, F90Ptr &derived_v,
                                    F90Ptr &derived_eta_dot_dpdn) const {
  push_3d(derived_phi, derived_omega_p, derived_v);
  push_4d(state_v, state_t, state_dp3d);
  push_eta_dot(derived_eta_dot_dpdn);
}

void Elements::push_3d(F90Ptr &derived_phi, F90Ptr &derived_omega_p, F90Ptr &derived_v) const {
  HostViewUnmanaged<Real *[NUM_PHYSICAL_LEV]   [NP][NP]> derived_phi_f90(derived_phi,m_num_elems);
  HostViewUnmanaged<Real *[NUM_PHYSICAL_LEV]   [NP][NP]> derived_omega_p_f90(derived_omega_p,m_num_elems);
  HostViewUnmanaged<Real *[NUM_PHYSICAL_LEV][2][NP][NP]> derived_v_f90(derived_v,m_num_elems);

  sync_to_host(m_phi,         derived_phi_f90);
  sync_to_host(m_omega_p,     derived_omega_p_f90);
  sync_to_host(m_derived_vn0, derived_v_f90);
}

void Elements::push_4d(F90Ptr &state_v, F90Ptr &state_t, F90Ptr &state_dp3d) const {
  HostViewUnmanaged<Real *[NUM_TIME_LEVELS][NUM_PHYSICAL_LEV]   [NP][NP]> state_t_f90    (state_t,m_num_elems);
  HostViewUnmanaged<Real *[NUM_TIME_LEVELS][NUM_PHYSICAL_LEV]   [NP][NP]> state_dp3d_f90 (state_dp3d,m_num_elems);
  HostViewUnmanaged<Real *[NUM_TIME_LEVELS][NUM_PHYSICAL_LEV][2][NP][NP]> state_v_f90    (state_v,m_num_elems);

  sync_to_host(m_t,    state_t_f90);
  sync_to_host(m_dp3d, state_dp3d_f90);
  sync_to_host(m_v,    state_v_f90);
}

void Elements::push_eta_dot(F90Ptr &derived_eta_dot_dpdn) const {
  HostViewUnmanaged<Real *[NUM_INTERFACE_LEV][NP][NP]> eta_dot_dpdn_f90(derived_eta_dot_dpdn,m_num_elems);
  sync_to_host_p2i(m_eta_dot_dpdn,eta_dot_dpdn_f90);
}

void Elements::d(Real *d_ptr, int ie) const {
  ExecViewUnmanaged<Real[2][2][NP][NP]> d_device = Homme::subview(m_d, ie);
  decltype(d_device)::HostMirror d_host = Kokkos::create_mirror_view(d_device);
  HostViewUnmanaged<Real[2][2][NP][NP]> d_wrapper(d_ptr);
  Kokkos::deep_copy(d_host, d_device);
  Kokkos::deep_copy(d_wrapper,d_host);
}

void Elements::dinv(Real *dinv_ptr, int ie) const {
  ExecViewUnmanaged<Real[2][2][NP][NP]> dinv_device = Homme::subview(m_dinv,ie);
  decltype(dinv_device)::HostMirror dinv_host = Kokkos::create_mirror_view(dinv_device);
  HostViewUnmanaged<Real[2][2][NP][NP]> dinv_wrapper(dinv_ptr);
  Kokkos::deep_copy(dinv_host, dinv_device);
  Kokkos::deep_copy(dinv_wrapper,dinv_host);
}

void Elements::BufferViews::init(const int num_elems) {
  pressure =
      ExecViewManaged<Scalar * [NP][NP][NUM_LEV]>("Pressure buffer", num_elems);
  pressure_grad = ExecViewManaged<Scalar * [2][NP][NP][NUM_LEV]>(
      "Gradient of pressure", num_elems);
  temperature_virt = ExecViewManaged<Scalar * [NP][NP][NUM_LEV]>(
      "Virtual Temperature", num_elems);
  temperature_grad = ExecViewManaged<Scalar * [2][NP][NP][NUM_LEV]>(
      "Gradient of temperature", num_elems);
  omega_p = ExecViewManaged<Scalar * [NP][NP][NUM_LEV]>(
      "Omega_P = omega/pressure = (Dp/Dt)/pressure", num_elems);
  vdp = ExecViewManaged<Scalar * [2][NP][NP][NUM_LEV]>("(u,v)*dp", num_elems);
  div_vdp = ExecViewManaged<Scalar * [NP][NP][NUM_LEV]>(
      "Divergence of dp3d * (u,v)", num_elems);
  ephi = ExecViewManaged<Scalar * [NP][NP][NUM_LEV]>(
      "Kinetic Energy + Geopotential Energy", num_elems);
  energy_grad = ExecViewManaged<Scalar * [2][NP][NP][NUM_LEV]>(
      "Gradient of ephi", num_elems);
  vorticity =
      ExecViewManaged<Scalar * [NP][NP][NUM_LEV]>("Vorticity", num_elems);

  ttens  = ExecViewManaged<Scalar*    [NP][NP][NUM_LEV]>("Temporary for temperature",num_elems);
  dptens = ExecViewManaged<Scalar*    [NP][NP][NUM_LEV]>("Temporary for dp3d",num_elems);
  vtens  = ExecViewManaged<Scalar* [2][NP][NP][NUM_LEV]>("Temporary for velocity",num_elems);

  vstar = ExecViewManaged<Scalar * [2][NP][NP][NUM_LEV]>("buffer for (flux v)/dp",
       num_elems);
  dpdissk = ExecViewManaged<Scalar * [NP][NP][NUM_LEV]>(
      "dpdissk", num_elems);

  preq_buf = ExecViewManaged<Real * [NP][NP]>("Preq Buffer", num_elems);

  sdot_sum = ExecViewManaged<Real * [NP][NP]>("Sdot sum buffer", num_elems);

  div_buf = ExecViewManaged<Scalar * [2][NP][NP][NUM_LEV]>("Divergence Buffer",
                                                           num_elems);
  grad_buf = ExecViewManaged<Scalar * [2][NP][NP][NUM_LEV]>("Gradient Buffer",
                                                            num_elems);
  curl_buf = ExecViewManaged<Scalar * [2][NP][NP][NUM_LEV]>("Vorticity Buffer",
                                                            num_elems);

  sphere_vector_buf = ExecViewManaged<Scalar * [2][NP][NP][NUM_LEV]>("laplacian vector Buffer", num_elems);

  divergence_temp = ExecViewManaged<Scalar * [NP][NP][NUM_LEV]>("Divergence temporary",
                                                            num_elems);
  vorticity_temp = ExecViewManaged<Scalar * [NP][NP][NUM_LEV]>("Vorticity temporary",
                                                            num_elems);
  lapl_buf_1 = ExecViewManaged<Scalar * [NP][NP][NUM_LEV]>("Scalar laplacian Buffer", num_elems);
  lapl_buf_2 = ExecViewManaged<Scalar * [NP][NP][NUM_LEV]>("Scalar laplacian Buffer", num_elems);
  lapl_buf_3 = ExecViewManaged<Scalar * [NP][NP][NUM_LEV]>("Scalar laplacian Buffer", num_elems);
  v_vadv_buf = ExecViewManaged<Scalar * [2][NP][NP][NUM_LEV]>("v_vadv buffer",
                                                              num_elems);
  t_vadv_buf = ExecViewManaged<Scalar * [NP][NP][NUM_LEV]>("t_vadv buffer",
                                                           num_elems);
  eta_dot_dpdn_buf = ExecViewManaged<Scalar * [NP][NP][NUM_LEV_P]>("eta_dot_dpdpn buffer",
                                                                   num_elems);

  kernel_start_times = ExecViewManaged<clock_t *>("Start Times", num_elems);
  kernel_end_times = ExecViewManaged<clock_t *>("End Times", num_elems);
}

} // namespace Homme
