/********************************************************************************
 * HOMMEXX 1.0: Copyright of Sandia Corporation
 * This software is released under the BSD license
 * See the file 'COPYRIGHT' in the HOMMEXX/src/share/cxx directory
 *******************************************************************************/

#include "ElementGeometry.hpp"

#include "ErrorDefs.hpp"

#include "utilities/TestUtils.hpp"

#include <random>

namespace Homme {

void ElementGeometry::view_memory (const bool consthv, Real* const memory) {
  m_consthv = consthv;

  int offset = 0;

  // Scalars
  m_fcor      = ExecViewUnmanaged<Real [NP][NP]>(memory);                   offset += NP*NP;
  m_spheremp  = ExecViewUnmanaged<Real [NP][NP]>(memory+offset);            offset += NP*NP;
  m_rspheremp = ExecViewUnmanaged<Real [NP][NP]>(memory+offset);            offset += NP*NP;
  m_metdet    = ExecViewUnmanaged<Real [NP][NP]>(memory+offset);            offset += NP*NP;

  // 2x2 tensors
  m_metinv = ExecViewUnmanaged<Real [2][2][NP][NP]>(memory+offset);         offset += 2*2*NP*NP;
  m_d      = ExecViewUnmanaged<Real [2][2][NP][NP]>(memory+offset);         offset += 2*2*NP*NP;
  m_dinv   = ExecViewUnmanaged<Real [2][2][NP][NP]>(memory+offset);         offset += 2*2*NP*NP;
  
  if (!m_consthv) {
    // 2x3 tensor
    m_vec_sph2cart = ExecViewUnmanaged<Real [2][3][NP][NP]>(memory+offset);   offset += 2*3*NP*NP;
  }

  Errors::runtime_check (offset==size(), "Error! Something went wrong while distributing memory in ElementGeometry.\n", -1);
}

void ElementGeometry::alloc_memory (const bool consthv) {
  m_consthv = consthv;

  m_storage = ExecViewManaged<Real*>("",size());

  view_memory (m_consthv, m_storage.data());
}

void ElementGeometry::fill (CF90Ptr &D, CF90Ptr &Dinv, CF90Ptr &fcor,
                            CF90Ptr &spheremp, CF90Ptr &rspheremp,
                            CF90Ptr &metdet, CF90Ptr &metinv,
                            CF90Ptr &vec_sph2cart) {

  using ScalarView   = ExecViewUnmanaged<Real [NP][NP]>;
  using TensorView   = ExecViewUnmanaged<Real [2][2][NP][NP]>;
  using Tensor23View = ExecViewUnmanaged<Real [2][3][NP][NP]>;

  using ScalarViewF90   = HostViewUnmanaged<const Real [NP][NP]>;
  using TensorViewF90   = HostViewUnmanaged<const Real [2][2][NP][NP]>;
  using Tensor23ViewF90 = HostViewUnmanaged<const Real [2][3][NP][NP]>;

  ScalarView::HostMirror h_fcor      = Kokkos::create_mirror_view(m_fcor);
  ScalarView::HostMirror h_metdet    = Kokkos::create_mirror_view(m_metdet);
  ScalarView::HostMirror h_spheremp  = Kokkos::create_mirror_view(m_spheremp);
  ScalarView::HostMirror h_rspheremp = Kokkos::create_mirror_view(m_rspheremp);
  TensorView::HostMirror h_metinv    = Kokkos::create_mirror_view(m_metinv);
  TensorView::HostMirror h_d         = Kokkos::create_mirror_view(m_d);
  TensorView::HostMirror h_dinv      = Kokkos::create_mirror_view(m_dinv);

  Tensor23View::HostMirror h_vec_sph2cart;
  if( !m_consthv ){
    h_vec_sph2cart = Kokkos::create_mirror_view(m_vec_sph2cart);
  }

  ScalarViewF90 h_fcor_f90         (fcor);
  ScalarViewF90 h_metdet_f90       (metdet);
  ScalarViewF90 h_spheremp_f90     (spheremp);
  ScalarViewF90 h_rspheremp_f90    (rspheremp);
  TensorViewF90 h_metinv_f90       (metinv);
  TensorViewF90 h_d_f90            (D);
  TensorViewF90 h_dinv_f90         (Dinv);
  Tensor23ViewF90 h_vec_sph2cart_f90 (vec_sph2cart);
  
  // Scalars
  for (int igp = 0; igp < NP; ++igp) {
    for (int jgp = 0; jgp < NP; ++jgp) {
      h_fcor      (igp, jgp) = h_fcor_f90      (igp,jgp);
      h_spheremp  (igp, jgp) = h_spheremp_f90  (igp,jgp);
      h_rspheremp (igp, jgp) = h_rspheremp_f90 (igp,jgp);
      h_metdet    (igp, jgp) = h_metdet_f90    (igp,jgp);
    }
  }

  // 2x2 tensors
  for (int idim = 0; idim < 2; ++idim) {
    for (int jdim = 0; jdim < 2; ++jdim) {
      for (int igp = 0; igp < NP; ++igp) {
        for (int jgp = 0; jgp < NP; ++jgp) {
          h_d      (idim,jdim,igp,jgp) = h_d_f90      (idim,jdim,igp,jgp);
          h_dinv   (idim,jdim,igp,jgp) = h_dinv_f90   (idim,jdim,igp,jgp);
          h_metinv (idim,jdim,igp,jgp) = h_metinv_f90 (idim,jdim,igp,jgp);
        }
      }
    }
  }
  
  if(!m_consthv) {
    for (int idim = 0; idim < 2; ++idim) {
      for (int jdim = 0; jdim < 3; ++jdim) {
        for (int igp = 0; igp < NP; ++igp) {
          for (int jgp = 0; jgp < NP; ++jgp) {
            h_vec_sph2cart (idim,jdim,igp,jgp) = h_vec_sph2cart_f90 (idim,jdim,igp,jgp);
          }
        }
      }
    }
  }

  Kokkos::deep_copy(m_fcor, h_fcor);
  Kokkos::deep_copy(m_metinv, h_metinv);
  Kokkos::deep_copy(m_metdet, h_metdet);
  Kokkos::deep_copy(m_spheremp, h_spheremp);
  Kokkos::deep_copy(m_rspheremp, h_rspheremp);
  Kokkos::deep_copy(m_d, h_d);
  Kokkos::deep_copy(m_dinv, h_dinv);
  if( !m_consthv ) {
    Kokkos::deep_copy(m_vec_sph2cart, h_vec_sph2cart);
  }
}


void ElementGeometry::random_init(const bool consthv, const bool allocate) {
  if (allocate) {
    alloc_memory (consthv);
  }

  // Check that we set the memory already (any view would do)
  Errors::runtime_check(m_d.data()!=nullptr, "Error! Set the memory first.\n", -1);

  // arbitrary minimum value to generate and minimum determinant allowed
  constexpr const Real min_value = 0.015625;
  std::random_device rd;
  std::mt19937_64 engine(rd());
  std::uniform_real_distribution<Real> random_dist(min_value, 1.0 / min_value);

  genRandArray(m_fcor,      engine, random_dist);
  genRandArray(m_spheremp,  engine, random_dist);
  genRandArray(m_rspheremp, engine, random_dist);
  genRandArray(m_metdet,    engine, random_dist);
  genRandArray(m_metinv,    engine, random_dist);

  if (!m_consthv) {
    // Generate vec_sphere2cart
    genRandArray(m_vec_sph2cart, engine, random_dist);
  }

  // Small lambda to compute a 2x2 tensor determinant
  const auto compute_det = [](HostViewManaged<Real[2][2]> mtx)->Real {
    return mtx(0, 0) * mtx(1, 1) - mtx(0, 1) * mtx(1, 0);
  };

  HostViewManaged<Real[2][2]> h_matrix("");
  auto h_d    = Kokkos::create_mirror_view(m_d);
  auto h_dinv = Kokkos::create_mirror_view(m_dinv);

  // Because this constraint is difficult to satisfy for all of the tensors,
  // incrementally generate the view
  for (int igp = 0; igp < NP; ++igp) {
    for (int jgp = 0; jgp < NP; ++jgp) {
      // Try to generate a 2x2 tensor
      do {
        genRandArray(h_matrix, engine, random_dist);
      } while (compute_det(h_matrix)<=0);

      // the 2x2 tensor has positive determinant! Proceed.
      for (int i = 0; i < 2; ++i) {
        for (int j = 0; j < 2; ++j) {
          h_d(i, j, igp, jgp) = h_matrix(i, j);
        }
      }
      const Real determinant = compute_det(h_matrix);
      h_dinv(0, 0, igp, jgp) =  h_matrix(1, 1) / determinant;
      h_dinv(1, 0, igp, jgp) = -h_matrix(1, 0) / determinant;
      h_dinv(0, 1, igp, jgp) = -h_matrix(0, 1) / determinant;
      h_dinv(1, 1, igp, jgp) =  h_matrix(0, 0) / determinant;
    }
  }
  Kokkos::deep_copy(m_d,   h_d);
  Kokkos::deep_copy(m_dinv,h_dinv);
}

} // namespace Homme
