/********************************************************************************
 * HOMMEXX 1.0: Copyright of Sandia Corporation
 * This software is released under the BSD license
 * See the file 'COPYRIGHT' in the HOMMEXX/src/share/cxx directory
 *******************************************************************************/

#ifndef HOMMEXX_CAAR_FUNCTOR_IMPL_HPP
#define HOMMEXX_CAAR_FUNCTOR_IMPL_HPP

#include "Types.hpp"
#include "Elements.hpp"
#include "Tracers.hpp"
#include "HybridVCoord.hpp"
#include "ReferenceElement.hpp"
#include "KernelVariables.hpp"
#include "SphereOperators.hpp"

#include "mpi/BoundaryExchange.hpp"
#include "utilities/SubviewUtils.hpp"

#include "profiling.hpp"
#include "ErrorDefs.hpp"

#include <assert.h>
#include <type_traits>


namespace Homme {

//buffer views are temporaries that matter only during local RK steps
struct CaarBuffers {
  CaarBuffers() = default;

  void init(Real* const memory) {
    using Scalar2d = ExecViewUnmanaged<Real      [NP][NP]>;
    using Scalar3d = ExecViewUnmanaged<Scalar    [NP][NP][NUM_LEV]>;
    using Vector3d = ExecViewUnmanaged<Scalar [2][NP][NP][NUM_LEV]>;

    int offset = 0;

    // Set the 2d view first, so we can then switch value type
    sdot_sum = Scalar2d(memory);  offset += NP*NP;   

    // Note: this works cause sizeof(Scalar)/sizeof(Real) divides NP*NP
    Scalar* memory3d = reinterpret_cast<Scalar*>(memory);

    pressure          = Scalar3d(memory3d + offset);    offset +=   NP*NP*NUM_LEV*VECTOR_SIZE;
    pressure_grad     = Vector3d(memory3d + offset);    offset += 2*NP*NP*NUM_LEV*VECTOR_SIZE;
    temperature_virt  = Scalar3d(memory3d + offset);    offset +=   NP*NP*NUM_LEV*VECTOR_SIZE;
    temperature_grad  = Vector3d(memory3d + offset);    offset += 2*NP*NP*NUM_LEV*VECTOR_SIZE;
    omega_p           = Scalar3d(memory3d + offset);    offset +=   NP*NP*NUM_LEV*VECTOR_SIZE;
    vdp               = Vector3d(memory3d + offset);    offset += 2*NP*NP*NUM_LEV*VECTOR_SIZE;
    div_vdp           = Scalar3d(memory3d + offset);    offset +=   NP*NP*NUM_LEV*VECTOR_SIZE;
    ephi              = Scalar3d(memory3d + offset);    offset +=   NP*NP*NUM_LEV*VECTOR_SIZE;
    energy_grad       = Vector3d(memory3d + offset);    offset += 2*NP*NP*NUM_LEV*VECTOR_SIZE;
    vorticity         = Scalar3d(memory3d + offset);    offset +=   NP*NP*NUM_LEV*VECTOR_SIZE;

    vstar             = Vector3d(memory3d + offset);    offset += 2*NP*NP*NUM_LEV*VECTOR_SIZE;
    divergence_temp   = Scalar3d(memory3d + offset);    offset +=   NP*NP*NUM_LEV*VECTOR_SIZE;
    v_vadv_buf        = Vector3d(memory3d + offset);    offset += 2*NP*NP*NUM_LEV*VECTOR_SIZE;
    t_vadv_buf        = Scalar3d(memory3d + offset);    offset +=   NP*NP*NUM_LEV*VECTOR_SIZE;
    eta_dot_dpdn_buf  = Scalar3d(memory3d + offset);    offset +=   NP*NP*NUM_LEV*VECTOR_SIZE;

    Errors::runtime_check (offset==size(), "Error! Something went wrong while creating the unmanaged views.\n", -1);

#ifdef DEBUG_TRACE
    kernel_start_times = ExecViewManaged<clock_t>("Start Times");
    kernel_end_times   = ExecViewManaged<clock_t>("End Times");
#endif
  }

  ExecViewUnmanaged<Scalar    [NP][NP][NUM_LEV]>      pressure;
  ExecViewUnmanaged<Scalar [2][NP][NP][NUM_LEV]>      pressure_grad;
  ExecViewUnmanaged<Scalar    [NP][NP][NUM_LEV]>      temperature_virt;
  ExecViewUnmanaged<Scalar [2][NP][NP][NUM_LEV]>      temperature_grad;
  ExecViewUnmanaged<Scalar    [NP][NP][NUM_LEV]>      omega_p;
  ExecViewUnmanaged<Scalar [2][NP][NP][NUM_LEV]>      vdp;
  ExecViewUnmanaged<Scalar    [NP][NP][NUM_LEV]>      div_vdp;
  ExecViewUnmanaged<Scalar    [NP][NP][NUM_LEV]>      ephi;
  ExecViewUnmanaged<Scalar [2][NP][NP][NUM_LEV]>      energy_grad;
  ExecViewUnmanaged<Scalar    [NP][NP][NUM_LEV]>      vorticity;

  // Buffers for EulerStepFunctor
  ExecViewUnmanaged<Scalar [2][NP][NP][NUM_LEV]>  vstar;

  // sdot_sum is used in case rsplit=0 and in energy diagnostics
  // (not yet coded).
  ExecViewUnmanaged<Real [NP][NP]> sdot_sum;

  // Buffers for spherical operators
  ExecViewUnmanaged<Scalar    [NP][NP][NUM_LEV]> divergence_temp;

  // Buffers for vertical advection terms in V and T for case
  // of Eulerian advection, rsplit=0. These buffers are used in both
  // cases, rsplit>0 and =0. Some of these values need to be init-ed
  // to zero at the beginning of each RK stage. Right now there is a code
  // for this, since Elements is a singleton.
  ExecViewUnmanaged<Scalar [2][NP][NP][NUM_LEV]> v_vadv_buf;
  ExecViewUnmanaged<Scalar [NP][NP][NUM_LEV]>    t_vadv_buf;
  ExecViewUnmanaged<Scalar [NP][NP][NUM_LEV]>    eta_dot_dpdn_buf;

  static int size () {
    return 9*(NP*NP*NUM_LEV*VECTOR_SIZE)  +  // Scalar 3d
           5*(2*NP*NP*NUM_LEV*VECTOR_SIZE) + // Vector 3d
           NP*NP;                            // Scalar 2d
  }
#ifdef DEBUG_TRACE
  clock_t kernel_start_times;
  clock_t kernel_end_times;
#endif
};

struct CaarFunctorImpl {

  struct CaarData {
    CaarData (const int rsplit_in) : rsplit(rsplit_in) {}
    int       nm1;
    int       n0;
    int       np1;
    int       n0_qdp;

    Real      dt;
    Real      eta_ave_w;

    const int rsplit;

    bool      compute_diagnostics;
  };

  ExecViewManaged<Real*>        m_buffers_storage;
  ExecViewManaged<CaarBuffers*> m_buffers;

  CaarData                      m_data;
  const HybridVCoord            m_hvcoord;
  const Elements                m_elements;
  const Tracers                 m_tracers;
  ReferenceElement::deriv_type  m_deriv;
  SphereOperators               m_sphere_ops;

  Kokkos::Array<std::shared_ptr<BoundaryExchange>, NUM_TIME_LEVELS> m_bes;

  CaarFunctorImpl(const Elements &elements, const Tracers &tracers,
                  const ReferenceElement& ref_FE, const HybridVCoord &hvcoord,
                  const SphereOperators &sphere_ops, 
                  const int rsplit)
      : m_data(rsplit)
      , m_hvcoord(hvcoord)
      , m_elements(elements)
      , m_tracers(tracers)
      , m_deriv(ref_FE.get_deriv())
      , m_sphere_ops(sphere_ops) {
    // Nothing to be done here
  }

  template<typename... Tags>
  void allocate_buffers (const Kokkos::TeamPolicy<ExecSpace,Tags...>& team_policy) {
    m_sphere_ops.allocate_buffers(team_policy);

    const int num_parallel_iterations = team_policy.league_size();
    const int alloc_dim = OnGpu<ExecSpace>::value ?
                          num_parallel_iterations : std::min(get_num_concurrent_teams(team_policy),num_parallel_iterations);

    m_buffers = ExecViewManaged<CaarBuffers*>("buffers",alloc_dim);
    auto h_buffers = Kokkos::create_mirror_view(m_buffers);
    int buf_size = CaarBuffers::size();
    m_buffers_storage = ExecViewManaged<Real*> ("", buf_size*alloc_dim );
    Real* memory = m_buffers_storage.data();
    for (int i=0; i<alloc_dim; ++i) {
      h_buffers(i).init(memory + i*buf_size);
    }
    Kokkos::deep_copy(m_buffers,h_buffers);
  }

  void init_boundary_exchanges (const std::shared_ptr<BuffersManager>& bm_exchange) {
    for (int tl=0; tl<NUM_TIME_LEVELS; ++tl) {
      m_bes[tl] = std::make_shared<BoundaryExchange>();
      auto& be = *m_bes[tl];
      be.set_buffers_manager(bm_exchange);
      be.set_num_fields(0,0,4);
      //be.register_field(m_elements.m_v,tl,2,0);
      //be.register_field(m_elements.m_t,1,tl);
      //be.register_field(m_elements.m_dp3d,1,tl);
      be.registration_completed();
    }
  }

  void set_n0_qdp (const int n0_qdp) { m_data.n0_qdp = n0_qdp; }

  void set_rk_stage_data (const int nm1, const int n0,   const int np1,
                          const Real dt, const Real eta_ave_w,
                          const bool compute_diagnostics) {
    m_data.nm1 = nm1;
    m_data.n0  = n0;
    m_data.np1 = np1;
    m_data.dt  = dt;

    m_data.eta_ave_w = eta_ave_w;
    m_data.compute_diagnostics = compute_diagnostics;
  }

  // Depends on PHI (after preq_hydrostatic), PECND
  // Modifies Ephi_grad
  // Computes \nabla (E + phi) + \nabla (P) * Rgas * T_v / P
  KOKKOS_INLINE_FUNCTION void compute_energy_grad(KernelVariables &kv) const {
    auto& elem    = m_elements.elems_d(kv.ie);
    auto& buffers = m_buffers(kv.team_idx);
    Kokkos::parallel_for(Kokkos::TeamThreadRange(kv.team, NP * NP),
                         [&](const int idx) {
      const int igp = idx / NP;
      const int jgp = idx % NP;
      Kokkos::parallel_for(Kokkos::ThreadVectorRange(kv.team, NUM_LEV), [&] (const int& ilev) {
        // pre-fill energy_grad with the pressure(_grad)-temperature part
        buffers.energy_grad(0, igp, jgp, ilev) =
            PhysicalConstants::Rgas *
            (buffers.temperature_virt(igp, jgp, ilev) /
             buffers.pressure(igp, jgp, ilev)) *
            buffers.pressure_grad(0, igp, jgp, ilev);

        buffers.energy_grad(1, igp, jgp, ilev) =
            PhysicalConstants::Rgas *
            (buffers.temperature_virt(igp, jgp, ilev) /
             buffers.pressure(igp, jgp, ilev)) *
            buffers.pressure_grad(1, igp, jgp, ilev);

        // Kinetic energy + PHI (geopotential energy) +
        Scalar k_energy =
            0.5 * (elem.m_v(m_data.n0, 0, igp, jgp, ilev) *
                       elem.m_v(m_data.n0, 0, igp, jgp, ilev) +
                   elem.m_v(m_data.n0, 1, igp, jgp, ilev) *
                       elem.m_v(m_data.n0, 1, igp, jgp, ilev));
        buffers.ephi(igp, jgp, ilev) += k_energy;
      });
    });
    kv.team_barrier();

    m_sphere_ops.gradient_sphere_update(kv, elem.m_geo, buffers.ephi, buffers.energy_grad);
  } // TESTED 1

  // Depends on pressure, PHI, U_current, V_current, METDET,
  // D, DINV, U, V, FCOR, SPHEREMP, T_v, ETA_DPDN
  KOKKOS_INLINE_FUNCTION void compute_phase_3(KernelVariables &kv) const {
    if (m_data.rsplit == 0) {
      // vertical Eulerian
      assign_zero_to_sdot_sum(kv);
      compute_eta_dot_dpdn_vertadv_euler(kv);
      preq_vertadv(kv);
      accumulate_eta_dot_dpdn(kv);
    }
    compute_omega_p(kv);
    compute_temperature_np1(kv);
    compute_velocity_np1(kv);
    compute_dp3d_np1(kv);
  } // TRIVIAL
  //is it?

  KOKKOS_INLINE_FUNCTION
  void print_debug(KernelVariables &kv, const int ie, const int which) const {
    if( kv.ie == ie ){
      for(int k = 0; k < NUM_PHYSICAL_LEV; ++k){
        const int ilev = k / VECTOR_SIZE;
        const int ivec = k % VECTOR_SIZE;
        int igp = 0, jgp = 0;
        Real val;
        if( which == 0)
          val = m_elements.elems_d(ie).m_t(m_data.np1, igp, jgp, ilev)[ivec];
        if( which == 1)
          val = m_elements.elems_d(ie).m_v(m_data.np1, 0, igp, jgp, ilev)[ivec];
        if( which == 2)
          val = m_elements.elems_d(ie).m_v(m_data.np1, 1, igp, jgp, ilev)[ivec];
        if( which == 3)
          val = m_elements.elems_d(ie).m_dp3d(m_data.np1, igp, jgp, ilev)[ivec];
        Kokkos::single(Kokkos::PerTeam(kv.team), [&] () {
            if( which == 0)
              std::printf("m_t %d (%d %d): % .17e \n", k, ilev, ivec, val);
            if( which == 1)
              std::printf("m_v(0) %d (%d %d): % .17e \n", k, ilev, ivec, val);
            if( which == 2)
              std::printf("m_v(1) %d (%d %d): % .17e \n", k, ilev, ivec, val);
            if( which == 3)
              std::printf("m_dp3d %d (%d %d): % .17e \n", k, ilev, ivec, val);
          });
      }
    }
  }

  // Depends on pressure, PHI, U_current, V_current, METDET,
  // D, DINV, U, V, FCOR, SPHEREMP, T_v
  KOKKOS_INLINE_FUNCTION
  void compute_velocity_np1(KernelVariables &kv) const {
    compute_energy_grad(kv);

    auto& elem    = m_elements.elems_d(kv.ie);
    auto& buffers = m_buffers(kv.team_idx);

    m_sphere_ops.vorticity_sphere(kv, elem.m_geo,
        Homme::subview(elem.m_v, m_data.n0),
        buffers.vorticity);

    const bool rsplit_gt0 = m_data.rsplit > 0;
    Kokkos::parallel_for(Kokkos::TeamThreadRange(kv.team, NP * NP),
                         [&](const int idx) {
      const int igp = idx / NP;
      const int jgp = idx % NP;
      Kokkos::parallel_for(Kokkos::ThreadVectorRange(kv.team, NUM_LEV), [&] (const int& ilev) {
        // Recycle vort to contain (fcor+vort)
        buffers.vorticity(igp, jgp, ilev) += elem.m_geo.m_fcor(igp, jgp);

        buffers.energy_grad(0, igp, jgp, ilev) *= -1;

        buffers.energy_grad(0, igp, jgp, ilev) +=
            (rsplit_gt0 ? 0 : - buffers.v_vadv_buf(0, igp, jgp, ilev)) +
            elem.m_v(m_data.n0, 1, igp, jgp, ilev) *
            buffers.vorticity(igp, jgp, ilev);

        buffers.energy_grad(1, igp, jgp, ilev) *= -1;

        buffers.energy_grad(1, igp, jgp, ilev) +=
            (rsplit_gt0 ? 0 : - buffers.v_vadv_buf(1, igp, jgp, ilev)) -
            elem.m_v(m_data.n0, 0, igp, jgp, ilev) *
            buffers.vorticity(igp, jgp, ilev);

        buffers.energy_grad(0, igp, jgp, ilev) *= m_data.dt;
        buffers.energy_grad(0, igp, jgp, ilev) +=
            elem.m_v(m_data.nm1, 0, igp, jgp, ilev);
        buffers.energy_grad(1, igp, jgp, ilev) *= m_data.dt;
        buffers.energy_grad(1, igp, jgp, ilev) +=
            elem.m_v(m_data.nm1, 1, igp, jgp, ilev);

        // Velocity at np1 = spheremp * buffer
        elem.m_v(m_data.np1, 0, igp, jgp, ilev) =
            elem.m_geo.m_spheremp(igp, jgp) *
            buffers.energy_grad(0, igp, jgp, ilev);
        elem.m_v(m_data.np1, 1, igp, jgp, ilev) =
            elem.m_geo.m_spheremp(igp, jgp) *
            buffers.energy_grad(1, igp, jgp, ilev);
      });
    });
    kv.team_barrier();
  } // UNTESTED 2
  //og: i'd better make a test for this

  //m_eta is zeroed outside of local kernels, in prim_step
  KOKKOS_INLINE_FUNCTION
  void accumulate_eta_dot_dpdn(KernelVariables &kv) const {
    auto& elem    = m_elements.elems_d(kv.ie);
    auto& buffers = m_buffers(kv.team_idx);
    Kokkos::parallel_for(Kokkos::TeamThreadRange(kv.team, NP * NP),
                         KOKKOS_LAMBDA(const int idx) {
      const int igp = idx / NP;
      const int jgp = idx % NP;
      for(int k = 0; k < NUM_LEV; ++k){
        elem.m_eta_dot_dpdn(igp, jgp, k) +=
           m_data.eta_ave_w * buffers.eta_dot_dpdn_buf(igp, jgp, k);
      }//k loop
    });
    kv.team_barrier();
  } //tested against caar_adjust_eta_dot_dpdn_c_int


  KOKKOS_INLINE_FUNCTION
  void assign_zero_to_sdot_sum(KernelVariables &kv) const {
    auto& buffers = m_buffers(kv.team_idx);
    Kokkos::parallel_for(Kokkos::TeamThreadRange(kv.team, NP * NP),
                         KOKKOS_LAMBDA(const int idx) {
      const int igp = idx / NP;
      const int jgp = idx % NP;
      buffers.sdot_sum(igp, jgp) = 0;
    });
    kv.team_barrier();
  } // TRIVIAL

  KOKKOS_INLINE_FUNCTION
  void compute_eta_dot_dpdn_vertadv_euler(KernelVariables &kv) const {
    auto& buffers = m_buffers(kv.team_idx);

    Kokkos::parallel_for(Kokkos::TeamThreadRange(kv.team, NP * NP),
                         KOKKOS_LAMBDA(const int idx) {
      const int igp = idx / NP;
      const int jgp = idx % NP;

      for(int k = 0; k < NUM_PHYSICAL_LEV-1; ++k){
        const int ilev = k / VECTOR_SIZE;
        const int ivec = k % VECTOR_SIZE;
        const int kp1 = k+1;
        const int ilevp1 = kp1 / VECTOR_SIZE;
        const int ivecp1 = kp1 % VECTOR_SIZE;
        buffers.sdot_sum(igp, jgp) +=
           buffers.div_vdp(igp, jgp, ilev)[ivec];
        buffers.eta_dot_dpdn_buf(igp, jgp, ilevp1)[ivecp1] =
           buffers.sdot_sum(igp, jgp);
      }//k loop
      //one more entry for sdot, separately, cause eta_dot_ is not of size LEV+1
      {
        const int ilev = (NUM_PHYSICAL_LEV - 1) / VECTOR_SIZE;
        const int ivec = (NUM_PHYSICAL_LEV - 1) % VECTOR_SIZE;
        buffers.sdot_sum(igp, jgp) +=
           buffers.div_vdp(igp, jgp, ilev)[ivec];
      }

      //note that index starts from 1
      for(int k = 1; k < NUM_PHYSICAL_LEV; ++k){
        const int ilev = k / VECTOR_SIZE;
        const int ivec = k % VECTOR_SIZE;
        buffers.eta_dot_dpdn_buf(igp, jgp, ilev)[ivec] =
           m_hvcoord.hybrid_bi(k)*buffers.sdot_sum(igp, jgp) -
           buffers.eta_dot_dpdn_buf(igp, jgp, ilev)[ivec];
      }//k loop
      buffers.eta_dot_dpdn_buf(igp, jgp, 0)[0] = 0.0;
    });//NP*NP loop
    kv.team_barrier();
  }//TESTED against compute_eta_dot_dpdn_vertadv_euler_c_int

  // Depends on PHIS, DP3D, PHI, pressure, T_v
  // Modifies PHI
  KOKKOS_INLINE_FUNCTION
  void preq_hydrostatic(KernelVariables &kv) const {
    preq_hydrostatic_impl<ExecSpace>(kv);
  } // TESTED 3

  // Depends on pressure, U_current, V_current, div_vdp,
  // omega_p
  KOKKOS_INLINE_FUNCTION
  void preq_omega_ps(KernelVariables &kv) const {
    preq_omega_ps_impl<ExecSpace>(kv);
  } // TESTED 4

  // Depends on DP3D
  KOKKOS_INLINE_FUNCTION
  void compute_pressure(KernelVariables &kv) const {
    compute_pressure_impl<ExecSpace>(kv);
  } // TESTED 5

  // Depends on DP3D, PHIS, DP3D, PHI, T_v
  // Modifies pressure, PHI
  KOKKOS_INLINE_FUNCTION
  void compute_scan_properties(KernelVariables &kv) const {
    compute_pressure(kv);
    preq_hydrostatic(kv);
    preq_omega_ps(kv);
  } // TRIVIAL

//should be renamed, instead of no tracer should be dry
  KOKKOS_INLINE_FUNCTION
  void compute_temperature_no_tracers_helper(KernelVariables &kv) const {
    auto& elem    = m_elements.elems_d(kv.ie);
    auto& buffers = m_buffers(kv.team_idx);
    Kokkos::parallel_for(Kokkos::TeamThreadRange(kv.team, NP * NP),
                         [&](const int idx) {
      const int igp = idx / NP;
      const int jgp = idx % NP;
      Kokkos::parallel_for(Kokkos::ThreadVectorRange(kv.team, NUM_LEV), [&] (const int& ilev) {
        buffers.temperature_virt(igp, jgp, ilev) = elem.m_t(m_data.n0, igp, jgp, ilev);
      });
    });
    kv.team_barrier();
  } // TESTED 6

//should be renamed
  KOKKOS_INLINE_FUNCTION
  void compute_temperature_tracers_helper(KernelVariables &kv) const {
    auto& elem    = m_elements.elems_d(kv.ie);
    auto& buffers = m_buffers(kv.team_idx);
    Kokkos::parallel_for(Kokkos::TeamThreadRange(kv.team, NP * NP),
                         [&](const int idx) {
      const int igp = idx / NP;
      const int jgp = idx % NP;
      Kokkos::parallel_for(Kokkos::ThreadVectorRange(kv.team, NUM_LEV), [&] (const int& ilev) {
        Scalar Qt = m_tracers.qdp(kv.ie, m_data.n0_qdp, 0, igp, jgp, ilev) /
                    elem.m_dp3d(m_data.n0, igp, jgp, ilev);
        Qt *= (PhysicalConstants::Rwater_vapor / PhysicalConstants::Rgas - 1.0);
        Qt += 1.0;
        buffers.temperature_virt(igp, jgp, ilev) =
            elem.m_t(m_data.n0, igp, jgp, ilev) * Qt;
      });
    });
    kv.team_barrier();
  } // TESTED 7

  // Depends on DERIVED_UN0, DERIVED_VN0, METDET, DINV
  // Initializes div_vdp, which is used 2 times afterwards
  // Modifies DERIVED_UN0, DERIVED_VN0
  // Requires NUM_LEV * 5 * NP * NP
  KOKKOS_INLINE_FUNCTION
  void compute_div_vdp(KernelVariables &kv) const {
    auto& elem    = m_elements.elems_d(kv.ie);
    auto& buffers = m_buffers(kv.team_idx);
    Kokkos::parallel_for(Kokkos::TeamThreadRange(kv.team, NP * NP),
                         [&](const int idx) {
      const int igp = idx / NP;
      const int jgp = idx % NP;
      Kokkos::parallel_for(Kokkos::ThreadVectorRange(kv.team, NUM_LEV), [&] (const int& ilev) {
        buffers.vdp(kv.team_idx, 0, igp, jgp, ilev) =
            elem.m_v(m_data.n0, 0, igp, jgp, ilev) *
            elem.m_dp3d(m_data.n0, igp, jgp, ilev);

        buffers.vdp(1, igp, jgp, ilev) =
            elem.m_v(m_data.n0, 1, igp, jgp, ilev) *
            elem.m_dp3d(m_data.n0, igp, jgp, ilev);

        elem.m_derived_vn0(0, igp, jgp, ilev) +=
            m_data.eta_ave_w * buffers.vdp(0, igp, jgp, ilev);

        elem.m_derived_vn0(1, igp, jgp, ilev) +=
            m_data.eta_ave_w * buffers.vdp(1, igp, jgp, ilev);
      });
    });
    kv.team_barrier();

    m_sphere_ops.divergence_sphere(kv,
        elem.m_geo, buffers.vdp, buffers.div_vdp);
  } // TESTED 8

  // Depends on T_current, DERIVE_UN0, DERIVED_VN0, METDET,
  // DINV
  // Might depend on QDP, DP3D_current
  KOKKOS_INLINE_FUNCTION
  void compute_temperature_div_vdp(KernelVariables &kv) const {
    if (m_data.n0_qdp < 0) {
      compute_temperature_no_tracers_helper(kv);
    } else {
      compute_temperature_tracers_helper(kv);
    }
    compute_div_vdp(kv);
  } // TESTED 9

  KOKKOS_INLINE_FUNCTION
  void compute_omega_p(KernelVariables &kv) const {
    auto& elem    = m_elements.elems_d(kv.ie);
    auto& buffers = m_buffers(kv.team_idx);
    Kokkos::parallel_for(Kokkos::TeamThreadRange(kv.team, NP * NP),
                         [&](const int idx) {
      const int igp = idx / NP;
      const int jgp = idx % NP;
      Kokkos::parallel_for(Kokkos::ThreadVectorRange(kv.team, NUM_LEV), [&] (const int& ilev) {
        elem.m_omega_p(igp, jgp, ilev) +=
            m_data.eta_ave_w * buffers.omega_p(igp, jgp, ilev);
      });
    });
    kv.team_barrier();
  } // TESTED 10

  // Depends on T (global), OMEGA_P (global), U (global), V
  // (global),
  // SPHEREMP (global), T_v, and omega_p
  // block_3d_scalars
  KOKKOS_INLINE_FUNCTION
  void compute_temperature_np1(KernelVariables &kv) const {
    auto& elem    = m_elements.elems_d(kv.ie);
    auto& buffers = m_buffers(kv.team_idx);

    m_sphere_ops.gradient_sphere(kv,
        elem.m_geo,
        Homme::subview(elem.m_t, m_data.n0),
        buffers.temperature_grad);

    const bool rsplit_gt0 = m_data.rsplit > 0;
    Kokkos::parallel_for(Kokkos::TeamThreadRange(kv.team, NP * NP),
                         [&](const int idx) {
      const int igp = idx / NP;
      const int jgp = idx % NP;

      Kokkos::parallel_for(Kokkos::ThreadVectorRange(kv.team, NUM_LEV), [&] (const int& ilev) {
        const Scalar vgrad_t =
            elem.m_v(m_data.n0, 0, igp, jgp, ilev) *
                buffers.temperature_grad(0, igp, jgp, ilev) +
            elem.m_v(m_data.n0, 1, igp, jgp, ilev) *
                buffers.temperature_grad(1, igp, jgp, ilev);

        const Scalar ttens =
              (rsplit_gt0 ? 0 : -buffers.t_vadv_buf(igp, jgp, ilev)) -
            vgrad_t +
            PhysicalConstants::kappa *
                buffers.temperature_virt(igp, jgp, ilev) *
                buffers.omega_p(igp, jgp, ilev);
        Scalar temp_np1 = ttens * m_data.dt +
                          elem.m_t(m_data.nm1, igp, jgp, ilev);
        temp_np1 *= elem.m_geo.m_spheremp(igp, jgp);
        elem.m_t(m_data.np1, igp, jgp, ilev) = temp_np1;
      });
    });
    kv.team_barrier();
  } // TESTED 11

  // Depends on DERIVED_UN0, DERIVED_VN0, U, V,
  // Modifies DERIVED_UN0, DERIVED_VN0, OMEGA_P, T, and DP3D
  KOKKOS_INLINE_FUNCTION
  void compute_dp3d_np1(KernelVariables &kv) const {
    auto& elem    = m_elements.elems_d(kv.ie);
    auto& buffers = m_buffers(kv.team_idx);
    Kokkos::parallel_for(Kokkos::TeamThreadRange(kv.team, NP * NP),
                         [&](const int idx) {
      const int igp = idx / NP;
      const int jgp = idx % NP;
      Kokkos::parallel_for(Kokkos::ThreadVectorRange(kv.team, NUM_LEV),
                           [&](const int &ilev) {
        Scalar tmp = buffers.eta_dot_dpdn_buf(kv.team_idx, igp, jgp, ilev);
        tmp.shift_left(1);
        tmp[VECTOR_SIZE - 1] = (ilev + 1 < NUM_LEV) ?
                                buffers.eta_dot_dpdn_buf(igp, jgp, ilev + 1)[0] : 0;
        // Add div_vdp before subtracting the previous value to eta_dot_dpdn
        // This will hopefully reduce numeric error
        tmp += buffers.div_vdp(igp, jgp, ilev);
        tmp -= buffers.eta_dot_dpdn_buf(igp, jgp, ilev);
        tmp = elem.m_dp3d(m_data.nm1, igp, jgp, ilev) -
              tmp * m_data.dt;

        elem.m_dp3d(m_data.np1, igp, jgp, ilev) =
            elem.m_geo.m_spheremp(igp, jgp) * tmp;
      });
    });
    kv.team_barrier();
  } // TESTED 12

  // depends on eta_dot_dpdn, dp3d, T, v, modifies v_vadv, t_vadv
  KOKKOS_INLINE_FUNCTION
  void preq_vertadv(KernelVariables &kv) const {
    auto& elem    = m_elements.elems_d(kv.ie);
    auto& buffers = m_buffers(kv.team_idx);
    Kokkos::parallel_for(Kokkos::TeamThreadRange(kv.team, NP * NP),
                         KOKKOS_LAMBDA(const int idx) {
      const int igp = idx / NP;
      const int jgp = idx % NP;

      // first level
      int k = 0;
      int ilev = k / VECTOR_SIZE;
      int ivec = k % VECTOR_SIZE;
      int kp1 = k + 1;
      int ilevp1 = kp1 / VECTOR_SIZE;
      int ivecp1 = kp1 % VECTOR_SIZE;

      // lets do this 1/dp thing to make it bfb with F and follow F for extra
      // (), not clear why
      Real facp =
          (0.5 * 1 /
           elem.m_dp3d(m_data.n0, igp, jgp, ilev)[ivec]) *
          buffers.eta_dot_dpdn_buf(igp, jgp, ilevp1)[ivecp1];
      Real facm;
      buffers.t_vadv_buf(igp, jgp, ilev)[ivec] =
          facp * (elem.m_t(m_data.n0, igp, jgp, ilevp1)[ivecp1] -
                  elem.m_t(m_data.n0, igp, jgp, ilev)[ivec]);
      buffers.v_vadv_buf(0, igp, jgp, ilev)[ivec] =
          facp *
          (elem.m_v(m_data.n0, 0, igp, jgp, ilevp1)[ivecp1] -
           elem.m_v(m_data.n0, 0, igp, jgp, ilev)[ivec]);
      buffers.v_vadv_buf(1, igp, jgp, ilev)[ivec] =
          facp *
          (elem.m_v(m_data.n0, 1, igp, jgp, ilevp1)[ivecp1] -
           elem.m_v(m_data.n0, 1, igp, jgp, ilev)[ivec]);

      for (k = 1; k < NUM_PHYSICAL_LEV - 1; ++k) {
        ilev = k / VECTOR_SIZE;
        ivec = k % VECTOR_SIZE;
        const int km1 = k - 1;
        const int ilevm1 = km1 / VECTOR_SIZE;
        const int ivecm1 = km1 % VECTOR_SIZE;
        kp1 = k + 1;
        ilevp1 = kp1 / VECTOR_SIZE;
        ivecp1 = kp1 % VECTOR_SIZE;

        facp = 0.5 *
               (1 / elem.m_dp3d(m_data.n0, igp, jgp, ilev)[ivec]) *
               buffers.eta_dot_dpdn_buf(igp, jgp, ilevp1)[ivecp1];
        facm = 0.5 *
               (1 / elem.m_dp3d(m_data.n0, igp, jgp, ilev)[ivec]) *
               buffers.eta_dot_dpdn_buf(igp, jgp, ilev)[ivec];

        buffers.t_vadv_buf(igp, jgp, ilev)[ivec] =
            facp * (elem.m_t(m_data.n0, igp, jgp, ilevp1)[ivecp1] -
                    elem.m_t(m_data.n0, igp, jgp, ilev)[ivec]) +
            facm * (elem.m_t(m_data.n0, igp, jgp, ilev)[ivec] -
                    elem.m_t(m_data.n0, igp, jgp, ilevm1)[ivecm1]);

        buffers.v_vadv_buf(0, igp, jgp, ilev)[ivec] =
            facp *
                (elem.m_v(m_data.n0, 0, igp, jgp, ilevp1)[ivecp1] -
                 elem.m_v(m_data.n0, 0, igp, jgp, ilev)[ivec]) +
            facm *
                (elem.m_v(m_data.n0, 0, igp, jgp, ilev)[ivec] -
                 elem.m_v(m_data.n0, 0, igp, jgp, ilevm1)[ivecm1]);

        buffers.v_vadv_buf(1, igp, jgp, ilev)[ivec] =
            facp *
                (elem.m_v(m_data.n0, 1, igp, jgp, ilevp1)[ivecp1] -
                 elem.m_v(m_data.n0, 1, igp, jgp, ilev)[ivec]) +
            facm *
                (elem.m_v(m_data.n0, 1, igp, jgp, ilev)[ivec] -
                 elem.m_v(m_data.n0, 1, igp, jgp, ilevm1)[ivecm1]);
      } // k loop

      k = NUM_PHYSICAL_LEV - 1;
      ilev = k / VECTOR_SIZE;
      ivec = k % VECTOR_SIZE;
      const int km1 = k - 1;
      const int ilevm1 = km1 / VECTOR_SIZE;
      const int ivecm1 = km1 % VECTOR_SIZE;
      // note the (), just to comply with F
      facm = (0.5 *
              (1 / elem.m_dp3d(m_data.n0, igp, jgp, ilev)[ivec])) *
             buffers.eta_dot_dpdn_buf(igp, jgp, ilev)[ivec];

      buffers.t_vadv_buf(igp, jgp, ilev)[ivec] =
          facm * (elem.m_t(m_data.n0, igp, jgp, ilev)[ivec] -
                  elem.m_t(m_data.n0, igp, jgp, ilevm1)[ivecm1]);

      buffers.v_vadv_buf(0, igp, jgp, ilev)[ivec] =
          facm *
          (elem.m_v(m_data.n0, 0, igp, jgp, ilev)[ivec] -
           elem.m_v(m_data.n0, 0, igp, jgp, ilevm1)[ivecm1]);

      buffers.v_vadv_buf(1, igp, jgp, ilev)[ivec] =
          facm *
          (elem.m_v(m_data.n0, 1, igp, jgp, ilev)[ivec] -
           elem.m_v(m_data.n0, 1, igp, jgp, ilevm1)[ivecm1]);
    }); // NP*NP
    kv.team_barrier();
  } // TESTED against preq_vertadv

  KOKKOS_INLINE_FUNCTION
  void operator()(const TeamMember &team) const {
    KernelVariables kv(team);

    compute_temperature_div_vdp(kv);
    kv.team.team_barrier();

    compute_scan_properties(kv);
    kv.team.team_barrier();

    compute_phase_3(kv);
  }

  KOKKOS_INLINE_FUNCTION
  size_t shmem_size(const int team_size) const {
    return KernelVariables::shmem_size(team_size);
  }

private:
  template <typename ExecSpaceType>
  KOKKOS_INLINE_FUNCTION typename std::enable_if<
      !std::is_same<ExecSpaceType, Hommexx_Cuda>::value, void>::type
  compute_pressure_impl(KernelVariables &kv) const {
    auto& buffers = m_buffers(kv.team_idx);
    auto& elem    = m_elements.elems_d(kv.ie);
    Kokkos::parallel_for(Kokkos::TeamThreadRange(kv.team, NP * NP),
                         [&](const int loop_idx) {
      const int igp = loop_idx / NP;
      const int jgp = loop_idx % NP;

      Real dp_prev = 0;
      Real p_prev = m_hvcoord.hybrid_ai0 * m_hvcoord.ps0;

      for (int ilev = 0; ilev < NUM_LEV; ++ilev) {
        const int vector_end =
            (ilev == NUM_LEV - 1
                 ? ((NUM_PHYSICAL_LEV + VECTOR_SIZE - 1) % VECTOR_SIZE)
                 : VECTOR_SIZE - 1);

        auto p = buffers.pressure(igp, jgp, ilev);
        const auto &dp = elem.m_dp3d(m_data.n0, igp, jgp, ilev);

        for (int iv = 0; iv <= vector_end; ++iv) {
          // p[k] = p[k-1] + 0.5*(dp[k-1] + dp[k])
          p[iv] = p_prev + 0.5 * (dp_prev + dp[iv]);
          // Update p[k-1] and dp[k-1]
          p_prev = p[iv];
          dp_prev = dp[iv];
        }
        buffers.pressure(kv.team_idx, igp, jgp, ilev) = p;
      };
    });
    kv.team_barrier();
  }

  template <typename ExecSpaceType>
  KOKKOS_INLINE_FUNCTION typename std::enable_if<
      std::is_same<ExecSpaceType, Hommexx_Cuda>::value, void>::type
  compute_pressure_impl(KernelVariables &kv) const {
    auto& elem    = m_elements.elems_d(kv.ie);
    auto& buffers = m_buffers(kv.team_idx);
    Kokkos::parallel_for(Kokkos::TeamThreadRange(kv.team, NP * NP),
                         [&](const int loop_idx) {
      const int igp = loop_idx / NP;
      const int jgp = loop_idx % NP;

      ExecViewUnmanaged<Real[NUM_PHYSICAL_LEV]>        p (&buffers.pressure(igp,jgp,0)[0]);
      ExecViewUnmanaged<const Real[NUM_PHYSICAL_LEV]> dp (&elem.m_dp3d(m_data.n0,igp,jgp,0)[0]);

      const Real p0 = m_hvcoord.hybrid_ai0 * m_hvcoord.ps0 + 0.5*dp(0);
      Kokkos::single(Kokkos::PerThread(kv.team),[&](){
        p(0) = p0;
      });
      // Compute cumsum of (dp(k-1) + dp(k)) in [1,NUM_PHYSICAL_LEV] and update p(k)
      // For level=1 (which is k=0), first add p0=hyai0*ps0 + 0.5*dp(0).
      Dispatch<ExecSpaceType>::parallel_scan(kv.team, NUM_PHYSICAL_LEV-1,
                                            [&](const int k, Real& accumulator, const bool last) {

        // Note: the actual level must range in [1,NUM_PHYSICAL_LEV], while k ranges in [0, NUM_PHYSICAL_LEV-1].
        // If k=0, add the p0 part: hyai0*ps0 + 0.5*dp(0)
        accumulator += (k==0 ? p0 : 0);
        accumulator += (dp(k) + dp(k+1))/2;

        if (last) {
          p(k+1) = accumulator;
        }
      });
    });
    kv.team_barrier();
  }

  template <typename ExecSpaceType>
  KOKKOS_INLINE_FUNCTION typename std::enable_if<
      !std::is_same<ExecSpaceType, Hommexx_Cuda>::value, void>::type
  preq_hydrostatic_impl(KernelVariables &kv) const {
    auto& buffers = m_buffers(kv.team_idx);
    auto& elem    = m_elements.elems_d(kv.ie);
    Kokkos::parallel_for(Kokkos::TeamThreadRange(kv.team, NP * NP),
                         [&](const int loop_idx) {
      const int igp = loop_idx / NP;
      const int jgp = loop_idx % NP;

      // Note: we add VECTOR_SIZE-1 rather than subtracting 1 since (0-1)%N=-1
      // while (0+N-1)%N=N-1.
      constexpr int last_lvl_last_vector_idx =
          (NUM_PHYSICAL_LEV + VECTOR_SIZE - 1) % VECTOR_SIZE;

      Real integration = 0;
      for (int ilev = NUM_LEV - 1; ilev >= 0; --ilev) {
        const int vec_start = (ilev == (NUM_LEV - 1) ? last_lvl_last_vector_idx
                                                     : VECTOR_SIZE - 1);

        const Real phis  = elem.m_phis(igp, jgp);
              auto &phi  = buffers.ephi(igp, jgp, ilev);
        const auto &t_v  = buffers.temperature_virt(igp, jgp, ilev);
        const auto &dp3d = elem.m_dp3d(m_data.n0, igp, jgp, ilev);
        const auto &p    = buffers.pressure(igp, jgp, ilev);

        // Precompute this product as a SIMD operation
        const auto rgas_tv_dp_over_p =
            PhysicalConstants::Rgas * t_v * (dp3d * 0.5 / p);

        // Integrate
        Scalar integration_ij;
        integration_ij[vec_start] = integration;
        for (int iv = vec_start - 1; iv >= 0; --iv)
          integration_ij[iv] =
              integration_ij[iv + 1] + rgas_tv_dp_over_p[iv + 1];

        // Add integral and constant terms to phi
        phi = phis + 2.0 * integration_ij + rgas_tv_dp_over_p;
        integration = integration_ij[0] + rgas_tv_dp_over_p[0];
      }
    });
    kv.team_barrier();
  }

  static KOKKOS_INLINE_FUNCTION void assert_vector_size_1() {
#ifndef NDEBUG
    if (VECTOR_SIZE != 1)
      Kokkos::abort("This impl is for GPU, for which VECTOR_SIZE is 1. It will "
                    "not work if VECTOR_SIZE > 1. Eventually, we may get "
                    "VECTOR_SIZE > 1 on GPU, at which point the alternative to "
                    "this impl will be the one to use, anyway.");
#endif
  }

  // CUDA version
  template <typename ExecSpaceType>
  KOKKOS_INLINE_FUNCTION typename std::enable_if<
      std::is_same<ExecSpaceType, Hommexx_Cuda>::value, void>::type
  preq_hydrostatic_impl(KernelVariables &kv) const {
    assert_vector_size_1();
    auto& buffers = m_buffers(kv.team_idx);
    auto& elem    = m_elements.elems_d(kv.ie);
    Kokkos::parallel_for(Kokkos::TeamThreadRange(kv.team, NP * NP),
                         [&](const int loop_idx) {
      const int igp = loop_idx / NP;
      const int jgp = loop_idx % NP;

      // Use currently unused buffers to store one column of data.
      const auto rgas_tv_dp_over_p = Homme::subview(buffers.vstar, 0, igp, jgp);
      const auto integration = Homme::subview(buffers.divergence_temp,igp,jgp);

      // Precompute this product
      Kokkos::parallel_for(Kokkos::ThreadVectorRange(kv.team, NUM_LEV),
                           [&](const int &ilev) {
        const auto &t_v  =  buffers.temperature_virt(igp, jgp, ilev);
        const auto &dp3d = elem.m_dp3d(m_data.n0, igp, jgp, ilev);
        const auto &p    = buffers.pressure(igp, jgp, ilev);

        rgas_tv_dp_over_p(ilev) =
            PhysicalConstants::Rgas * t_v * (dp3d * 0.5 / p);

        // Init integration to 0, so it's ready for later
        integration(ilev) = 0.0;
      });

      // accumulate rgas_tv_dp_over_p in [NUM_PHYSICAL_LEV,1].
      // Store sum(NUM_PHYSICAL_LEV,k) in integration(k-1)
      Dispatch<ExecSpaceType>::parallel_scan(kv.team, NUM_PHYSICAL_LEV-1,
                                            [&](const int k, Real& accumulator, const bool last) {
        // level must range in [NUM_PHYSICAL_LEV-1,1], while k ranges in [0, NUM_PHYSICAL_LEV-2].
        const int level = NUM_PHYSICAL_LEV-1-k;

        accumulator += rgas_tv_dp_over_p(level)[0];

        if (last) {
          integration(level-1) = accumulator;
        }
      });

      // Update phi with a parallel_for: phi(k)= phis + 2.0 * integration(k+1) + rgas_tv_dp_over_p(k);
      const Real phis = elem.m_phis(igp, jgp);
      const auto phi = Homme::subview(buffers.ephi,igp, jgp);
      Kokkos::parallel_for(Kokkos::ThreadVectorRange(kv.team, NUM_LEV),
                           [&](const int level) {
        phi(level) = phis + 2.0 * integration(level) + rgas_tv_dp_over_p(level);
      });
    });
    kv.team_barrier();
  }

  // CUDA version
  template <typename ExecSpaceType>
  KOKKOS_INLINE_FUNCTION typename std::enable_if<
      std::is_same<ExecSpaceType, Hommexx_Cuda>::value, void>::type
  preq_omega_ps_impl(KernelVariables &kv) const {
    auto& buffers = m_buffers(kv.team_idx);
    auto& elem    = m_elements.elems_d(kv.ie);
    assert_vector_size_1();
#ifdef DEBUG_TRACE
    Kokkos::single(Kokkos::PerTeam(kv.team), [&]() {
      buffers.kernel_start_times() = clock();
    });
#endif
    m_sphere_ops.gradient_sphere(
        kv, elem.m_geo, buffers.pressure, buffers.pressure_grad);

    Kokkos::parallel_for(Kokkos::TeamThreadRange(kv.team, NP * NP),
                         [&](const int loop_idx) {
      const int igp = loop_idx / NP;
      const int jgp = loop_idx % NP;

      ExecViewUnmanaged<Real[NUM_PHYSICAL_LEV]> omega_p(&buffers.omega_p(igp, jgp, 0)[0]);
      ExecViewUnmanaged<Real[NUM_PHYSICAL_LEV]> div_vdp(&buffers.div_vdp(igp, jgp, 0)[0]);
      Dispatch<ExecSpaceType>::parallel_scan(kv.team, NUM_PHYSICAL_LEV,
                                            [&](const int level, Real& accumulator, const bool last) {
        if (last) {
          omega_p(level) = accumulator;
        }

        accumulator += div_vdp(level);
      });

      Kokkos::parallel_for(Kokkos::ThreadVectorRange(kv.team, NUM_LEV),
                           [&](const int ilev) {
        const Scalar vgrad_p =
            elem.m_v(m_data.n0, 0, igp, jgp, ilev) *
                buffers.pressure_grad(0, igp, jgp, ilev) +
            elem.m_v(m_data.n0, 1, igp, jgp, ilev) *
                buffers.pressure_grad(1, igp, jgp, ilev);

        const auto &p = buffers.pressure(igp, jgp, ilev);
        buffers.omega_p(igp, jgp, ilev) =
            (vgrad_p -
             (buffers.omega_p(igp, jgp, ilev) +
              0.5 * buffers.div_vdp(igp, jgp, ilev))) /
            p;
      });
    });
#ifdef DEBUG_TRACE
    Kokkos::single(Kokkos::PerTeam(kv.team), [&]() {
      buffers.kernel_end_times(kv.ie) = clock();
    });
#endif
  }

  // Non-CUDA version
  template <typename ExecSpaceType>
  KOKKOS_INLINE_FUNCTION typename std::enable_if<
      !std::is_same<ExecSpaceType, Hommexx_Cuda>::value, void>::type
  preq_omega_ps_impl(KernelVariables &kv) const {
    auto& buffers = m_buffers(kv.team_idx);
    auto& elem    = m_elements.elems_d(kv.ie);
    m_sphere_ops.gradient_sphere(
        kv, elem.m_geo, buffers.pressure, buffers.pressure_grad);

    Kokkos::parallel_for(Kokkos::TeamThreadRange(kv.team, NP * NP),
                         [&](const int loop_idx) {
      const int igp = loop_idx / NP;
      const int jgp = loop_idx % NP;

      Real integration = 0;
      for (int ilev = 0; ilev < NUM_LEV; ++ilev) {
        const int vector_end =
            (ilev == NUM_LEV - 1
                 ? ((NUM_PHYSICAL_LEV + VECTOR_SIZE - 1) % VECTOR_SIZE)
                 : VECTOR_SIZE - 1);

        const Scalar vgrad_p =
            elem.m_v(m_data.n0, 0, igp, jgp, ilev) *
                buffers.pressure_grad(0, igp, jgp, ilev) +
            elem.m_v(m_data.n0, 1, igp, jgp, ilev) *
                buffers.pressure_grad(1, igp, jgp, ilev);
        auto &omega_p = buffers.omega_p(igp, jgp, ilev);
        const auto &p = buffers.pressure(igp, jgp, ilev);
        const auto &div_vdp =
            buffers.div_vdp(igp, jgp, ilev);

        Scalar integration_ij;
        integration_ij[0] = integration;
        for (int iv = 0; iv < vector_end; ++iv)
          integration_ij[iv + 1] = integration_ij[iv] + div_vdp[iv];
        omega_p = (vgrad_p - (integration_ij + 0.5 * div_vdp)) / p;
        integration = integration_ij[vector_end] + div_vdp[vector_end];
      }
    });
  }
};

} // Namespace Homme

#endif // HOMMEXX_CAAR_FUNCTOR_IMPL_HPP
