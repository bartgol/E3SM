/********************************************************************************
 * HOMMEXX 1.0: Copyright of Sandia Corporation
 * This software is released under the BSD license
 * See the file 'COPYRIGHT' in the HOMMEXX/src/share/cxx directory
 *******************************************************************************/

#ifndef HOMMEXX_ELEMENT_GEOMETRY_HPP
#define HOMMEXX_ELEMENT_GEOMETRY_HPP

#include "Types.hpp"
#include "utilities/StaticArray.hpp"

namespace Homme {

/* Per element data - specific velocity, temperature, pressure, etc. */
class ElementGeometry {
public:
  // Coriolis force
  ExecViewUnmanaged<Real [NP][NP]>        m_fcor;

  // Quadrature weights and metric tensor
  ExecViewUnmanaged<Real [NP][NP]>        m_spheremp;
  ExecViewUnmanaged<Real [NP][NP]>        m_rspheremp;
  ExecViewUnmanaged<Real [NP][NP]>        m_metdet;
  ExecViewUnmanaged<Real [2][2][NP][NP]>  m_metinv;
  ExecViewUnmanaged<Real [2][3][NP][NP]>  m_vec_sph2cart;

  // D (map for covariant coordinates) and D^{-1}
  ExecViewUnmanaged<Real [2][2][NP][NP]>  m_d;
  ExecViewUnmanaged<Real [2][2][NP][NP]>  m_dinv;

  ElementGeometry () = default;

  void view_memory  (const bool consthv, Real* const memory);
  void alloc_memory (const bool consthv);

  // Fill the exec space views with data coming from F90 pointers
  void fill (CF90Ptr &D, CF90Ptr &Dinv, CF90Ptr &fcor,
             CF90Ptr &spheremp, CF90Ptr &rspheremp,
             CF90Ptr &metdet, CF90Ptr &metinv, 
             CF90Ptr &vec_sph2cart);

  // If allocate is true, we allocate our own storage, otherwise view_memory must have been called
  void random_init (const bool consthv, const bool allocate);

  int size () const { return 4*(NP*NP) + 3*(2*2*NP*NP) + (m_consthv ? 0 : 1*(2*3*NP*NP)); }

private:
  // We use this for unit tests, so we don't need track memory from outside
  ExecViewManaged<Real*>  m_storage;

  bool m_consthv;
};

} // Homme

#endif // HOMMEXX_ELEMENT_GEOMETRY_HPP
