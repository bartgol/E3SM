/********************************************************************************
 * HOMMEXX 1.0: Copyright of Sandia Corporation
 * This software is released under the BSD license
 * See the file 'COPYRIGHT' in the HOMMEXX/src/share/cxx directory
 *******************************************************************************/

#ifndef HOMMEXX_ELEMENT_HPP
#define HOMMEXX_ELEMENT_HPP

#include "Types.hpp"
#include "ElementGeometry.hpp"

namespace Homme {

class HybridVCoord;

/*
//buffer views are temporaries that matter only during local RK steps
//(dynamics and tracers time step).
//m_ views are also used outside of local timesteps.
struct BufferViews {
  BufferViews() = default;

  void init(Real* const memory);

  ExecViewUnmanaged<Scalar    [NP][NP][NUM_LEV]>      pressure;
  ExecViewUnmanaged<Scalar    [NP][NP][NUM_LEV]>      temperature_virt;
  ExecViewUnmanaged<Scalar    [NP][NP][NUM_LEV]>      omega_p;
  ExecViewUnmanaged<Scalar    [NP][NP][NUM_LEV]>      div_vdp;
  ExecViewUnmanaged<Scalar    [NP][NP][NUM_LEV]>      ephi;
  ExecViewUnmanaged<Scalar    [NP][NP][NUM_LEV]>      vorticity;
  ExecViewUnmanaged<Scalar    [NP][NP][NUM_LEV]>      ttens;
  ExecViewUnmanaged<Scalar    [NP][NP][NUM_LEV]>      dptens;
  ExecViewUnmanaged<Scalar [2][NP][NP][NUM_LEV]>      pressure_grad;
  ExecViewUnmanaged<Scalar [2][NP][NP][NUM_LEV]>      temperature_grad;
  ExecViewUnmanaged<Scalar [2][NP][NP][NUM_LEV]>      vdp;
  ExecViewUnmanaged<Scalar [2][NP][NP][NUM_LEV]>      energy_grad;
  ExecViewUnmanaged<Scalar [2][NP][NP][NUM_LEV]>      vtens;

  // Buffers for EulerStepFunctor
  ExecViewUnmanaged<Scalar [2][NP][NP][NUM_LEV]>  vstar;
  ExecViewUnmanaged<Scalar    [NP][NP][NUM_LEV]>  dpdissk;
  ExecViewUnmanaged<Real [NP][NP]> preq_buf;

  // sdot_sum is used in case rsplit=0 and in energy diagnostics
  // (not yet coded).
  ExecViewUnmanaged<Real [NP][NP]> sdot_sum;

  // Buffers for spherical operators
  ExecViewUnmanaged<Scalar [2][NP][NP][NUM_LEV]> div_buf;
  ExecViewUnmanaged<Scalar [2][NP][NP][NUM_LEV]> grad_buf;
  ExecViewUnmanaged<Scalar [2][NP][NP][NUM_LEV]> curl_buf;
  ExecViewUnmanaged<Scalar [2][NP][NP][NUM_LEV]> sphere_vector_buf;

  ExecViewUnmanaged<Scalar    [NP][NP][NUM_LEV]> divergence_temp;
  ExecViewUnmanaged<Scalar    [NP][NP][NUM_LEV]> vorticity_temp;
  ExecViewUnmanaged<Scalar    [NP][NP][NUM_LEV]> lapl_buf_1;
  ExecViewUnmanaged<Scalar    [NP][NP][NUM_LEV]> lapl_buf_2;
  ExecViewUnmanaged<Scalar    [NP][NP][NUM_LEV]> lapl_buf_3;

  // Buffers for vertical advection terms in V and T for case
  // of Eulerian advection, rsplit=0. These buffers are used in both
  // cases, rsplit>0 and =0. Some of these values need to be init-ed
  // to zero at the beginning of each RK stage. Right now there is a code
  // for this, since Elements is a singleton.
  ExecViewUnmanaged<Scalar [2][NP][NP][NUM_LEV]> v_vadv_buf;
  ExecViewUnmanaged<Scalar [NP][NP][NUM_LEV]>    t_vadv_buf;
  ExecViewUnmanaged<Scalar [NP][NP][NUM_LEV]>    eta_dot_dpdn_buf;

  int size () const {
    return 16*(NP*NP*NUM_LEV*VECTOR_SIZE)   +    // Scalar 3d
           10*(2*NP*NP*NUM_LEV*VECTOR_SIZE) +    // Vector 3d
           2*(NP*NP);                            // Scalar 2d
  }
#ifdef DEBUG_TRACE
  clock_t kernel_start_times;
  clock_t kernel_end_times;
#endif
};
*/

// Per element data
class Element {
public:
  using ScalarState2d = ExecViewUnmanaged<Real   [NUM_TIME_LEVELS]   [NP][NP]>;
  using ScalarState3d = ExecViewUnmanaged<Scalar [NUM_TIME_LEVELS]   [NP][NP][NUM_LEV]>;
  using VectorState3d = ExecViewUnmanaged<Scalar [NUM_TIME_LEVELS][2][NP][NP][NUM_LEV]>;

  using Scalar2d = ExecViewUnmanaged<Real         [NP][NP]>;
  using Tensor2d = ExecViewUnmanaged<Real   [2][2][NP][NP]>;
  using Scalar3d = ExecViewUnmanaged<Scalar       [NP][NP][NUM_LEV]>;
  using Vector3d = ExecViewUnmanaged<Scalar    [2][NP][NP][NUM_LEV]>;

  ElementGeometry       m_geo;

  // Some 2d views
  Scalar2d          m_phis;                       // surface geopotential (prescribed)
  Tensor2d          m_tensorvisc;                 // matrix for tensor viscosity

  // The states
  ScalarState3d     m_t;                          // temperature
  ScalarState3d     m_dp3d;                       // delta p on levels
  VectorState3d     m_v;                          // velocity
  ScalarState2d     m_ps_v;                       // surface pressure

  // Diagnostics for explicit timesteps
  Scalar3d          m_omega_p;                    // vertical tendency (derived)
  Scalar3d          m_eta_dot_dpdn;               // mean vertical flux from dynamics

  // Storage for subcycling tracers/dynamics
  Vector3d          m_derived_vn0;                // velocity for SE tracers advection
  Scalar3d          m_derived_dpdiss_biharmonic;  // mean dp dissipation tendency, if nu_p>0
  Scalar3d          m_derived_dpdiss_ave;         // mean dp used to compute psdiss_tens

  // Tracer advection fields used for consistency and limiters
  Scalar3d          m_derived_dp;                 // for dp_tracers at physics timestep
  Scalar3d          m_derived_divdp;              // divergence of dp
  Scalar3d          m_derived_divdp_proj;         // DSSed divdp

  // Forcing terms
  Vector3d          m_fm;                         // Momentum (? units are wrong in apply_cam_forcing...) forcing
  Scalar3d          m_ft;                         // Temperature forcing

  Element() = default;

  void view_memory (Real* const memory);
  void alloc_memory ();

  void random_init(Real max_pressure = 1.0);
  void random_init(Real max_pressure, const HybridVCoord& hvcoord);

  int size () const {
     return 4*NUM_TIME_LEVELS*NP*NP*NUM_LEV*VECTOR_SIZE +   // States
            (1+2*2+NUM_TIME_LEVELS)*(NP*NP)             +   // 2d views (1 scalar, 1 2x2 tensor, 1 state)
            9*(NP*NP*NUM_LEV*VECTOR_SIZE)               +   // 3d scalars
            2*(2*NP*NP*NUM_LEV*VECTOR_SIZE)             +   // 3d vecrors
            m_geo.size();
  }

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

  // This is the actual storage of the views, in case
  // each element allocates its own memory
  ExecViewManaged<Real*>    m_storage;

  bool                      m_consthv;
};

} // Homme

#endif // HOMMEXX_ELEMENT_HPP
