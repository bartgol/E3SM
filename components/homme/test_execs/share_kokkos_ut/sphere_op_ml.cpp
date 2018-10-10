#include <catch2/catch.hpp>

#include <limits>

#include "ElementGeometry.hpp"
#include "ReferenceElement.hpp"
#include "Dimensions.hpp"
#include "KernelVariables.hpp"
#include "SphereOperators.hpp"
#include "Types.hpp"
#include "utilities/GenericUtils.hpp"
#include "utilities/TestUtils.hpp"
#include "utilities/SubviewUtils.hpp"

#include <assert.h>
#include <stdio.h>
#include <random>

using namespace Homme;

using rngAlg = std::mt19937_64;

extern "C" {

// inserting names of params for _c_callable functions.
// instead of using names as in function descriptions, using
// verbose names
void laplace_simple_c_callable(const Real *input,
                               const Real *dvv,
                               const Real *dinv,
                               const Real *metdet,
                               Real *output);

void gradient_sphere_c_callable(const Real *input,
                                const Real *dvv,
                                const Real *dinv,
                                Real *output);

void divergence_sphere_c_callable(const Real *input,
                                  const Real *dvv,
                                  const Real *metdet,
                                  const Real *dinv,
                                  Real *output);

void divergence_sphere_wk_c_callable(const Real *input,
                                     const Real *dvv,
                                     const Real *spheremp,
                                     const Real *dinv,
                                     Real *output);

// This is an interface for the most general 2d laplace in
// F. In c++ we will ignore 'var coef' HV. If one uses
//var_coef in below, then both 'var_coef' and 'tensor' HV
// are possible. the switch between them is whether hvpower
// is <>0. To exclude 'var coef' HV we will always set
//hvpower to 0. If hvscaling is <>0, then tensor is used.
// So, settings for usual HV are var_coef=FALSE.
// Settings for tensor HV are var_coef=TRUE, hvpower=0,
// hvscaling >0. (don't ask me why this is so).  Update,
// copypasting from stackoverflow:  Typical Fortran
// implementations pass all arguments by reference,  while in
// C(++) the default is by value. This fixes an issue with
// hvpower, etc.
void laplace_sphere_wk_c_callable(
    const Real *input, const Real *dvv, const Real *dinv,
    const Real *spheremp, const Real *tensorVisc,
    const Real &hvpower,    // should be set to 0 always
    const Real &hvscaling,  // should be set to !=0 value
    const bool
        var_coef,  // should be set to 1 for tensor HV
    Real *output);

void curl_sphere_wk_testcov_c_callable(
    const Real *input,  // s(np,np)
    const Real *dvv,    // dvv(np, np)
    const Real *D,      // D(np, np, 2, 2)
    const Real *mp,     // mp(np, np)
    Real *output);      // ds(np,np,2)

void gradient_sphere_wk_testcov_c_callable(
    const Real *input,   // s(np,np)
    const Real *dvv,     // dvv(np, np)
    const Real *metinv,  // metinv(np, np, 2, 2)
    const Real *metdet,  // metdet(np, np)
    const Real *D,       // D(np, np, 2, 2)
    const Real *mp,      // mp(np, np)
    Real *output);       // ds(np,np,2)

void vlaplace_sphere_wk_cartesian_c_callable(
    const Real *input, const Real *dvv, const Real *dinv,
    const Real *spheremp, const Real *tensorVisc,
    const Real *vec_sph2cart, const Real &hvpower,
    const Real &hvscaling, const bool var_coef,
    Real *output);

void vlaplace_sphere_wk_contra_c_callable(
    const Real *input, const Real *dvv, const Real *d,
    const Real *dinv, const Real *mp, const Real *spheremp,
    const Real *metinv, const Real *metdet,
    const Real *rmetdet, const Real &nu_ratio,
    Real *output);

void vorticity_sphere_c_callable(
    const Real *input, const Real *dvv,
    const Real *metdet, const Real *d,
    Real *output);

}  // extern C

class compute_sphere_operator_test_ml {
 public:

  compute_sphere_operator_test_ml(int num_elems)
      : _num_elems(num_elems),
        scalar_input_d("scalar input", num_elems),
        scalar_input_COPY2_d("scalar input 2", num_elems),
        vector_input_d("vector input", num_elems),
        scalar_output_d("scalar output", num_elems),
        vector_output_d("vector output", num_elems),
        tensorVisc_d("tensor", num_elems),

        // Make certain Kokkos doesn't use the same arrays
        // as used on the device when they are mutable
        scalar_input_host("scalar input host", num_elems),
        scalar_input_COPY2_host("scalar input host copy", num_elems),
        vector_input_host("vector input host", num_elems),
        scalar_output_host(
            Kokkos::create_mirror_view(scalar_output_d)),
        vector_output_host(
            Kokkos::create_mirror_view(vector_output_d)),
        tensorVisc_host(
            Kokkos::create_mirror_view(tensorVisc_d))
  {
    std::random_device rd;
    rngAlg engine(rd());

    // Although scalar_output_d is an output, it can be used as input by divergence_sphere_update
    genRandArray(
        scalar_output_host, engine,
        std::uniform_real_distribution<Real>(-1000.0,
                                             1000.0));
    Kokkos::deep_copy(scalar_output_host, scalar_output_d);

    genRandArray(
        scalar_input_host, engine,
        std::uniform_real_distribution<Real>(-1000.0,
                                             1000.0));
    Kokkos::deep_copy(scalar_input_d, scalar_input_host);
    Kokkos::deep_copy(scalar_input_COPY2_d, scalar_input_host);
    Kokkos::deep_copy(scalar_input_COPY2_host, scalar_input_host);

    genRandArray(
        vector_input_host, engine,
        std::uniform_real_distribution<Real>(-1000.0,
                                             1000.0));
    Kokkos::deep_copy(vector_input_d, vector_input_host);

    genRandArray(&nu_ratio, 1, engine,
                 std::uniform_real_distribution<Real>(
                     0.0001, 1000.0));

    genRandArray(&alpha, 1, engine,
                 std::uniform_real_distribution<Real>(
                     -30.0, 30.0));

    genRandArray(&add_hyperviscosity, 1, engine,
                 std::uniform_int_distribution<int>(0,1));

    ref_FE.random_init();

    geometry = ExecViewManaged<ElementGeometry *>("",num_elems);
    auto geometry_host = Kokkos::create_mirror_view(geometry);
    for (int ie=0; ie<num_elems; ++ie) {
      geometry_host(ie).random_init(false,true);
    }
    Kokkos::deep_copy(geometry,geometry_host);
    sphere_ops.init(ref_FE);
  }  // end of constructor

  int _num_elems;  // league size, serves as ie index

  // device
  ExecViewManaged<Scalar * [NP][NP][NUM_LEV]>
      scalar_input_d;
  ExecViewManaged<Scalar * [NP][NP][NUM_LEV]>
      scalar_input_COPY2_d;
  ExecViewManaged<Scalar * [2][NP][NP][NUM_LEV]>
      vector_input_d;
  ExecViewManaged<Scalar * [NP][NP][NUM_LEV]>
      scalar_output_d;
  ExecViewManaged<Scalar * [2][NP][NP][NUM_LEV]>
      vector_output_d;
  ExecViewManaged<Real * [2][2][NP][NP]>
      tensorVisc_d;
  // host
  // rely on fact NUM_PHYSICAL_LEV=NUM_LEV*VECTOR_SIZE
  ExecViewManaged<Scalar * [NP][NP][NUM_LEV]>::HostMirror
      scalar_input_host,
      scalar_input_COPY2_host;
  const int scalar_input_len =
      NUM_PHYSICAL_LEV * NP * NP;  // temp code

  ExecViewManaged<Scalar * [2][NP][NP][NUM_LEV]>::HostMirror
      vector_input_host;
  const int vector_input_len =
      NUM_PHYSICAL_LEV * 2 * NP * NP;

  ExecViewManaged<Scalar * [NP][NP][NUM_LEV]>::HostMirror
      scalar_output_host;
  ExecViewManaged<Scalar * [2][NP][NP][NUM_LEV]>::HostMirror
      vector_output_host;
  ExecViewManaged<Real * [2][2][NP][NP]>::HostMirror
      tensorVisc_host;

  ReferenceElement ref_FE;
  ExecViewManaged<ElementGeometry *> geometry;
  SphereOperators  sphere_ops;

  Real nu_ratio;
  Real alpha;
  int add_hyperviscosity;

  // tag for laplace_simple()
  struct TagSimpleLaplaceML {};
  // tag for gradient_sphere()
  struct TagGradientSphereML {};
  // tag for divergence_sphere_wk
  struct TagDivergenceSphereWkML {};
  // tag for divergence_sphere
  struct TagDivergenceSphereML {};
  // tag for divergence_sphere_update
  struct TagDivergenceSphereUpdateML {};
  // tag for laplace_tensor
  struct TagTensorLaplaceML {};
  // tag for laplace_tensor
  struct TagTensorLaplaceReplaceML {};
  // tag for curl_sphere_wk_testcov
  struct TagCurlSphereWkTestCovML {};
  // tag for grad_sphere_wk_testcov
  struct TagGradSphereWkTestCovML {};
  // tag for vlaplace_sphere_wk_cartesian
  struct TagVLaplaceCartesianML {};
  // tag for vlaplace_sphere_wk_contra
  struct TagVLaplaceContraML {};
  // tag for vorticity_sphere
  struct TagVorticityVectorML {};
  // tag for default, a dummy
  struct TagDefault {};

  KOKKOS_INLINE_FUNCTION
  void operator()(const TagDefault &,
                  const TeamMember& team) const {
      // do nothing or print a message
  };

  KOKKOS_INLINE_FUNCTION
  void operator()(const TagGradientSphereML &,
                  const TeamMember& team) const {
    KernelVariables kv(team);

    sphere_ops.gradient_sphere(kv,
                    geometry(kv.ie),
                    Homme::subview(scalar_input_d, kv.ie),
                    Homme::subview(vector_output_d,kv.ie));

  }  // end of op() for grad_sphere_ml

  KOKKOS_INLINE_FUNCTION
  void operator()(const TagDivergenceSphereWkML &,
                  const TeamMember& team) const {
    KernelVariables kv(team);

    sphere_ops.divergence_sphere_wk(kv,
                         geometry(kv.ie),
                         Homme::subview(vector_input_d, kv.ie),
                         Homme::subview(scalar_output_d,kv.ie));
  }  // end of op() for divergence_sphere_wk_ml

  KOKKOS_INLINE_FUNCTION
  void operator()(const TagDivergenceSphereML &,
                  const TeamMember& team) const {
    KernelVariables kv(team);

    sphere_ops.divergence_sphere(kv,
                         geometry(kv.ie),
                         Homme::subview(vector_input_d, kv.ie),
                         Homme::subview(scalar_output_d,kv.ie));
  }  // end of op() for divergence_sphere_update

  KOKKOS_INLINE_FUNCTION
  void operator()(const TagDivergenceSphereUpdateML &,
                  const TeamMember& team) const {
    KernelVariables kv(team);

    sphere_ops.divergence_sphere_update(kv,
                         geometry(kv.ie),
                         alpha,add_hyperviscosity!=0,
                         Homme::subview(vector_input_d, kv.ie),
                         Homme::subview(scalar_input_d, kv.ie),
                         Homme::subview(scalar_output_d,kv.ie));
  }  // end of op() for divergence_sphere_update

  KOKKOS_INLINE_FUNCTION
  void operator()(const TagSimpleLaplaceML &,
                  const TeamMember& team) const {
    KernelVariables kv(team);

    sphere_ops.laplace_simple(kv,
                   geometry(kv.ie),
                   Homme::subview(scalar_input_d, kv.ie),
                   Homme::subview(scalar_output_d,kv.ie));
  }  // end of op() for laplace_wk_ml

  KOKKOS_INLINE_FUNCTION
  void operator()(const TagTensorLaplaceML &,
                  const TeamMember& team) const {
    KernelVariables kv(team);

    sphere_ops.laplace_tensor(kv,
                   geometry(kv.ie),
                   Homme::subview(tensorVisc_d,kv.ie),
                   Homme::subview(scalar_input_d, kv.ie),
                   Homme::subview(scalar_output_d,kv.ie));
  }  // end of op() for laplace_tensor multil

  KOKKOS_INLINE_FUNCTION
  void operator()(const TagCurlSphereWkTestCovML &,
                  const TeamMember& team) const {
    KernelVariables kv(team);

    sphere_ops.curl_sphere_wk_testcov(kv,
                           geometry(kv.ie),
                           Homme::subview(scalar_input_d, kv.ie),
                           Homme::subview(vector_output_d,kv.ie));
  }  // end of op() for curl_sphere_wk_testcov

  KOKKOS_INLINE_FUNCTION
  void operator()(const TagGradSphereWkTestCovML &,
                  const TeamMember& team) const {
    KernelVariables kv(team);

    sphere_ops.grad_sphere_wk_testcov(kv,
                           geometry(kv.ie),
                           Homme::subview(scalar_input_d, kv.ie),
                           Homme::subview(vector_output_d,kv.ie));
  }  // end of op() for grad_sphere_wk_testcov

  KOKKOS_INLINE_FUNCTION
  void operator()(const TagVLaplaceCartesianML &,
                  const TeamMember& team) const {
    KernelVariables kv(team);

    sphere_ops.vlaplace_sphere_wk_cartesian (kv,
                                  geometry(kv.ie),
                                  Homme::subview(tensorVisc_d,kv.ie),
                                  Homme::subview(vector_input_d,kv.ie),
                                  Homme::subview(vector_output_d,kv.ie));
  }  // end of op() for laplace_tensor multil

  KOKKOS_INLINE_FUNCTION
  void operator()(const TagVLaplaceContraML &,
                  const TeamMember& team) const {
    KernelVariables kv(team);

    // don't forget to introduce nu_ratio
    sphere_ops.vlaplace_sphere_wk_contra(kv,
                              geometry(kv.ie), nu_ratio,
                              Homme::subview(vector_input_d, kv.ie),
                              Homme::subview(vector_output_d,kv.ie));
  }  // end of op() for laplace_tensor multil


  KOKKOS_INLINE_FUNCTION
  void operator()(const TagVorticityVectorML &,
                  const TeamMember& team) const {
    KernelVariables kv(team);
    sphere_ops.vorticity_sphere (kv,
                      geometry(kv.ie),
                      Homme::subview(vector_input_d, kv.ie),
                      Homme::subview(scalar_output_d,kv.ie));
  }  // end of op() for vorticity_sphere_vector multilevel



  void run_functor_gradient_sphere() {
    // league, team, vector_length_request=1
    auto policy = Homme::get_default_team_policy<ExecSpace, TagGradientSphereML>(_num_elems);
    sphere_ops.allocate_buffers(policy);
    Kokkos::parallel_for(policy, *this);
    ExecSpace::fence();
    // TO FROM
    Kokkos::deep_copy(vector_output_host, vector_output_d);
  };

  void run_functor_divergence_sphere_wk() {
    auto policy = Homme::get_default_team_policy<ExecSpace, TagDivergenceSphereWkML>(_num_elems);
    sphere_ops.allocate_buffers(policy);
    Kokkos::parallel_for(policy, *this);
    ExecSpace::fence();
    Kokkos::deep_copy(scalar_output_host, scalar_output_d);
  };

  void run_functor_divergence_sphere() {
    auto policy = Homme::get_default_team_policy<ExecSpace, TagDivergenceSphereML>(_num_elems);
    sphere_ops.allocate_buffers(policy);
    Kokkos::parallel_for(policy, *this);
    ExecSpace::fence();
    Kokkos::deep_copy(scalar_output_host, scalar_output_d);
  };

  void run_functor_divergence_sphere_update() {
    auto policy = Homme::get_default_team_policy<ExecSpace, TagDivergenceSphereUpdateML>(_num_elems);
    sphere_ops.allocate_buffers(policy);
    Kokkos::parallel_for(policy, *this);
    ExecSpace::fence();
    Kokkos::deep_copy(scalar_output_host, scalar_output_d);
  };

  void run_functor_laplace_wk() {
    auto policy = Homme::get_default_team_policy<ExecSpace, TagSimpleLaplaceML>(_num_elems);
    sphere_ops.allocate_buffers(policy);
    Kokkos::parallel_for(policy, *this);
    ExecSpace::fence();
    Kokkos::deep_copy(scalar_output_host, scalar_output_d);
  };

  void run_functor_tensor_laplace() {
    auto policy = Homme::get_default_team_policy<ExecSpace, TagTensorLaplaceML>(_num_elems);
    sphere_ops.allocate_buffers(policy);
    Kokkos::parallel_for(policy, *this);
    ExecSpace::fence();
    Kokkos::deep_copy(scalar_output_host, scalar_output_d);
  };

  void run_functor_curl_sphere_wk_testcov() {
    auto policy = Homme::get_default_team_policy<ExecSpace, TagCurlSphereWkTestCovML>(_num_elems);
    sphere_ops.allocate_buffers(policy);
    Kokkos::parallel_for(policy, *this);
    ExecSpace::fence();
    Kokkos::deep_copy(vector_output_host, vector_output_d);
  };

  void run_functor_grad_sphere_wk_testcov() {
    auto policy = Homme::get_default_team_policy<ExecSpace, TagGradSphereWkTestCovML>(_num_elems);
    sphere_ops.allocate_buffers(policy);
    Kokkos::parallel_for(policy, *this);
    ExecSpace::fence();
    Kokkos::deep_copy(vector_output_host, vector_output_d);
  };

  void run_functor_vlaplace_cartesian_reduced() {
    auto policy = Homme::get_default_team_policy<ExecSpace, TagVLaplaceCartesianML>(_num_elems);
    sphere_ops.allocate_buffers(policy);
    Kokkos::parallel_for(policy, *this);
    ExecSpace::fence();
    Kokkos::deep_copy(vector_output_host, vector_output_d);
  };

  void run_functor_vlaplace_contra() {
    auto policy = Homme::get_default_team_policy<ExecSpace, TagVLaplaceContraML>(_num_elems);
    sphere_ops.allocate_buffers(policy);
    Kokkos::parallel_for(policy, *this);
    ExecSpace::fence();
    Kokkos::deep_copy(vector_output_host, vector_output_d);
  };

  void run_functor_vorticity_sphere_vector() {
    auto policy = Homme::get_default_team_policy<ExecSpace, TagVorticityVectorML>(_num_elems);
    sphere_ops.allocate_buffers(policy);
    Kokkos::parallel_for(policy, *this);
    ExecSpace::fence();
    Kokkos::deep_copy(scalar_output_host, scalar_output_d);
  };

};  // end of class def compute_sphere_op_test_ml

// SHMEM ????
TEST_CASE("Testing_gradient_sphere", "gradient_sphere") {
  constexpr const int elements = 10;

  compute_sphere_operator_test_ml testing_grad_ml(elements);
  testing_grad_ml.run_functor_gradient_sphere();

  auto geo_host   = Homme::create_mirror_view_and_copy(testing_grad_ml.geometry);
  auto deriv_host = Homme::create_mirror_view_and_copy(testing_grad_ml.ref_FE.get_deriv());

  for(int ie = 0; ie < elements; ie++) {
    auto local_dinv = Homme::create_mirror_view_and_copy(geo_host(ie).m_dinv);
    for(int level = 0; level < NUM_LEV; ++level) {
      for(int v = 0; v < VECTOR_SIZE; ++v) {
        // fortran output
        Real local_fortran_output[2][NP][NP];
        // F input
        Real sf[NP][NP];

        // since i don't need flipping arrays, maybe, it is
        // enough to pass data() pointer?
        for(int _i = 0; _i < NP; _i++) {
          for(int _j = 0; _j < NP; _j++) {
            sf[_i][_j] = testing_grad_ml.scalar_input_host(
                ie, _i, _j, level)[v];
          }
        }

        // running F version of operator
        gradient_sphere_c_callable(
            &(sf[0][0]), deriv_host.data(),
            local_dinv.data(),
            &(local_fortran_output[0][0][0]));

        // compare with the part from C run
        for(int igp = 0; igp < NP; ++igp) {
          for(int jgp = 0; jgp < NP; ++jgp) {
            const Real coutput0 =
                testing_grad_ml.vector_output_host(
                    ie, 0, igp, jgp, level)[v];
            const Real coutput1 =
                testing_grad_ml.vector_output_host(
                    ie, 1, igp, jgp, level)[v];
            REQUIRE(!std::isnan(coutput0));
            REQUIRE(!std::isnan(coutput1));

            REQUIRE(!std::isnan(
                local_fortran_output[0][igp][jgp]));
            REQUIRE(!std::isnan(
                local_fortran_output[1][igp][jgp]));

            // what is 128 here?
            REQUIRE(local_fortran_output[0][igp][jgp] ==
                        coutput0);
            REQUIRE(local_fortran_output[1][igp][jgp] ==
                        coutput1);
          }  // jgp
        }    // igp
      }      // v
    }        // level
  }          //ie

  std::cout << "test grad multilevel finished. \n";

}  // end fo test grad_sphere_ml

TEST_CASE("Testing divergence_sphere_wk()",
          "divergence_sphere_wk") {
  constexpr const int elements = 10;

  compute_sphere_operator_test_ml testing_div_ml(elements);
  testing_div_ml.run_functor_divergence_sphere_wk();

  auto geo_host   = Homme::create_mirror_view_and_copy(testing_div_ml.geometry);
  auto deriv_host = Homme::create_mirror_view_and_copy(testing_div_ml.ref_FE.get_deriv());

  for(int ie = 0; ie < elements; ie++) {
    auto local_spheremp = Homme::create_mirror_view_and_copy(geo_host(ie).m_spheremp);
    auto local_dinv     = Homme::create_mirror_view_and_copy(geo_host(ie).m_dinv);
    for(int level = 0; level < NUM_LEV; ++level) {
      for(int v = 0; v < VECTOR_SIZE; ++v) {
        // fortran output
        Real local_fortran_output[NP][NP];
        // F input
        Real vf[2][NP][NP];
        Real dvvf[NP][NP];
        Real dinvf[2][2][NP][NP];
        Real sphf[NP][NP];

        for(int _i = 0; _i < NP; _i++)
          for(int _j = 0; _j < NP; _j++) {
            sphf[_i][_j] = local_spheremp(_i, _j);
            dvvf[_i][_j] = deriv_host(_i, _j);
            for(int _d1 = 0; _d1 < 2; _d1++) {
              vf[_d1][_i][_j] =
                  testing_div_ml.vector_input_host(
                      ie, _d1, _i, _j, level)[v];
              for(int _d2 = 0; _d2 < 2; _d2++)
                dinvf[_d1][_d2][_i][_j] = local_dinv(_d1,_d2, _i, _j);
            }
          }
        divergence_sphere_wk_c_callable(
            &(vf[0][0][0]), &(dvvf[0][0]), &(sphf[0][0]),
            &(dinvf[0][0][0][0]),
            &(local_fortran_output[0][0]));
        // compare with the part from C run
        for(int igp = 0; igp < NP; ++igp) {
          for(int jgp = 0; jgp < NP; ++jgp) {
            Real coutput0 =
                testing_div_ml.scalar_output_host(
                    ie, igp, jgp, level)[v];
            REQUIRE(!std::isnan(
                local_fortran_output[igp][jgp]));
            REQUIRE(!std::isnan(coutput0));
            REQUIRE(local_fortran_output[igp][jgp] ==
                        coutput0);
          }  // jgp
        }    // igp
      }      // v
    }        // level
  }          //ie

  std::cout << "test div_wk multilevel finished. \n";

}  // end of test div_sphere_wk_ml

TEST_CASE("Testing divergence_sphere()",
          "divergence_sphere") {
  constexpr const int elements = 10;

  compute_sphere_operator_test_ml testing_div_ml(elements);

  testing_div_ml.run_functor_divergence_sphere();

  auto geo_host   = Homme::create_mirror_view_and_copy(testing_div_ml.geometry);
  auto deriv_host = Homme::create_mirror_view_and_copy(testing_div_ml.ref_FE.get_deriv());

  for(int ie = 0; ie < elements; ie++) {
    auto local_metdet = Homme::create_mirror_view_and_copy(geo_host(ie).m_metdet);
    auto local_dinv   = Homme::create_mirror_view_and_copy(geo_host(ie).m_dinv);
    for(int level = 0; level < NUM_LEV; ++level) {
      for(int v = 0; v < VECTOR_SIZE; ++v) {
        // fortran output
        Real local_fortran_output[NP][NP] = {};
        // F input
        Real vf[2][NP][NP];
        Real dvvf[NP][NP];
        Real dinvf[2][2][NP][NP];
        Real metdetf[NP][NP];

        for(int _i = 0; _i < NP; _i++) {
          for(int _j = 0; _j < NP; _j++) {
            metdetf[_i][_j] = local_metdet(_i, _j);
            dvvf[_i][_j] = deriv_host(_i, _j);
            for(int _d1 = 0; _d1 < 2; _d1++) {
              vf[_d1][_i][_j] =
                  testing_div_ml.vector_input_host(
                      ie, _d1, _i, _j, level)[v];
              for(int _d2 = 0; _d2 < 2; _d2++)
                dinvf[_d1][_d2][_i][_j] = local_dinv(_d1, _d2, _i, _j);
            }
          }
        }
        divergence_sphere_c_callable(
            &(vf[0][0][0]), &(dvvf[0][0]), &(metdetf[0][0]),
            &(dinvf[0][0][0][0]),
            &(local_fortran_output[0][0]));

        // compare with the part from C run
        for(int igp = 0; igp < NP; ++igp) {
          for(int jgp = 0; jgp < NP; ++jgp) {
            Real coutput0 =
                testing_div_ml.scalar_output_host(
                    ie, igp, jgp, level)[v];
            REQUIRE(!std::isnan(
                local_fortran_output[igp][jgp]));
            REQUIRE(!std::isnan(coutput0));
            REQUIRE(local_fortran_output[igp][jgp] ==
                        coutput0);
          }  // jgp
        }    // igp
      }      // v
    }        // level
  }          //ie

  std::cout << "test div multilevel finished. \n";

}  // end of test div_sphere_wk_ml

TEST_CASE("Testing divergence_sphere_update()",
          "divergence_sphere_update") {
  constexpr const int elements = 10;

  compute_sphere_operator_test_ml testing_div_update_ml(elements);

  auto geo_host   = Homme::create_mirror_view_and_copy(testing_div_update_ml.geometry);
  auto deriv_host = Homme::create_mirror_view_and_copy(testing_div_update_ml.ref_FE.get_deriv());

  auto scalar_input_host = testing_div_update_ml.scalar_input_COPY2_host;

  // divergence_sphere_update uses the output as part of input (if add_hyperviscosity!=0),
  // so create a copy here for fortran
  decltype(testing_div_update_ml.scalar_output_d)::HostMirror scalar_output_host_copy("",elements);
  Kokkos::deep_copy(scalar_output_host_copy, testing_div_update_ml.scalar_output_d);

  testing_div_update_ml.run_functor_divergence_sphere_update();

  Real alpha = testing_div_update_ml.alpha;
  int add_hyperviscosity = testing_div_update_ml.add_hyperviscosity;
  for(int ie = 0; ie < elements; ie++) {
    auto local_metdet = Homme::create_mirror_view_and_copy(geo_host(ie).m_metdet);
    auto local_dinv   = Homme::create_mirror_view_and_copy(geo_host(ie).m_dinv);
    for(int level = 0; level < NUM_LEV; ++level) {
      for(int v = 0; v < VECTOR_SIZE; ++v) {
        // fortran output
        Real local_fortran_output[NP][NP] = {};
        // F input
        Real vf[2][NP][NP];
        Real dvvf[NP][NP];
        Real dinvf[2][2][NP][NP];
        Real metdetf[NP][NP];

        for(int _i = 0; _i < NP; _i++) {
          for(int _j = 0; _j < NP; _j++) {
            metdetf[_i][_j] = local_metdet(_i, _j);
            dvvf[_i][_j] = deriv_host(_i, _j);
            for(int _d1 = 0; _d1 < 2; _d1++) {
              vf[_d1][_i][_j] =
                  testing_div_update_ml.vector_input_host(
                      ie, _d1, _i, _j, level)[v] *
                  scalar_input_host(ie,_i,_j,level)[v];
              for(int _d2 = 0; _d2 < 2; _d2++)
                dinvf[_d1][_d2][_i][_j] = local_dinv(_d1, _d2, _i, _j);
            }
          }
        }
        divergence_sphere_c_callable(
            &(vf[0][0][0]), &(dvvf[0][0]), &(metdetf[0][0]),
            &(dinvf[0][0][0][0]),
            &(local_fortran_output[0][0]));

        for(int _i = 0; _i < NP; _i++) {
          for(int _j = 0; _j < NP; _j++) {
            local_fortran_output[_i][_j] *= alpha;
            local_fortran_output[_i][_j] += scalar_input_host(ie,_i,_j,level)[v];
            if (add_hyperviscosity!=0) {
              local_fortran_output[_i][_j] += scalar_output_host_copy(ie,_i,_j,level)[v];
            }
          }
        }
        // compare with the part from C run
        for(int igp = 0; igp < NP; ++igp) {
          for(int jgp = 0; jgp < NP; ++jgp) {
            Real coutput0 =
                testing_div_update_ml.scalar_output_host(
                    ie, igp, jgp, level)[v];
            REQUIRE(!std::isnan(
                local_fortran_output[igp][jgp]));
            REQUIRE(!std::isnan(coutput0));
            REQUIRE(local_fortran_output[igp][jgp] ==
                        coutput0);
          }  // jgp
        }    // igp
      }      // v
    }        // level
  }          //ie

  std::cout << "test div_updated multilevel finished. \n";

}  // end of test div_sphere_wk_ml

TEST_CASE("Testing simple laplace_wk()", "laplace_wk") {
  constexpr const int elements = 10;

  compute_sphere_operator_test_ml testing_laplace_ml(
      elements);
  testing_laplace_ml.run_functor_laplace_wk();

  auto geo_host   = Homme::create_mirror_view_and_copy(testing_laplace_ml.geometry);
  auto deriv_host = Homme::create_mirror_view_and_copy(testing_laplace_ml.ref_FE.get_deriv());

  for(int ie = 0; ie < elements; ie++) {
    auto local_spheremp = Homme::create_mirror_view_and_copy(geo_host(ie).m_spheremp);
    auto local_dinv     = Homme::create_mirror_view_and_copy(geo_host(ie).m_dinv);
    for(int level = 0; level < NUM_LEV; ++level) {
      for(int v = 0; v < VECTOR_SIZE; ++v) {
        // fortran output
        Real local_fortran_output[NP][NP];
        // F input
        Real sf[NP][NP];
        Real dvvf[NP][NP];
        Real dinvf[2][2][NP][NP];
        Real sphf[NP][NP];

        for(int _i = 0; _i < NP; _i++)
          for(int _j = 0; _j < NP; _j++) {
            sf[_i][_j] =
                testing_laplace_ml.scalar_input_host(
                    ie, _i, _j, level)[v];
            sphf[_i][_j] = local_spheremp(_i, _j);
            dvvf[_i][_j] = deriv_host(_i, _j);
            for(int _d1 = 0; _d1 < 2; _d1++)
              for(int _d2 = 0; _d2 < 2; _d2++)
                dinvf[_d1][_d2][_i][_j] = local_dinv(_d1, _d2, _i, _j);
          }

        laplace_simple_c_callable(
            &(sf[0][0]), &(dvvf[0][0]),
            &(dinvf[0][0][0][0]), &(sphf[0][0]),
            &(local_fortran_output[0][0]));

        for(int igp = 0; igp < NP; ++igp) {
          for(int jgp = 0; jgp < NP; ++jgp) {
            Real coutput0 =
                testing_laplace_ml.scalar_output_host(
                    ie, igp, jgp, level)[v];
            REQUIRE(!std::isnan(
                local_fortran_output[igp][jgp]));
            REQUIRE(!std::isnan(coutput0));
            // what is 128 here?
            REQUIRE(local_fortran_output[igp][jgp] ==
                        coutput0);
          }  // jgp
        }    // igp
      }      // v
    }        // level
  }          //ie

  std::cout
      << "test laplace_simple multilevel finished. \n";

}  // end of test laplace_simple multilevel

TEST_CASE("Testing laplace_tensor() multilevel",
          "laplace_tensor") {
  constexpr const int elements = 10;

  compute_sphere_operator_test_ml testing_tensor_laplace(
      elements);
  testing_tensor_laplace.run_functor_tensor_laplace();

  auto geo_host   = Homme::create_mirror_view_and_copy(testing_tensor_laplace.geometry);
  auto deriv_host = Homme::create_mirror_view_and_copy(testing_tensor_laplace.ref_FE.get_deriv());

  for(int ie = 0; ie < elements; ie++) {
    auto local_spheremp = Homme::create_mirror_view_and_copy(geo_host(ie).m_spheremp);
    auto local_dinv     = Homme::create_mirror_view_and_copy(geo_host(ie).m_dinv);
    for(int level = 0; level < NUM_LEV; ++level) {
      for(int v = 0; v < VECTOR_SIZE; ++v) {
        Real local_fortran_output[NP][NP];
        // F input
        Real sf[NP][NP];
        Real dvvf[NP][NP];
        Real dinvf[2][2][NP][NP];
        Real tensorf[2][2][NP][NP];
        Real sphf[NP][NP];

        for(int _i = 0; _i < NP; _i++)
          for(int _j = 0; _j < NP; _j++) {
            sf[_i][_j] =
                testing_tensor_laplace.scalar_input_host(
                    ie, _i, _j, level)[v];
            sphf[_i][_j] = local_spheremp(_i, _j);
            dvvf[_i][_j] = deriv_host(_i, _j);
            for(int _d1 = 0; _d1 < 2; _d1++)
              for(int _d2 = 0; _d2 < 2; _d2++) {
                dinvf[_d1][_d2][_i][_j] = local_dinv(_d1, _d2, _i, _j);

                tensorf[_d1][_d2][_i][_j] =
                    testing_tensor_laplace.tensorVisc_host(
                        ie, _d1, _d2, _i, _j);

              }  // end of d2 loop
          }

        Real _hp = 0.0;
        Real _hs = 1.0;
        bool _vc = true;

        laplace_sphere_wk_c_callable(
            &(sf[0][0]), &(dvvf[0][0]),
            &(dinvf[0][0][0][0]), &(sphf[0][0]),
            &(tensorf[0][0][0][0]), _hp, _hs, _vc,
            &(local_fortran_output[0][0]));

        for(int igp = 0; igp < NP; ++igp) {
          for(int jgp = 0; jgp < NP; ++jgp) {
            Real coutput0 =
                testing_tensor_laplace.scalar_output_host(
                    ie, igp, jgp, level)[v];

            REQUIRE(!std::isnan(
                local_fortran_output[igp][jgp]));
            REQUIRE(!std::isnan(coutput0));
            REQUIRE(local_fortran_output[igp][jgp] ==
                        coutput0);
          }  // jgp
        }    // igp
      }      // v
    }        // level
  }          //ie

  std::cout
      << "test laplace_tensor multilevel finished. \n";

}  // end of test laplace_tensor multilevel

TEST_CASE("Testing curl_sphere_wk_testcov() multilevel",
          "curl_sphere_wk_testcov") {
  constexpr const int elements = 10;

  compute_sphere_operator_test_ml testing_curl(elements);
  testing_curl.run_functor_curl_sphere_wk_testcov();

  auto geo_host   = Homme::create_mirror_view_and_copy(testing_curl.geometry);
  auto deriv_host = Homme::create_mirror_view_and_copy(testing_curl.ref_FE.get_deriv());
  auto mass_host  = Homme::create_mirror_view_and_copy(testing_curl.ref_FE.get_mass());

  HostViewManaged<Real[2][NP][NP]> local_fortran_output("curl_sphere_wk_testcov fortran results");
  for(int ie = 0; ie < elements; ie++) {
    auto local_spheremp = Homme::create_mirror_view_and_copy(geo_host(ie).m_spheremp);
    auto local_d        = Homme::create_mirror_view_and_copy(geo_host(ie).m_d);
    for(int level = 0; level < NUM_LEV; ++level) {
      for(int v = 0; v < VECTOR_SIZE; ++v) {
        Real sf[NP][NP];
        Real dvvf[NP][NP];
        Real df[2][2][NP][NP];
        Real mpf[NP][NP];

        for(int _i = 0; _i < NP; _i++)
          for(int _j = 0; _j < NP; _j++) {
            sf[_i][_j] = testing_curl.scalar_input_host(
                ie, _i, _j, level)[v];
            mpf[_i][_j] = mass_host(_i, _j);
            dvvf[_i][_j] = deriv_host(_i, _j);

            for(int _d1 = 0; _d1 < 2; _d1++)
              for(int _d2 = 0; _d2 < 2; _d2++) {
                df[_d1][_d2][_i][_j] = local_d(_d1, _d2, _i, _j);

              }  // end of d2 loop
          }
        curl_sphere_wk_testcov_c_callable(
            &(sf[0][0]), &(dvvf[0][0]), &(df[0][0][0][0]),
            &(mpf[0][0]), local_fortran_output.data());

        for(int igp = 0; igp < NP; ++igp) {
          for(int jgp = 0; jgp < NP; ++jgp) {
            Real coutput0 = testing_curl.vector_output_host(
                ie, 0, igp, jgp, level)[v];

            Real coutput1 = testing_curl.vector_output_host(
                ie, 1, igp, jgp, level)[v];

            REQUIRE(!std::isnan(
                    local_fortran_output(0, igp, jgp)));
            REQUIRE(!std::isnan(
                    local_fortran_output(1, igp, jgp)));

            REQUIRE(!std::isnan(coutput0));
            REQUIRE(!std::isnan(coutput1));

            REQUIRE(local_fortran_output(0, igp, jgp) ==
                        coutput0);
            REQUIRE(local_fortran_output(1, igp, jgp) == coutput1);

          }  // jgp
        }    // igp
      }      // v
    }        // level
  }          //ie

  std::cout << "test curl_sphere_wk_testcov multilevel "
               "finished. \n";

}  // end of test laplace_tensor multilevel

TEST_CASE("Testing grad_sphere_wk_testcov() multilevel",
          "grad_sphere_wk_testcov") {
  constexpr const int elements = 10;

  compute_sphere_operator_test_ml testing_grad(elements);
  testing_grad.run_functor_grad_sphere_wk_testcov();

  auto geo_host   = Homme::create_mirror_view_and_copy(testing_grad.geometry);
  auto deriv_host = Homme::create_mirror_view_and_copy(testing_grad.ref_FE.get_deriv());
  auto mass_host  = Homme::create_mirror_view_and_copy(testing_grad.ref_FE.get_mass());

  for(int ie = 0; ie < elements; ie++) {
    auto local_metdet = Homme::create_mirror_view_and_copy(geo_host(ie).m_metdet);
    auto local_metinv = Homme::create_mirror_view_and_copy(geo_host(ie).m_metinv);
    auto local_d      = Homme::create_mirror_view_and_copy(geo_host(ie).m_d);
    for(int level = 0; level < NUM_LEV; ++level) {
      for(int v = 0; v < VECTOR_SIZE; ++v) {
        Real local_fortran_output[2][NP][NP];
        Real sf[NP][NP];
        Real dvvf[NP][NP];
        Real df[2][2][NP][NP];
        Real mpf[NP][NP];
        Real metdetf[NP][NP];
        Real metinvf[2][2][NP][NP];

        for(int _i = 0; _i < NP; _i++)
          for(int _j = 0; _j < NP; _j++) {
            sf[_i][_j] = testing_grad.scalar_input_host(
                ie, _i, _j, level)[v];
            mpf[_i][_j] = mass_host(_i, _j);
            metdetf[_i][_j] = local_metdet(_i, _j);
            dvvf[_i][_j] = deriv_host(_i, _j);

            for(int _d1 = 0; _d1 < 2; _d1++)
              for(int _d2 = 0; _d2 < 2; _d2++) {
                df[_d1][_d2][_i][_j] = local_d(_d1, _d2, _i, _j);

                metinvf[_d1][_d2][_i][_j] = local_metinv(_d1, _d2, _i, _j);
              }  // end of d2 loop
          }
        gradient_sphere_wk_testcov_c_callable(
            &(sf[0][0]), &(dvvf[0][0]),
            &(metinvf[0][0][0][0]), &(metdetf[0][0]),
            &(df[0][0][0][0]), &(mpf[0][0]),
            &(local_fortran_output[0][0][0]));

        for(int igp = 0; igp < NP; ++igp) {
          for(int jgp = 0; jgp < NP; ++jgp) {
            Real coutput0 = testing_grad.vector_output_host(
                ie, 0, igp, jgp, level)[v];

            Real coutput1 = testing_grad.vector_output_host(
                ie, 1, igp, jgp, level)[v];

            REQUIRE(!std::isnan(
                local_fortran_output[0][igp][jgp]));

            REQUIRE(!std::isnan(
                local_fortran_output[1][igp][jgp]));

            REQUIRE(!std::isnan(coutput0));
            REQUIRE(!std::isnan(coutput1));
            REQUIRE(local_fortran_output[0][igp][jgp] ==
                        coutput0);
            REQUIRE(local_fortran_output[1][igp][jgp] ==
                        coutput1);

          }  // jgp
        }    // igp
      }      // v
    }        // level
  }          //ie

  std::cout << "test grad_sphere_wk_testcov multilevel "
               "finished. \n";

}  // end of test laplace_tensor multilevel

TEST_CASE(
    "Testing vlaplace_sphere_wk_cartesian() multilevel",
    "vlaplace_sphere_wk_cartesian") {
  constexpr const int elements = 10;

  compute_sphere_operator_test_ml testing_vlaplace(
      elements);
  testing_vlaplace.run_functor_vlaplace_cartesian_reduced();

  auto geo_host   = Homme::create_mirror_view_and_copy(testing_vlaplace.geometry);
  auto deriv_host = Homme::create_mirror_view_and_copy(testing_vlaplace.ref_FE.get_deriv());

  for(int ie = 0; ie < elements; ie++) {
    auto local_vec_sph2cart = Homme::create_mirror_view_and_copy(geo_host(ie).m_vec_sph2cart);
    auto local_spheremp     = Homme::create_mirror_view_and_copy(geo_host(ie).m_spheremp);
    auto local_dinv         = Homme::create_mirror_view_and_copy(geo_host(ie).m_dinv);
    for(int level = 0; level < NUM_LEV; ++level) {
      for(int v = 0; v < VECTOR_SIZE; ++v) {
        Real local_fortran_output[2][NP][NP];
        Real vf[2][NP][NP];  // input
        Real dvvf[NP][NP];
        Real dinvf[2][2][NP][NP];
        Real tensorf[2][2][NP][NP];
        Real vec_sph2cartf[2][3][NP][NP];
        Real sphf[NP][NP];

        for(int _i = 0; _i < NP; _i++)
          for(int _j = 0; _j < NP; _j++) {
            vf[0][_i][_j] =
                testing_vlaplace.vector_input_host(
                    ie, 0, _i, _j, level)[v];
            vf[1][_i][_j] =
                testing_vlaplace.vector_input_host(
                    ie, 1, _i, _j, level)[v];

            sphf[_i][_j] = local_spheremp(_i, _j);
            dvvf[_i][_j] = deriv_host(_i, _j);
            for(int _d1 = 0; _d1 < 2; _d1++)
              for(int _d2 = 0; _d2 < 2; _d2++) {
                dinvf[_d1][_d2][_i][_j] = local_dinv(_d1, _d2, _i, _j);

                tensorf[_d1][_d2][_i][_j] =
                    testing_vlaplace.tensorVisc_host(ie, _d1, _d2, _i, _j);

              }  // end of d2 loop
            for(int _d1 = 0; _d1 < 2; _d1++)
              for(int _d2 = 0; _d2 < 3; _d2++) {
                vec_sph2cartf[_d1][_d2][_i][_j] = local_vec_sph2cart(_d1, _d2, _i, _j);
              }  // end of d2 loop
          }      // end of j loop

        Real _hp = 0.0;
        Real _hs = 1.0;
        bool _vc = true;

        vlaplace_sphere_wk_cartesian_c_callable(
            &(vf[0][0][0]), &(dvvf[0][0]),
            &(dinvf[0][0][0][0]), &(sphf[0][0]),
            &(tensorf[0][0][0][0]),
            &(vec_sph2cartf[0][0][0][0]), _hp, _hs, _vc,
            &(local_fortran_output[0][0][0]));

        for(int igp = 0; igp < NP; ++igp) {
          for(int jgp = 0; jgp < NP; ++jgp) {
            Real coutput0 =
                testing_vlaplace.vector_output_host(
                    ie, 0, igp, jgp, level)[v];
            Real coutput1 =
                testing_vlaplace.vector_output_host(
                    ie, 1, igp, jgp, level)[v];

            REQUIRE(!std::isnan(
                local_fortran_output[0][igp][jgp]));
            REQUIRE(!std::isnan(
                local_fortran_output[1][igp][jgp]));
            REQUIRE(!std::isnan(coutput0));
            REQUIRE(!std::isnan(coutput1));
            REQUIRE(local_fortran_output[0][igp][jgp] ==
                        coutput0);
            REQUIRE(local_fortran_output[1][igp][jgp] ==
                        coutput1);
          }  // jgp
        }    // igp
      }      // v
    }        // level
  }          //ie

  std::cout << "test vlaplace_sphere_wk_cartesian "
               "multilevel finished. \n";

}  // end of test laplace_sphere_wk_contra multilevel

TEST_CASE("Testing vlaplace_sphere_wk_contra() multilevel",
          "vlaplace_sphere_wk_contra") {
  constexpr const int elements = 10;

  compute_sphere_operator_test_ml testing_vlaplace(
      elements);
  testing_vlaplace.run_functor_vlaplace_contra();

  auto geo_host   = Homme::create_mirror_view_and_copy(testing_vlaplace.geometry);
  auto deriv_host = Homme::create_mirror_view_and_copy(testing_vlaplace.ref_FE.get_deriv());
  auto mass_host  = Homme::create_mirror_view_and_copy(testing_vlaplace.ref_FE.get_mass());

  for(int ie = 0; ie < elements; ie++) {
    auto local_spheremp = Homme::create_mirror_view_and_copy(geo_host(ie).m_spheremp);
    auto local_metdet   = Homme::create_mirror_view_and_copy(geo_host(ie).m_metdet);
    auto local_metinv   = Homme::create_mirror_view_and_copy(geo_host(ie).m_metinv);
    auto local_dinv     = Homme::create_mirror_view_and_copy(geo_host(ie).m_dinv);
    auto local_d        = Homme::create_mirror_view_and_copy(geo_host(ie).m_d);
    for(int level = 0; level < NUM_LEV; ++level) {
      for(int v = 0; v < VECTOR_SIZE; ++v) {
        Real local_fortran_output[2][NP][NP];
        Real vf[2][NP][NP];  // input
        Real dvvf[NP][NP];
        Real df[2][2][NP][NP];
        Real dinvf[2][2][NP][NP];
        Real mpf[NP][NP];
        Real sphf[NP][NP];
        Real metinvf[2][2][NP][NP];
        Real metdetf[NP][NP];
        Real rmetdetf[NP][NP];
        // let's test with 1 first, then we need to test
        // with random...
        Real nu_ratio = testing_vlaplace.nu_ratio;

        for(int _i = 0; _i < NP; _i++)
          for(int _j = 0; _j < NP; _j++) {
            vf[0][_i][_j] =
                testing_vlaplace.vector_input_host(
                    ie, 0, _i, _j, level)[v];
            vf[1][_i][_j] =
                testing_vlaplace.vector_input_host(
                    ie, 1, _i, _j, level)[v];

            mpf[_i][_j] = mass_host(_i, _j);
            sphf[_i][_j] = local_spheremp(_i, _j);
            metdetf[_i][_j] = local_metdet(_i, _j);
            rmetdetf[_i][_j] = 1.0 / metdetf[_i][_j];

            dvvf[_i][_j] = deriv_host(_i, _j);
            for(int _d1 = 0; _d1 < 2; _d1++)
              for(int _d2 = 0; _d2 < 2; _d2++) {
                df[_d1][_d2][_i][_j]    = local_d(_d1, _d2, _i, _j);
                dinvf[_d1][_d2][_i][_j] = local_dinv(_d1, _d2, _i, _j);

                metinvf[_d1][_d2][_i][_j] = local_metinv(_d1, _d2, _i, _j);
              }  // end of d2 loop
          }      // end of j loop

        vlaplace_sphere_wk_contra_c_callable(
            &(vf[0][0][0]), &(dvvf[0][0]),
            &(df[0][0][0][0]), &(dinvf[0][0][0][0]),
            &(mpf[0][0]), &(sphf[0][0]),
            &(metinvf[0][0][0][0]), &(metdetf[0][0]),
            &(rmetdetf[0][0]), nu_ratio,
            &(local_fortran_output[0][0][0]));

        for(int igp = 0; igp < NP; ++igp) {
          for(int jgp = 0; jgp < NP; ++jgp) {
            Real coutput0 =
                testing_vlaplace.vector_output_host(
                    ie, 0, igp, jgp, level)[v];
            Real coutput1 =
                testing_vlaplace.vector_output_host(
                    ie, 1, igp, jgp, level)[v];
            // std::cout << igp << "," << jgp << " F output0
            // = " <<  local_fortran_output[0][igp][jgp] << ",
            // C output0 = " << coutput0 << "\n";
            REQUIRE(!std::isnan(
                local_fortran_output[0][igp][jgp]));
            REQUIRE(!std::isnan(
                local_fortran_output[1][igp][jgp]));
            REQUIRE(!std::isnan(coutput0));
            REQUIRE(!std::isnan(coutput1));
            REQUIRE(local_fortran_output[0][igp][jgp] ==
                        coutput0);
            REQUIRE(local_fortran_output[1][igp][jgp] ==
                        coutput1);
          }  // jgp
        }    // igp
      }      // v
    }        // level
  }          //ie

  std::cout << "test vlaplace_sphere_wk_contra multilevel "
               "finished. \n";

}  // end of test vlaplace_contra multilevel

TEST_CASE("Testing vorticity_sphere_vector()",
          "vorticity_sphere_vector") {
  constexpr const int elements = 10;

  compute_sphere_operator_test_ml testing_vort(elements);
  testing_vort.run_functor_vorticity_sphere_vector();

  auto geo_host   = Homme::create_mirror_view_and_copy(testing_vort.geometry);
  auto deriv_host = Homme::create_mirror_view_and_copy(testing_vort.ref_FE.get_deriv());

  for(int ie = 0; ie < elements; ie++) {
    auto local_metdet   = Homme::create_mirror_view_and_copy(geo_host(ie).m_metdet);
    auto local_d        = Homme::create_mirror_view_and_copy(geo_host(ie).m_d);
    for(int level = 0; level < NUM_LEV; ++level) {
      for(int v = 0; v < VECTOR_SIZE; ++v) {
        Real local_fortran_output[NP][NP];
        Real vf[2][NP][NP];
        Real dvvf[NP][NP];
        Real df[2][2][NP][NP];
        Real rmetdetf[NP][NP];

        for(int _i = 0; _i < NP; _i++)
          for(int _j = 0; _j < NP; _j++) {
            rmetdetf[_i][_j] = 1./local_metdet(_i, _j);
            dvvf[_i][_j] = deriv_host(_i, _j);
            for(int _d1 = 0; _d1 < 2; _d1++) {
              vf[_d1][_i][_j] =
                  testing_vort.vector_input_host(
                      ie, _d1, _i, _j, level)[v];
              for(int _d2 = 0; _d2 < 2; _d2++)
                df[_d1][_d2][_i][_j] = local_d(_d1, _d2, _i, _j);
            }
          }
        vorticity_sphere_c_callable(
            &(vf[0][0][0]), &(dvvf[0][0]), &(rmetdetf[0][0]),
            &(df[0][0][0][0]),
            &(local_fortran_output[0][0]));
        for(int igp = 0; igp < NP; ++igp) {
          for(int jgp = 0; jgp < NP; ++jgp) {
            Real coutput0 =
                testing_vort.scalar_output_host(
                    ie, igp, jgp, level)[v];
            REQUIRE(!std::isnan(
                local_fortran_output[igp][jgp]));
            REQUIRE(!std::isnan(coutput0));
            REQUIRE(local_fortran_output[igp][jgp] ==
                        coutput0);
          }  // jgp
        }    // igp
      }      // v
    }        // level
  }          //ie

  std::cout << "test vorticity_sphere_vector multilevel finished. \n";

}  // end of test div_sphere_wk_ml
