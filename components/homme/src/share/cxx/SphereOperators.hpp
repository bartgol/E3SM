/********************************************************************************
 * HOMMEXX 1.0: Copyright of Sandia Corporation
 * This software is released under the BSD license
 * See the file 'COPYRIGHT' in the HOMMEXX/src/share/cxx directory
 *******************************************************************************/

#ifndef HOMMEXX_SPHERE_OPERATORS_HPP
#define HOMMEXX_SPHERE_OPERATORS_HPP

#include "Types.hpp"
#include "ElementGeometry.hpp"
#include "ReferenceElement.hpp"
#include "KernelVariables.hpp"
#include "PhysicalConstants.hpp"
#include "utilities/SubviewUtils.hpp"

#include <Kokkos_Core.hpp>

namespace Homme {

class SphereOperators
{
public:

  SphereOperators () : m_inited(false) {}

  SphereOperators (const ReferenceElement& refFE) {
    init (refFE);
  }

  bool is_inited () const { return m_inited; }

  void init (const ReferenceElement& refFE) {
    // Get derivative and mass matrices
    m_deriv = refFE.get_deriv();
    m_mass  = refFE.get_mass();

    m_inited = true;
  }

  template<typename... Tags>
  void allocate_buffers (const Kokkos::TeamPolicy<ExecSpace,Tags...>& team_policy)
  {
    const int num_parallel_iterations = team_policy.league_size();
    const int alloc_dim = OnGpu<ExecSpace>::value ?
                          num_parallel_iterations : std::min(get_num_concurrent_teams(team_policy),num_parallel_iterations);

    if (m_buffers.size()<alloc_dim) {
      m_buffers_storage = ExecViewManaged<Real*>("storage",alloc_dim*SphereOpBuffers::size());
      auto h_buffers = Kokkos::create_mirror_view(m_buffers);
      Real* memory = m_buffers_storage.data();
      int offset = 0;
      for (int i=0; i<alloc_dim; ++i) {
        auto& b = h_buffers(i);
        b.vector_buf_sl = decltype(b.vector_buf_sl) (memory+offset);
        offset += NUM_2D_VECTOR_BUFFERS*2*NP*NP;
        b.scalar_buf_ml = decltype(b.scalar_buf_ml) (reinterpret_cast<Scalar*>(memory+offset));
        offset += NUM_3D_SCALAR_BUFFERS*NP*NP*NUM_LEV*VECTOR_SIZE;
        b.vector_buf_ml = decltype(b.vector_buf_ml) (reinterpret_cast<Scalar*>(memory+offset));
        offset += NUM_3D_VECTOR_BUFFERS*2*NP*NP*NUM_LEV*VECTOR_SIZE;
      }
    }
  }

// ================ SINGLE-LEVEL IMPLEMENTATION =========================== //

  KOKKOS_INLINE_FUNCTION void
  gradient_sphere_sl (const KernelVariables &kv,
                      const ElementGeometry& geo,
                      const ExecViewUnmanaged<const Real    [NP][NP]> scalar,
                      const ExecViewUnmanaged<      Real [2][NP][NP]> grad_s) const
  {
    const auto& D_inv = geo.m_dinv;
    const auto& bufs = m_buffers(kv.team_idx);
    const auto& temp_v_buf = Homme::subview(bufs.vector_buf_sl,0);
    constexpr int np_squared = NP * NP;
    // TODO: Use scratch space for this
    Kokkos::parallel_for(Kokkos::TeamThreadRange(kv.team, np_squared),
                         [&](const int loop_idx) {
      const int j = loop_idx / NP;
      const int l = loop_idx % NP;
      Real dsdx(0), dsdy(0);
      for (int i = 0; i < NP; ++i) {
        dsdx += m_deriv(l, i) * scalar(j, i);
        dsdy += m_deriv(l, i) * scalar(i, j);
      }
      temp_v_buf(0, j, l) = dsdx * PhysicalConstants::rrearth;
      temp_v_buf(1, l, j) = dsdy * PhysicalConstants::rrearth;
    });
    kv.team_barrier();

    constexpr int grad_iters = 2 * NP * NP;
    Kokkos::parallel_for(Kokkos::TeamThreadRange(kv.team, grad_iters),
                         [&](const int loop_idx) {
      const int h = (loop_idx / NP) / NP;
      const int i = (loop_idx / NP) % NP;
      const int j = loop_idx % NP;
      grad_s(h, j, i) = D_inv(h, 0, j, i) * temp_v_buf(0, j, i) +
                        D_inv(h, 1, j, i) * temp_v_buf(1, j, i);
    });
    kv.team_barrier();
  }

  KOKKOS_INLINE_FUNCTION void
  gradient_sphere_update_sl (const KernelVariables &kv,
                             const ElementGeometry& geo,
                             const ExecViewUnmanaged<const Real    [NP][NP]> scalar,
                             const ExecViewUnmanaged<      Real [2][NP][NP]> grad_s) const
  {
    constexpr int np_squared = NP * NP;
    const auto& D_inv = geo.m_dinv;
    const auto& bufs = m_buffers(kv.team_idx);
    const auto& temp_v_buf = Homme::subview(bufs.vector_buf_sl,0);
    Kokkos::parallel_for(Kokkos::TeamThreadRange(kv.team, np_squared),
                         [&](const int loop_idx) {
      const int j = loop_idx / NP;
      const int l = loop_idx % NP;
      Real dsdx(0), dsdy(0);
      for (int i = 0; i < NP; ++i) {
        dsdx += m_deriv(l, i) * scalar(j, i);
        dsdy += m_deriv(l, i) * scalar(i, j);
      }
      temp_v_buf(0, j, l) = dsdx * PhysicalConstants::rrearth;
      temp_v_buf(1, l, j) = dsdy * PhysicalConstants::rrearth;
    });
    kv.team_barrier();

    constexpr int grad_iters = NP * NP;
    Kokkos::parallel_for(Kokkos::TeamThreadRange(kv.team, grad_iters),
                         [&](const int loop_idx) {
      const int i = loop_idx / NP;
      const int j = loop_idx % NP;
      const auto& tmp0 = temp_v_buf(0,i,j);
      const auto& tmp1 = temp_v_buf(1,i,j);
      grad_s(0,i,j) += D_inv(0,0,i,j) * tmp0 + D_inv(0,1,i,j) * tmp1;
      grad_s(1,i,j) += D_inv(1,0,i,j) * tmp0 + D_inv(1,1,i,j) * tmp1;
    });
    kv.team_barrier();
  }

  KOKKOS_INLINE_FUNCTION void
  divergence_sphere_sl (const KernelVariables &kv,
                        const ElementGeometry& geo,
                        const ExecViewUnmanaged<const Real [2][NP][NP]> v,
                        const ExecViewUnmanaged<      Real    [NP][NP]> div_v) const
  {
    const auto& metdet = geo.m_metdet;
    const auto& D_inv  = geo.m_dinv;
    const auto& bufs = m_buffers(kv.team_idx);
    const auto& gv_buf = Homme::subview(bufs.vector_buf_sl,0);
    constexpr int np_squared = NP * NP;
    Kokkos::parallel_for(Kokkos::TeamThreadRange(kv.team, np_squared),
                         [&](const int loop_idx) {
      const int igp = loop_idx / NP;
      const int jgp = loop_idx % NP;
      const auto& v0 = v(0,igp,jgp);
      const auto& v1 = v(1,igp,jgp);
      gv_buf(0,igp,jgp) = (D_inv(0,0,igp,jgp) * v0 + D_inv(1,0,igp,jgp) * v1) * metdet(igp,jgp);
      gv_buf(1,igp,jgp) = (D_inv(0,1,igp,jgp) * v0 + D_inv(1,1,igp,jgp) * v1) * metdet(igp,jgp);
    });
    kv.team_barrier();

    constexpr int div_iters = NP * NP;
    Kokkos::parallel_for(Kokkos::TeamThreadRange(kv.team, div_iters),
                         [&](const int loop_idx) {
      const int igp = loop_idx / NP;
      const int jgp = loop_idx % NP;
      Real dudx = 0.0, dvdy = 0.0;
      for (int kgp = 0; kgp < NP; ++kgp) {
        dudx += m_deriv(jgp, kgp) * gv_buf(0, igp, kgp);
        dvdy += m_deriv(igp, kgp) * gv_buf(1, kgp, jgp);
      }
      div_v(igp,jgp) = (dudx + dvdy) * ((1.0 / metdet(igp,jgp)) *
                                         PhysicalConstants::rrearth);
    });
    kv.team_barrier();
  }

  KOKKOS_INLINE_FUNCTION void
  divergence_sphere_wk_sl (const KernelVariables &kv,
                           const ElementGeometry& geo,
                           const ExecViewUnmanaged<const Real [2][NP][NP]> v,
                           const ExecViewUnmanaged<      Real    [NP][NP]> div_v) const
  {
    const auto& D_inv    = geo.m_dinv;
    const auto& spheremp = geo.m_spheremp;
    const auto& bufs = m_buffers(kv.team_idx);
    const auto& gv_buf = Homme::subview(bufs.vector_buf_sl,0);

    // copied from strong divergence as is but without metdet
    // conversion to contravariant
    constexpr int np_squared = NP * NP;
    Kokkos::parallel_for(Kokkos::TeamThreadRange(kv.team, np_squared),
                         [&](const int loop_idx) {
      const int igp = loop_idx / NP;
      const int jgp = loop_idx % NP;
      const auto& v0 = v(0,igp,jgp);
      const auto& v1 = v(1,igp,jgp);
      gv_buf(0,igp,jgp) = D_inv(0,0,igp,jgp) * v0 + D_inv(1,0,igp,jgp) * v1;
      gv_buf(1,igp,jgp) = D_inv(0,1,igp,jgp) * v0 + D_inv(1,1,igp,jgp) * v1;
    });
    kv.team_barrier();

    // in strong div
    // kgp = i in strong code, jgp=j, igp=l
    // in weak div, n is like j in strong div,
    // n(weak)=j(strong)=jgp
    // m(weak)=l(strong)=igp
    // j(weak)=i(strong)=kgp
    constexpr int div_iters = NP * NP;
    // keeping indices' names as in F
    Kokkos::parallel_for(Kokkos::TeamThreadRange(kv.team, div_iters),
                         [&](const int loop_idx) {
      // Note: for this one time, it is better if m strides faster, due to
      //       the way the views are accessed.
      const int mgp = loop_idx % NP;
      const int ngp = loop_idx / NP;
      Real dd = 0.0;
      for (int jgp = 0; jgp < NP; ++jgp) {
        dd -= (spheremp(ngp, jgp) * gv_buf(0, ngp, jgp) * m_deriv(jgp, mgp) +
               spheremp(jgp, mgp) * gv_buf(1, jgp, mgp) * m_deriv(jgp, ngp)) *
              PhysicalConstants::rrearth;
      }
      div_v(ngp, mgp) = dd;
    });
    kv.team_barrier();

  } // end of divergence_sphere_wk_sl

  // Note that divergence_sphere requires scratch space of 3 x NP x NP Reals
  // This must be called from the device space
  KOKKOS_INLINE_FUNCTION void
  vorticity_sphere_sl (const KernelVariables &kv,
                       const ElementGeometry& geo,
                       const ExecViewUnmanaged<const Real [NP][NP]> u,
                       const ExecViewUnmanaged<const Real [NP][NP]> v,
                       const ExecViewUnmanaged<      Real [NP][NP]> vort) const
  {
    const auto& D      = geo.m_d;
    const auto& metdet = geo.m_metdet;
    const auto& bufs = m_buffers(kv.team_idx);
    const auto& vcov_buf = Homme::subview(bufs.vector_buf_sl,0);

    constexpr int np_squared = NP * NP;
    Kokkos::parallel_for(Kokkos::TeamThreadRange(kv.team, np_squared),
                         [&](const int loop_idx) {
      const int igp = loop_idx / NP;
      const int jgp = loop_idx % NP;
      const auto& u_ij = u(igp,jgp);
      const auto& v_ij = v(igp,jgp);
      vcov_buf(0,igp,jgp) = D(0,0,igp,jgp) * u_ij + D(0,1,igp,jgp) * v_ij;
      vcov_buf(1,igp,jgp) = D(1,0,igp,jgp) * u_ij + D(1,1,igp,jgp) * v_ij;
    });
    kv.team_barrier();

    constexpr int vort_iters = NP * NP;
    Kokkos::parallel_for(Kokkos::TeamThreadRange(kv.team, vort_iters),
                         [&](const int loop_idx) {
      const int igp = loop_idx / NP;
      const int jgp = loop_idx % NP;
      Real dudy = 0.0;
      Real dvdx = 0.0;
      for (int kgp = 0; kgp < NP; ++kgp) {
        dvdx += m_deriv(jgp, kgp) * vcov_buf(1, igp, kgp);
        dudy += m_deriv(igp, kgp) * vcov_buf(0, kgp, jgp);
      }

      vort(igp, jgp) = (dvdx - dudy) * ((1.0 / metdet(igp, jgp)) *
                                        PhysicalConstants::rrearth);
    });
    kv.team_barrier();
  }

  // analog of fortran's laplace_wk_sphere
  // Single level implementation
  KOKKOS_INLINE_FUNCTION void
  laplace_wk_sl (const KernelVariables &kv,
                 const ElementGeometry& geo,
                 const ExecViewUnmanaged<const Real [NP][NP]> field,
                 const ExecViewUnmanaged<      Real [NP][NP]> laplace) const
  {
    const auto& bufs = m_buffers(kv.team_idx);
    const auto& grad_s = Homme::subview(bufs.vector_buf_sl, 1);
    gradient_sphere_sl(kv, geo, field, grad_s);
    divergence_sphere_wk_sl(kv, geo, grad_s, laplace);
  } // end of laplace_wk_sl

  // ================ MULTI-LEVEL IMPLEMENTATION =========================== //

  template<int NUM_LEV_REQUEST = NUM_LEV>
  KOKKOS_INLINE_FUNCTION void
  gradient_sphere (const KernelVariables &kv,
                   const ElementGeometry& geo,
                   const ExecViewUnmanaged<const Scalar    [NP][NP][NUM_LEV]> scalar,
                   const ExecViewUnmanaged<      Scalar [2][NP][NP][NUM_LEV]> grad_s) const
  {
    static_assert(NUM_LEV_REQUEST>0, "Error! Template argument NUM_LEV_REQUEST must be positive.\n");

    const auto& D_inv = geo.m_dinv;

    constexpr int np_squared = NP * NP;
    Kokkos::parallel_for(Kokkos::TeamThreadRange(kv.team, np_squared),
                         [&](const int loop_idx) {
      const int igp = loop_idx / NP;
      const int jgp = loop_idx % NP;
      Kokkos::parallel_for(Kokkos::ThreadVectorRange(kv.team, NUM_LEV_REQUEST), [&] (const int& ilev) {
        Scalar v0, v1;
        for (int kgp = 0; kgp < NP; ++kgp) {
          v0 += m_deriv(jgp, kgp) * scalar(igp, kgp, ilev);
          v1 += m_deriv(igp, kgp) * scalar(kgp, jgp, ilev);
        }
        v0 *= PhysicalConstants::rrearth;
        v1 *= PhysicalConstants::rrearth;
        grad_s(0,igp,jgp,ilev) = D_inv(0,0,igp,jgp) * v0 + D_inv(0,1,igp,jgp) * v1;
        grad_s(1,igp,jgp,ilev) = D_inv(1,0,igp,jgp) * v0 + D_inv(1,1,igp,jgp) * v1;
      });
    });
    kv.team_barrier();
  }

  template<int NUM_LEV_REQUEST = NUM_LEV>
  KOKKOS_INLINE_FUNCTION void
  gradient_sphere_update (const KernelVariables &kv,
                          const ElementGeometry& geo,
                          const ExecViewUnmanaged<const Scalar    [NP][NP][NUM_LEV]> scalar,
                          const ExecViewUnmanaged<      Scalar [2][NP][NP][NUM_LEV]> grad_s) const
  {
    static_assert(NUM_LEV_REQUEST>0, "Error! Template argument NUM_LEV_REQUEST must be positive.\n");

    const auto& D_inv = geo.m_dinv;
    constexpr int np_squared = NP * NP;
    Kokkos::parallel_for(Kokkos::TeamThreadRange(kv.team, np_squared),
                         [&](const int loop_idx) {
      const int igp = loop_idx / NP;
      const int jgp = loop_idx % NP;
      Kokkos::parallel_for(Kokkos::ThreadVectorRange(kv.team, NUM_LEV_REQUEST), [&] (const int& ilev) {
        Scalar dsdx, dsdy;
        for (int kgp = 0; kgp < NP; ++kgp) {
          dsdx += m_deriv(jgp, kgp) * scalar(igp, kgp, ilev);
          dsdy += m_deriv(igp, kgp) * scalar(kgp, jgp, ilev);
        }
        dsdx *= PhysicalConstants::rrearth;
        dsdy *= PhysicalConstants::rrearth;
        grad_s(0,igp,jgp,ilev) += D_inv(0,0,igp,jgp) * dsdx + D_inv(0,1,igp,jgp) * dsdy;
        grad_s(1,igp,jgp,ilev) += D_inv(1,0,igp,jgp) * dsdx + D_inv(1,1,igp,jgp) * dsdy;
      });
    });
    kv.team_barrier();
  }

  template<int NUM_LEV_REQUEST = NUM_LEV>
  KOKKOS_INLINE_FUNCTION void
  divergence_sphere (const KernelVariables &kv,
                     const ElementGeometry& geo,
                     const ExecViewUnmanaged<const Scalar [2][NP][NP][NUM_LEV]> v,
                     const ExecViewUnmanaged<      Scalar    [NP][NP][NUM_LEV]> div_v) const
  {
    static_assert(NUM_LEV_REQUEST>0, "Error! Template argument NUM_LEV_REQUEST must be positive.\n");

    const auto& D_inv  = geo.m_dinv;
    const auto& metdet = geo.m_metdet;
    const auto& bufs = m_buffers(kv.team_idx);
    const auto& gv_buf = Homme::subview(bufs.vector_buf_ml, 0);
    constexpr int np_squared = NP * NP;
    Kokkos::parallel_for(Kokkos::TeamThreadRange(kv.team, np_squared),
                         [&](const int loop_idx) {
      const int igp = loop_idx / NP;
      const int jgp = loop_idx % NP;
      Kokkos::parallel_for(Kokkos::ThreadVectorRange(kv.team, NUM_LEV_REQUEST), [&] (const int& ilev) {
        const auto& v0 = v(0, igp, jgp, ilev);
        const auto& v1 = v(1, igp, jgp, ilev);
        gv_buf(0,igp,jgp,ilev) = (D_inv(0,0,igp,jgp) * v0 + D_inv(1,0,igp,jgp) * v1) * metdet(igp,jgp);
        gv_buf(1,igp,jgp,ilev) = (D_inv(0,1,igp,jgp) * v0 + D_inv(1,1,igp,jgp) * v1) * metdet(igp,jgp);
      });
    });
    kv.team_barrier();

    // j, l, i -> i, j, k
    constexpr int div_iters = NP * NP;
    Kokkos::parallel_for(Kokkos::TeamThreadRange(kv.team, div_iters),
                         [&](const int loop_idx) {
      const int igp = loop_idx / NP;
      const int jgp = loop_idx % NP;
      Kokkos::parallel_for(Kokkos::ThreadVectorRange(kv.team, NUM_LEV_REQUEST), [&] (const int& ilev) {
        Scalar dudx, dvdy;
        for (int kgp = 0; kgp < NP; ++kgp) {
          dudx += m_deriv(jgp, kgp) * gv_buf(0, igp, kgp, ilev);
          dvdy += m_deriv(igp, kgp) * gv_buf(1, kgp, jgp, ilev);
        }
        div_v(igp, jgp, ilev) =
            (dudx + dvdy) * (1.0 / metdet(igp, jgp) * PhysicalConstants::rrearth);
      });
    });
    kv.team_barrier();
  }

  // Note: this updates the field div_v as follows:
  //     div_v = beta*div_v + alpha*div(v)
  template<int NUM_LEV_REQUEST = NUM_LEV>
  KOKKOS_INLINE_FUNCTION void
  divergence_sphere_update (const KernelVariables &kv,
                            const ElementGeometry& geo,
                            const Real alpha, const bool add_hyperviscosity,
                            const ExecViewUnmanaged<const Scalar [2][NP][NP][NUM_LEV]> vstar,
                            const ExecViewUnmanaged<const Scalar    [NP][NP][NUM_LEV]> qdp,
                            // On input, qtens_biharmonic if add_hyperviscosity, undefined
                            // if not; on output, qtens.
                            const ExecViewUnmanaged<      Scalar    [NP][NP][NUM_LEV]> qtens) const
  {
    static_assert(NUM_LEV_REQUEST>0, "Error! Template argument NUM_LEV_REQUEST must be positive.\n");

    const auto& D_inv  = geo.m_dinv;
    const auto& metdet = geo.m_metdet;
    const auto& bufs = m_buffers(kv.team_idx);
    const auto& gv = Homme::subview(bufs.vector_buf_ml,0);
    constexpr int np_squared = NP * NP;
    Kokkos::parallel_for(Kokkos::TeamThreadRange(kv.team, np_squared),
                         [&](const int loop_idx) {
      const int igp = loop_idx / NP;
      const int jgp = loop_idx % NP;
      Kokkos::parallel_for(Kokkos::ThreadVectorRange(kv.team, NUM_LEV_REQUEST), [&] (const int& ilev) {
        const auto& qdpijk = qdp(igp, jgp, ilev);
        const auto v0 = vstar(0, igp, jgp, ilev) * qdpijk;
        const auto v1 = vstar(1, igp, jgp, ilev) * qdpijk;
        gv(0,igp,jgp,ilev) = (D_inv(0,0,igp,jgp) * v0 + D_inv(1,0,igp,jgp) * v1) * metdet(igp,jgp);
        gv(1,igp,jgp,ilev) = (D_inv(0,1,igp,jgp) * v0 + D_inv(1,1,igp,jgp) * v1) * metdet(igp,jgp);
      });
    });
    kv.team_barrier();

    constexpr int div_iters = NP * NP;
    Kokkos::parallel_for(Kokkos::TeamThreadRange(kv.team, div_iters),
                         [&](const int loop_idx) {
      const int igp = loop_idx / NP;
      const int jgp = loop_idx % NP;
      Kokkos::parallel_for(Kokkos::ThreadVectorRange(kv.team, NUM_LEV_REQUEST), [&] (const int& ilev) {
        Scalar dudx, dvdy;
        for (int kgp = 0; kgp < NP; ++kgp) {
          dudx += m_deriv(jgp, kgp) * gv(0, igp, kgp, ilev);
          dvdy += m_deriv(igp, kgp) * gv(1, kgp, jgp, ilev);
        }
        const Scalar qtensijk0 = add_hyperviscosity ? qtens(igp,jgp,ilev) : 0;
        qtens(igp,jgp,ilev) = (qdp(igp,jgp,ilev) +
                               alpha*((dudx + dvdy) * (1.0 / metdet(igp,jgp) * PhysicalConstants::rrearth)) +
                               qtensijk0);
      });
    });
    kv.team_barrier();
  }

  template<int NUM_LEV_REQUEST = NUM_LEV>
  KOKKOS_INLINE_FUNCTION void
  vorticity_sphere (const KernelVariables &kv,
                    const ElementGeometry& geo,
                    const ExecViewUnmanaged<const Scalar [NP][NP][NUM_LEV]> u,
                    const ExecViewUnmanaged<const Scalar [NP][NP][NUM_LEV]> v,
                    const ExecViewUnmanaged<      Scalar [NP][NP][NUM_LEV]> vort) const
  {
    static_assert(NUM_LEV_REQUEST>0, "Error! Template argument NUM_LEV_REQUEST must be positive.\n");

    const auto& D      = geo.m_d;
    const auto& metdet = geo.m_metdet;
    const auto& bufs = m_buffers(kv.team_idx);
    const auto& vcov_buf = Homme::subview(bufs.vector_buf_ml,0);
    constexpr int np_squared = NP * NP;
    Kokkos::parallel_for(Kokkos::TeamThreadRange(kv.team, np_squared),
                         [&](const int loop_idx) {
      const int igp = loop_idx / NP;
      const int jgp = loop_idx % NP;
      Kokkos::parallel_for(Kokkos::ThreadVectorRange(kv.team, NUM_LEV_REQUEST), [&] (const int& ilev) {
        const auto& u_ijk = u(igp, jgp, ilev);
        const auto& v_ijk = v(igp, jgp, ilev);
        vcov_buf(0,igp,jgp,ilev) = D(0,0,igp,jgp) * u_ijk + D(0,1,igp,jgp) * v_ijk;
        vcov_buf(1,igp,jgp,ilev) = D(1,0,igp,jgp) * u_ijk + D(1,1,igp,jgp) * v_ijk;
      });
    });
    kv.team_barrier();

    constexpr int vort_iters = NP * NP;
    Kokkos::parallel_for(Kokkos::TeamThreadRange(kv.team, vort_iters),
                         [&](const int loop_idx) {
      const int igp = loop_idx / NP;
      const int jgp = loop_idx % NP;
      Kokkos::parallel_for(Kokkos::ThreadVectorRange(kv.team, NUM_LEV_REQUEST), [&] (const int& ilev) {
        Scalar dudy, dvdx;
        for (int kgp = 0; kgp < NP; ++kgp) {
          dvdx += m_deriv(jgp, kgp) * vcov_buf(1, igp, kgp, ilev);
          dudy += m_deriv(igp, kgp) * vcov_buf(0, kgp, jgp, ilev);
        }
        vort(igp, jgp, ilev) = (dvdx - dudy) * (1.0 / metdet(igp, jgp) *
                                                PhysicalConstants::rrearth);
      });
    });
    kv.team_barrier();
  }

  //Why does the prev version take u and v separately?
  //rewriting this to take vector as the input.
  template<int NUM_LEV_REQUEST = NUM_LEV>
  KOKKOS_INLINE_FUNCTION void
  vorticity_sphere (const KernelVariables &kv,
                    const ElementGeometry& geo,
                    const ExecViewUnmanaged<const Scalar [2][NP][NP][NUM_LEV]> v,
                    const ExecViewUnmanaged<      Scalar    [NP][NP][NUM_LEV]> vort) const
  {
    static_assert(NUM_LEV_REQUEST>0, "Error! Template argument NUM_LEV_REQUEST must be positive.\n");

    const auto& D      = geo.m_d;
    const auto& metdet = geo.m_metdet;
    const auto& bufs = m_buffers(kv.team_idx);
    const auto& sphere_buf = Homme::subview(bufs.vector_buf_ml,0);
    constexpr int np_squared = NP * NP;
    Kokkos::parallel_for(Kokkos::TeamThreadRange(kv.team, np_squared),
                         [&](const int loop_idx) {
      const int igp = loop_idx / NP;
      const int jgp = loop_idx % NP;
      Kokkos::parallel_for(Kokkos::ThreadVectorRange(kv.team, NUM_LEV_REQUEST), [&] (const int& ilev) {
        const auto& v0 = v(0,igp,jgp,ilev);
        const auto& v1 = v(1,igp,jgp,ilev);
        sphere_buf(0,igp,jgp,ilev) = D(0,0,igp,jgp) * v0 + D(0,1,igp,jgp) * v1;
        sphere_buf(1,igp,jgp,ilev) = D(1,0,igp,jgp) * v0 + D(1,1,igp,jgp) * v1;
      });
    });
    kv.team_barrier();

    constexpr int vort_iters = NP * NP;
    Kokkos::parallel_for(Kokkos::TeamThreadRange(kv.team, vort_iters),
                         [&](const int loop_idx) {
      const int igp = loop_idx / NP;
      const int jgp = loop_idx % NP;
      Kokkos::parallel_for(Kokkos::ThreadVectorRange(kv.team, NUM_LEV_REQUEST), [&] (const int& ilev) {
        Scalar dudy, dvdx;
        for (int kgp = 0; kgp < NP; ++kgp) {
          dvdx += m_deriv(jgp, kgp) * sphere_buf(1, igp, kgp, ilev);
          dudy += m_deriv(igp, kgp) * sphere_buf(0, kgp, jgp, ilev);
        }
        vort(igp, jgp, ilev) = (dvdx - dudy) * (1.0 / metdet(igp, jgp) *
                                                PhysicalConstants::rrearth);
      });
    });
    kv.team_barrier();
  }


  template<int NUM_LEV_REQUEST = NUM_LEV>
  KOKKOS_INLINE_FUNCTION void
  divergence_sphere_wk (const KernelVariables &kv,
                        const ElementGeometry& geo,
                        // On input, a field whose divergence is sought; on
                        // output, the view's data are invalid.
                        const ExecViewUnmanaged<Scalar [2][NP][NP][NUM_LEV]> v,
                        const ExecViewUnmanaged<Scalar    [NP][NP][NUM_LEV]> div_v) const
  {
    static_assert(NUM_LEV_REQUEST>0, "Error! Template argument NUM_LEV_REQUEST must be positive.\n");

    const auto& D_inv    = geo.m_dinv;
    const auto& spheremp = geo.m_spheremp;
    constexpr int np_squared = NP * NP;
    Kokkos::parallel_for(Kokkos::TeamThreadRange(kv.team, np_squared),
                         [&](const int loop_idx) {
      const int igp = loop_idx / NP;
      const int jgp = loop_idx % NP;
      Kokkos::parallel_for(Kokkos::ThreadVectorRange(kv.team, NUM_LEV_REQUEST), [&] (const int& ilev) {
        const auto v0 = v(0,igp,jgp,ilev);
        const auto v1 = v(1,igp,jgp,ilev);
        v(0,igp,jgp,ilev) = D_inv(0, 0, igp, jgp) * v0 + D_inv(1, 0, igp, jgp) * v1;
        v(1,igp,jgp,ilev) = D_inv(0, 1, igp, jgp) * v0 + D_inv(1, 1, igp, jgp) * v1;
      });
    });
    kv.team_barrier();

    constexpr int div_iters = NP * NP;
    Kokkos::parallel_for(Kokkos::TeamThreadRange(kv.team, div_iters),
                         [&](const int loop_idx) {
      // Note: for this one time, it is better if m strides faster, due to
      //       the way the views are accessed.
      const int mgp = loop_idx % NP;
      const int ngp = loop_idx / NP;
      Kokkos::parallel_for(Kokkos::ThreadVectorRange(kv.team, NUM_LEV_REQUEST), [&] (const int& ilev) {
        Scalar dd;
        // TODO: move multiplication by rrearth outside the loop
        for (int jgp = 0; jgp < NP; ++jgp) {
          // Here, v is the temporary buffer, aliased on the input v.
          dd -= (spheremp(ngp, jgp) * v(0, ngp, jgp, ilev) * m_deriv(jgp, mgp) +
                 spheremp(jgp, mgp) * v(1, jgp, mgp, ilev) * m_deriv(jgp, ngp)) *
                PhysicalConstants::rrearth;
        }
        div_v(ngp, mgp, ilev) = dd;
      });
    });
    kv.team_barrier();

  }//end of divergence_sphere_wk

  //analog of laplace_simple_c_callable
  template<int NUM_LEV_REQUEST = NUM_LEV>
  KOKKOS_INLINE_FUNCTION void
  laplace_simple (const KernelVariables &kv,
                  const ElementGeometry& geo,
                  const ExecViewUnmanaged<const Scalar [NP][NP][NUM_LEV]> field,
                  const ExecViewUnmanaged<      Scalar [NP][NP][NUM_LEV]> laplace) const
  {
    static_assert(NUM_LEV_REQUEST>0, "Error! Template argument NUM_LEV_REQUEST must be positive.\n");

    const auto& bufs = m_buffers(kv.team_idx);
    const auto& grad_s = Homme::subview(bufs.vector_buf_ml, 0);
    gradient_sphere<NUM_LEV_REQUEST>(kv, geo, field, grad_s);
    divergence_sphere_wk<NUM_LEV_REQUEST>(kv, geo, grad_s, laplace);
  }//end of laplace_simple

  //analog of laplace_wk_c_callable
  //but without if-statements for hypervis_power, var_coef, and hypervis_scaling.
  //for 2d fields, there should be either laplace_simple, or laplace_tensor for the whole run.
  template<int NUM_LEV_REQUEST = NUM_LEV>
  KOKKOS_INLINE_FUNCTION void
  laplace_tensor(const KernelVariables &kv,
                 const ElementGeometry& geo,
                 const ExecViewUnmanaged<const Real   [2][2][NP][NP]>          tensorVisc,
                 const ExecViewUnmanaged<const Scalar       [NP][NP][NUM_LEV]> field,         // input
                 const ExecViewUnmanaged<      Scalar       [NP][NP][NUM_LEV]> laplace) const
  {
    static_assert(NUM_LEV_REQUEST>0, "Error! Template argument NUM_LEV_REQUEST must be positive.\n");

    const auto& bufs = m_buffers(kv.team_idx);
    const auto& grad_s     = Homme::subview(bufs.vector_buf_ml, 1);
    const auto& sphere_buf = Homme::subview(bufs.vector_buf_ml, 2);

    gradient_sphere<NUM_LEV_REQUEST>(kv, geo, field, grad_s);
  //now multiply tensorVisc(:,:,i,j)*grad_s(i,j) (matrix*vector, independent of i,j )
  //but it requires a temp var to store a result. the result is then placed to grad_s,
  //or should it be an extra temp var instead of an extra loop?
    constexpr int num_iters = NP * NP;
    Kokkos::parallel_for(Kokkos::TeamThreadRange(kv.team, num_iters),
                       [&](const int loop_idx) {
      const int igp = loop_idx / NP;
      const int jgp = loop_idx % NP;
      Kokkos::parallel_for(Kokkos::ThreadVectorRange(kv.team, NUM_LEV_REQUEST), [&] (const int& ilev) {
        const auto& grad_s0 = grad_s(0,igp,jgp,ilev);
        const auto& grad_s1 = grad_s(1,igp,jgp,ilev);
        sphere_buf(0,igp,jgp,ilev) = tensorVisc(0,0,igp,jgp) * grad_s0 + tensorVisc(1,0,igp,jgp) * grad_s1;
        sphere_buf(1,igp,jgp,ilev) = tensorVisc(0,1,igp,jgp) * grad_s0 + tensorVisc(1,1,igp,jgp) * grad_s1;
      });
    });
    kv.team_barrier();

    divergence_sphere_wk<NUM_LEV_REQUEST>(kv, geo, sphere_buf, laplace);
  }//end of laplace_tensor

  template<int NUM_LEV_REQUEST = NUM_LEV>
  KOKKOS_INLINE_FUNCTION void
  curl_sphere_wk_testcov (const KernelVariables &kv,
                          const ElementGeometry& geo,
                          const ExecViewUnmanaged<const Scalar    [NP][NP][NUM_LEV]> scalar,
                          const ExecViewUnmanaged<      Scalar [2][NP][NP][NUM_LEV]> curls) const
  {
    static_assert(NUM_LEV_REQUEST>0, "Error! Template argument NUM_LEV_REQUEST must be positive.\n");

    const auto& D = geo.m_d;
    const auto& bufs = m_buffers(kv.team_idx);
    const auto& sphere_buf = Homme::subview(bufs.vector_buf_ml,0);
    constexpr int np_squared = NP * NP;
    Kokkos::parallel_for(Kokkos::TeamThreadRange(kv.team, np_squared), [&](const int loop_idx) {
      const int ngp = loop_idx / NP;
      const int mgp = loop_idx % NP;
      Kokkos::parallel_for(Kokkos::ThreadVectorRange(kv.team, NUM_LEV_REQUEST), [&] (const int& ilev) {
        auto& sb0 = sphere_buf(0, ngp, mgp, ilev);
        auto& sb1 = sphere_buf(1, ngp, mgp, ilev);
        sb0 = 0;
        sb1 = 0;
        for (int jgp = 0; jgp < NP; ++jgp) {
          sb0 -= m_mass(jgp,mgp)*scalar(jgp,mgp,ilev)*m_deriv(jgp,ngp);
          sb1 += m_mass(ngp,jgp)*scalar(ngp,jgp,ilev)*m_deriv(jgp,mgp);
        }
      });
    });
    kv.team_barrier();

    Kokkos::parallel_for(Kokkos::TeamThreadRange(kv.team, np_squared),
                         [&](const int loop_idx) {
      const int igp = loop_idx / NP; //slowest
      const int jgp = loop_idx % NP; //fastest
      Kokkos::parallel_for(Kokkos::ThreadVectorRange(kv.team, NUM_LEV_REQUEST), [&] (const int& ilev) {
        const auto& sb0 = sphere_buf(0, igp, jgp, ilev);
        const auto& sb1 = sphere_buf(1, igp, jgp, ilev);
        curls(0,igp,jgp,ilev) = (D(0,0,igp,jgp) * sb0 + D(1,0,igp,jgp) * sb1) * PhysicalConstants::rrearth;
        curls(1,igp,jgp,ilev) = (D(0,1,igp,jgp) * sb0 + D(1,1,igp,jgp) * sb1) * PhysicalConstants::rrearth;
      });
    });
    kv.team_barrier();
  }

  // This computes curls = alpha*curl(scalar) + beta*curls, where scalar is the input view,
  // and curls is the output view
  template<int NUM_LEV_REQUEST = NUM_LEV>
  KOKKOS_INLINE_FUNCTION void
  curl_sphere_wk_testcov_update (const KernelVariables &kv,
                                 const ElementGeometry& geo,
                                 const Real alpha, const Real beta,
                                 const ExecViewUnmanaged<const Scalar       [NP][NP][NUM_LEV]> scalar,
                                 const ExecViewUnmanaged<      Scalar    [2][NP][NP][NUM_LEV]> curls) const
  {
    static_assert(NUM_LEV_REQUEST>0, "Error! Template argument NUM_LEV_REQUEST must be positive.\n");

    const auto& D = geo.m_d;
    constexpr int np_squared = NP * NP;
    Kokkos::parallel_for(Kokkos::TeamThreadRange(kv.team, np_squared), [&](const int loop_idx) {
      const int ngp = loop_idx / NP;
      const int mgp = loop_idx % NP;
      Kokkos::parallel_for(Kokkos::ThreadVectorRange(kv.team, NUM_LEV_REQUEST), [&] (const int& ilev) {
        Scalar sb0, sb1;
        for (int jgp = 0; jgp < NP; ++jgp) {
          sb0 -= m_mass(jgp,mgp)*scalar(jgp,mgp,ilev)*m_deriv(jgp,ngp);
          sb1 += m_mass(ngp,jgp)*scalar(ngp,jgp,ilev)*m_deriv(jgp,mgp);
        }
        curls(0,ngp,mgp,ilev) = beta*curls(0,ngp,mgp,ilev) + alpha *
                                ( D(0,0,ngp,mgp)*sb0 + D(1,0,ngp,mgp)*sb1 )
                                * PhysicalConstants::rrearth;
        curls(1,ngp,mgp,ilev) = beta*curls(1,ngp,mgp,ilev) + alpha *
                                ( D(0,1,ngp,mgp)*sb0 + D(1,1,ngp,mgp)*sb1 )
                              * PhysicalConstants::rrearth;
      });
    });
    kv.team_barrier();
  }

  template<int NUM_LEV_REQUEST = NUM_LEV>
  KOKKOS_INLINE_FUNCTION void
  grad_sphere_wk_testcov (const KernelVariables &kv,
                          const ElementGeometry& geo,
                          const ExecViewUnmanaged<const Scalar    [NP][NP][NUM_LEV]> scalar,
                          const ExecViewUnmanaged<      Scalar [2][NP][NP][NUM_LEV]> grads) const
  {
    static_assert(NUM_LEV_REQUEST>0, "Error! Template argument NUM_LEV_REQUEST must be positive.\n");

    const auto& D      = geo.m_d;
    const auto& metinv = geo.m_metinv;
    const auto& metdet = geo.m_metdet;
    constexpr int np_squared = NP * NP;
    Kokkos::parallel_for(Kokkos::TeamThreadRange(kv.team, np_squared), [&](const int loop_idx) {
      const int ngp = loop_idx / NP;
      const int mgp = loop_idx % NP;
      Kokkos::parallel_for(Kokkos::ThreadVectorRange(kv.team, NUM_LEV_REQUEST), [&] (const int& ilev) {
        Scalar b0, b1;
        for (int jgp = 0; jgp < NP; ++jgp) {
          const auto& mpnj = m_mass(ngp,jgp);
          const auto& mpjm = m_mass(jgp,mgp);
          const auto& md = metdet(ngp,mgp);
          const auto& snj = scalar(ngp,jgp,ilev);
          const auto& sjm = scalar(jgp,mgp,ilev);
          const auto& djm = m_deriv(jgp,mgp);
          const auto& djn = m_deriv(jgp,ngp);
          b0 -= (mpnj * metinv(0,0,ngp,mgp) * md * snj * djm +
                 mpjm * metinv(0,1,ngp,mgp) * md * sjm * djn);
          b1 -= (mpnj * metinv(1,0,ngp,mgp) * md * snj * djm +
                 mpjm * metinv(1,1,ngp,mgp) * md * sjm * djn);
        }
        grads(0,ngp,mgp,ilev) = (D(0,0,ngp,mgp) * b0 + D(1,0,ngp,mgp) * b1) * PhysicalConstants::rrearth;
        grads(1,ngp,mgp,ilev) = (D(0,1,ngp,mgp) * b0 + D(1,1,ngp,mgp) * b1) * PhysicalConstants::rrearth;
      });
    });
    kv.team_barrier();
  }

  template<int NUM_LEV_REQUEST = NUM_LEV>
  KOKKOS_INLINE_FUNCTION void
  vlaplace_sphere_wk_cartesian (const KernelVariables &kv,
                                const ElementGeometry& geo,
                                const ExecViewUnmanaged<const Real   [2][2][NP][NP]>          tensorVisc,
                                const ExecViewUnmanaged<const Scalar    [2][NP][NP][NUM_LEV]> vector,
                                const ExecViewUnmanaged<      Scalar    [2][NP][NP][NUM_LEV]> laplace) const
  {
    static_assert(NUM_LEV_REQUEST>0, "Error! Template argument NUM_LEV_REQUEST must be positive.\n");

    const auto& spheremp = geo.m_spheremp;
    const auto& vec_sph2cart = geo.m_vec_sph2cart;
    const auto& bufs = m_buffers(kv.team_idx);
    const auto& laplace0 = Homme::subview(bufs.scalar_buf_ml,0);
    const auto& laplace1 = Homme::subview(bufs.scalar_buf_ml,1);
    const auto& laplace2 = Homme::subview(bufs.scalar_buf_ml,2);
    constexpr int np_squared = NP * NP;
    Kokkos::parallel_for(Kokkos::TeamThreadRange(kv.team, np_squared),
                         [&](const int loop_idx) {
      const int igp = loop_idx / NP; //slowest
      const int jgp = loop_idx % NP; //fastest
      Kokkos::parallel_for(Kokkos::ThreadVectorRange(kv.team, NUM_LEV_REQUEST), [&] (const int& ilev) {
        const auto& v0 = vector(0,igp,jgp,ilev);
        const auto& v1 = vector(1,igp,jgp,ilev);
        laplace0(igp,jgp,ilev) = vec_sph2cart(0,0,igp,jgp)*v0 + vec_sph2cart(1,0,igp,jgp)*v1 ;
        laplace1(igp,jgp,ilev) = vec_sph2cart(0,1,igp,jgp)*v0 + vec_sph2cart(1,1,igp,jgp)*v1 ;
        laplace2(igp,jgp,ilev) = vec_sph2cart(0,2,igp,jgp)*v0 + vec_sph2cart(1,2,igp,jgp)*v1 ;
      });
    });
    kv.team_barrier();

    // Use laplace* as input, and then overwrite it with the output (saves temporaries)
    laplace_tensor<NUM_LEV_REQUEST>(kv,geo,tensorVisc,laplace0,laplace0);
    laplace_tensor<NUM_LEV_REQUEST>(kv,geo,tensorVisc,laplace1,laplace1);
    laplace_tensor<NUM_LEV_REQUEST>(kv,geo,tensorVisc,laplace2,laplace2);

    Kokkos::parallel_for(Kokkos::TeamThreadRange(kv.team, np_squared),
                         [&](const int loop_idx) {
      const int igp = loop_idx / NP; //slowest
      const int jgp = loop_idx % NP; //fastest
      Kokkos::parallel_for(Kokkos::ThreadVectorRange(kv.team, NUM_LEV_REQUEST), [&] (const int& ilev) {
#define UNDAMPRRCART
#ifdef UNDAMPRRCART
        laplace(0,igp,jgp,ilev) = vec_sph2cart(0,0,igp,jgp)*laplace0(igp,jgp,ilev)
                                + vec_sph2cart(0,1,igp,jgp)*laplace1(igp,jgp,ilev)
                                + vec_sph2cart(0,2,igp,jgp)*laplace2(igp,jgp,ilev)
                                + 2.0*spheremp(igp,jgp)*vector(0,igp,jgp,ilev)
                                        *(PhysicalConstants::rrearth)*(PhysicalConstants::rrearth);

        laplace(1,igp,jgp,ilev) = vec_sph2cart(1,0,igp,jgp)*laplace0(igp,jgp,ilev)
                                + vec_sph2cart(1,1,igp,jgp)*laplace1(igp,jgp,ilev)
                                + vec_sph2cart(1,2,igp,jgp)*laplace2(igp,jgp,ilev)
                                + 2.0*spheremp(igp,jgp)*vector(1,igp,jgp,ilev)
                                        *(PhysicalConstants::rrearth)*(PhysicalConstants::rrearth);
#else
        laplace(0,igp,jgp,ilev) = vec_sph2cart(0,0,igp,jgp)*laplace0(igp,jgp,ilev)
                                + vec_sph2cart(0,1,igp,jgp)*laplace1(igp,jgp,ilev)
                                + vec_sph2cart(0,2,igp,jgp)*laplace2(igp,jgp,ilev);
        laplace(1,igp,jgp,ilev) = vec_sph2cart(1,0,igp,jgp)*laplace0(igp,jgp,ilev)
                                + vec_sph2cart(1,1,igp,jgp)*laplace1(igp,jgp,ilev)
                                + vec_sph2cart(1,2,igp,jgp)*laplace2(igp,jgp,ilev);
#endif
      });
    });
    kv.team_barrier();
  } // end of vlaplace_sphere_wk_cartesian

  template<int NUM_LEV_REQUEST = NUM_LEV>
  KOKKOS_INLINE_FUNCTION void
  vlaplace_sphere_wk_contra (const KernelVariables &kv,
                             const ElementGeometry& geo,
                             const Real nu_ratio,
                             const ExecViewUnmanaged<const Scalar [2][NP][NP][NUM_LEV]> vector,
                             const ExecViewUnmanaged<      Scalar [2][NP][NP][NUM_LEV]> laplace) const
  {
    static_assert(NUM_LEV_REQUEST>0, "Error! Template argument NUM_LEV_REQUEST must be positive.\n");

    const auto& spheremp = geo.m_spheremp;

    const auto& bufs = m_buffers(kv.team_idx);
    const auto& div           = Homme::subview(bufs.scalar_buf_ml,0);
    const auto& vort          = Homme::subview(bufs.scalar_buf_ml,0);
    const auto& grad_curl_cov = Homme::subview(bufs.vector_buf_ml,1);
    constexpr int np_squared = NP * NP;

    // grad(div(v))
    divergence_sphere<NUM_LEV_REQUEST>(kv,geo,vector,div);
    if (nu_ratio>0 && nu_ratio!=1.0) {
      Kokkos::parallel_for(Kokkos::TeamThreadRange(kv.team, np_squared),
                          [&](const int loop_idx) {
        const int igp = loop_idx / NP; //slow
        const int jgp = loop_idx % NP; //fast
        Kokkos::parallel_for(Kokkos::ThreadVectorRange(kv.team, NUM_LEV_REQUEST), [&] (const int& ilev) {
          div(igp,jgp,ilev) *= nu_ratio;
        });
      });
      kv.team_barrier();
    }
    grad_sphere_wk_testcov<NUM_LEV_REQUEST>(kv,geo,div,grad_curl_cov);

    // curl(curl(v))
    vorticity_sphere<NUM_LEV_REQUEST>(kv,geo,vector,vort);
    curl_sphere_wk_testcov_update<NUM_LEV_REQUEST>(kv,geo,-1.0,1.0,vort,grad_curl_cov);

    const auto re2 = PhysicalConstants::rrearth*PhysicalConstants::rrearth;
    Kokkos::parallel_for(Kokkos::TeamThreadRange(kv.team, np_squared),
                        [&](const int loop_idx) {
      const int igp = loop_idx / NP; //slow
      const int jgp = loop_idx % NP; //fast
      Kokkos::parallel_for(Kokkos::ThreadVectorRange(kv.team, NUM_LEV_REQUEST), [&] (const int& ilev) {
        const auto f = 2.0*spheremp(igp,jgp);
        laplace(0,igp,jgp,ilev) = f*vector(0,igp,jgp,ilev)*re2 + grad_curl_cov(0,igp,jgp,ilev);
        laplace(1,igp,jgp,ilev) = f*vector(1,igp,jgp,ilev)*re2 + grad_curl_cov(1,igp,jgp,ilev);
      });
     });
     kv.team_barrier();
  }//end of vlaplace_sphere_wk_contra

private:
  static constexpr int NUM_2D_VECTOR_BUFFERS = 2;
  static constexpr int NUM_3D_SCALAR_BUFFERS = 3;
  static constexpr int NUM_3D_VECTOR_BUFFERS = 3;

  // These buffers should be enough to handle any single call to any
  // single sphere operator.
  //   One might prefer them to be private, but they are handy for
  // users of SphereOperators for other things, and using the same
  // memory buffers for multiple things gives performance in mem
  // b/w-limited computations.

  struct SphereOpBuffers {
    ExecViewUnmanaged<Real   [NUM_2D_VECTOR_BUFFERS][2][NP][NP]>           vector_buf_sl;
    ExecViewUnmanaged<Scalar [NUM_3D_SCALAR_BUFFERS][NP][NP][NUM_LEV]>     scalar_buf_ml;
    ExecViewUnmanaged<Scalar [NUM_3D_VECTOR_BUFFERS][2][NP][NP][NUM_LEV]>  vector_buf_ml;

    static int size () {
       return NUM_2D_VECTOR_BUFFERS*2*NP*NP +
              NUM_3D_SCALAR_BUFFERS*2*NP*NP*NUM_LEV*VECTOR_SIZE +
              NUM_3D_VECTOR_BUFFERS*2*NP*NP*NUM_LEV*VECTOR_SIZE;
    }
  };

  ExecViewManaged<Real*>            m_buffers_storage;
  ExecViewManaged<SphereOpBuffers*> m_buffers;

  ExecViewManaged<const Real [NP][NP]>      m_deriv;
  ExecViewManaged<const Real [NP][NP]>      m_mass;

  bool m_inited;
};

} // namespace Homme

#endif // HOMMEXX_SPHERE_OPERATORS_HPP
