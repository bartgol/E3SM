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

void laplace_simple_c_callable(const Real *input,
                               const Real *dvv,
                               const Real *dinv,
                               const Real *metdet,
                               Real *output);

void gradient_sphere_c_callable(const Real *input,
                                const Real *dvv,
                                const Real *dinv,
                                Real *output);

void divergence_sphere_wk_c_callable(const Real *input,
                                     const Real *dvv,
                                     const Real *spheremp,
                                     const Real *dinv,
                                     Real *output);

}  // extern C

class compute_sphere_operator_test {
 public:
  compute_sphere_operator_test(int num_elems)
      : _num_elems(num_elems),
        scalar_input_d("scalar input", num_elems),
        vector_input_d("vector input", num_elems),
        scalar_output_d("scalar output", num_elems),
        vector_output_d("vector output", num_elems),
        scalar_input_host(
            Kokkos::create_mirror_view(scalar_input_d)),
        vector_input_host(
            Kokkos::create_mirror_view(vector_input_d)),
        scalar_output_host(
            Kokkos::create_mirror_view(scalar_output_d)),
        vector_output_host(
            Kokkos::create_mirror_view(vector_output_d))
  {
    // constructor's body
    // init randonly

    std::random_device rd;
    rngAlg engine(rd());

    // check singularities? divergence_wk uses both D and
    // Dinv, does it matter if they are not inverses of
    // each other?
    genRandArray(scalar_input_host, engine,
                 std::uniform_real_distribution<Real>(
                     0, 100.0));
    Kokkos::deep_copy(scalar_input_d, scalar_input_host);

    genRandArray(vector_input_host, engine,
                 std::uniform_real_distribution<Real>(
                     -100.0, 100.0));
    Kokkos::deep_copy(vector_input_d, vector_input_host);

    // Geometric stuff
    geometry = ExecViewManaged<ElementGeometry *>("",num_elems);
    auto geometry_host = Kokkos::create_mirror_view(geometry);
    for (int ie=0; ie<num_elems; ++ie) {
      geometry_host(ie).random_init(true,true);
    }
    Kokkos::deep_copy(geometry,geometry_host);

    // Reference element stuff
    ref_FE.random_init();

    // Set reference element views in SphereOperators
    sphere_ops.init(ref_FE);
  }

  const int _num_elems;  // league size, serves as ie index

  // device views
  ExecViewManaged<Real * [NP][NP]> scalar_input_d;
  ExecViewManaged<Real * [2][NP][NP]> vector_input_d;
  ExecViewManaged<Real * [NP][NP]> scalar_output_d;
  ExecViewManaged<Real * [2][NP][NP]> vector_output_d;

  // host views, one dim is num_elems. Spherical operators
  // do not take ie or nlev fields,  but to make it a more
  // reasonable test and to have parallel_for we ise another
  // dimension.
  ExecViewManaged<Real * [NP][NP]>::HostMirror
      scalar_input_host;
  // how to get total length of view? use dim0*dim1*...till
  // dim7
  const int scalar_input_len = NP * NP;  // temp code
  ExecViewManaged<Real * [2][NP][NP]>::HostMirror
      vector_input_host;
  // how to get total length of view? use dim0*dim1*...till
  // dim7
  const int vector_input_len = 2 * NP * NP;  // temp code

  ExecViewManaged<ElementGeometry *> geometry;

  ExecViewManaged<Real * [NP][NP]>::HostMirror
      scalar_output_host;
  ExecViewManaged<Real * [2][NP][NP]>::HostMirror
      vector_output_host;

  ReferenceElement    ref_FE;
  SphereOperators     sphere_ops;

  // tag for laplace_simple()
  struct TagSimpleLaplace {};
  // tag for gradient_sphere()
  struct TagGradientSphere {};
  // tag for divergence_sphere_wk
  struct TagDivergenceSphereWk {};
  // tag for default, a dummy
  struct TagDefault {};

  KOKKOS_INLINE_FUNCTION
  void operator()(const TagDefault &,
                  TeamMember team) const {
      // do nothing or print a message
  };

  KOKKOS_INLINE_FUNCTION
  void operator()(const TagSimpleLaplace &,
                  TeamMember team) const {
    KernelVariables kv(team);

    sphere_ops.laplace_wk_sl(kv,
                  geometry(kv.ie),
                  Homme::subview(scalar_input_d,kv.ie),
                  Homme::subview(scalar_output_d,kv.ie));
  };  // end of op() for laplace_simple

  /*
   * A comment on how these tests work:
   * Consider 160 threads available, with _num_elems=10;
   *
   * Below are lines
   *     Kokkos::TeamPolicy<ExecSpace, TagSimpleLaplace>
   * policy(_num_elems, 16); Kokkos::parallel_for(policy,
   * *this); this one will call operator() with, say,
   * weak divergence tag. Then first 160 threads will
   * be clustered into 10 leagues (as many as _num_elems).
   * Each league will contain 16 threads (16 as the second
   * argument in policy().) Each league will have its
   * separate subview input and subview output (subview of
   * global arrays based on team.league_rank), so, league 2
   * will have input from local_vector_input_d(2,:,:), etc.
   * When divergence_sphere_wk is called, it will be
   * executed by 16 threads in league, each sharing input
   * and output. So, it is not a perfect situation and not a
   * perfect test, because 16 team threads run the same code
   * and OVERWRITE the same output. A better test should
   * have another level of parallelism, a loop with
   * TeamThreadRange. Also, one should note that
   * divergence_sphere_wk as well as other SphereOperators
   * should be called from loop with aeamThreadRange.
   */

  KOKKOS_INLINE_FUNCTION
  void operator()(const TagDivergenceSphereWk &,
                  TeamMember team) const {
    KernelVariables kv(team);

    sphere_ops.divergence_sphere_wk_sl(team,
                            geometry(kv.ie),
                            Homme::subview(vector_input_d, kv.ie),
                            Homme::subview(scalar_output_d,kv.ie));
  };  // end of op() for divergence_sphere_wk

  KOKKOS_INLINE_FUNCTION
  void operator()(const TagGradientSphere &,
                  TeamMember team) const {
    KernelVariables kv(team);

    sphere_ops.gradient_sphere_sl(team,
                       geometry(kv.ie),
                       Homme::subview(scalar_input_d, kv.ie),
                       Homme::subview(vector_output_d,kv.ie));
  };

  // this could be even nicer,
  // put in a param in run_functor(param) to only branch
  // policy type
  void run_functor_simple_laplace() {
    auto policy = Homme::get_default_team_policy<ExecSpace,TagSimpleLaplace>(_num_elems);
    sphere_ops.allocate_buffers(policy);
    Kokkos::parallel_for(policy, *this);
    ExecSpace::fence();
    // TO FROM
    Kokkos::deep_copy(scalar_output_host, scalar_output_d);
  };

  void run_functor_gradient_sphere() {
    auto policy = Homme::get_default_team_policy<ExecSpace,TagGradientSphere>(_num_elems);
    sphere_ops.allocate_buffers(policy);
    Kokkos::parallel_for(policy, *this);
    ExecSpace::fence();
    // TO FROM
    Kokkos::deep_copy(vector_output_host, vector_output_d);
  };

  void run_functor_div_wk() {
    auto policy = Homme::get_default_team_policy<ExecSpace,TagDivergenceSphereWk>(_num_elems);
    sphere_ops.allocate_buffers(policy);
    Kokkos::parallel_for(policy, *this);
    ExecSpace::fence();
    // TO FROM
    // remember to copy correct output
    Kokkos::deep_copy(scalar_output_host, scalar_output_d);
  };

};  // end of definition of compute_sphere_operator_test()

TEST_CASE("testing_laplace_simple_sl",
          "laplace_simple_sl") {
  constexpr const int elements = 10;

  compute_sphere_operator_test testing_laplace(elements);

  testing_laplace.run_functor_simple_laplace();

  HostViewManaged<Real[NP][NP]> local_fortran_output("fortran results");
  auto geo_host   = Homme::create_mirror_view_and_copy(testing_laplace.geometry);
  auto deriv_host = Homme::create_mirror_view_and_copy(testing_laplace.ref_FE.get_deriv());

  for(int ie = 0; ie < elements; ie++) {
    HostViewUnmanaged<Real[NP][NP]> local_scalar_input =
        Homme::subview(testing_laplace.scalar_input_host,
                        ie);

    auto local_dinv      = Homme::create_mirror_view_and_copy(geo_host(ie).m_dinv);
    auto local_spheremp  = Homme::create_mirror_view_and_copy(geo_host(ie).m_spheremp);

    // run F code
    laplace_simple_c_callable(
        local_scalar_input.data(),
        deriv_host.data(),
        local_dinv.data(), local_spheremp.data(),
        local_fortran_output.data());

    // compare answers
    for(int igp = 0; igp < NP; ++igp) {
      for(int jgp = 0; jgp < NP; ++jgp) {
        REQUIRE(
            !std::isnan(local_fortran_output(igp, jgp)));
        REQUIRE(
            !std::isnan(testing_laplace.scalar_output_host(
                ie, igp, jgp)));
        Real rel_error = compare_answers(
                    local_fortran_output(igp, jgp),
                    testing_laplace.scalar_output_host(
                        ie, igp, jgp));
        REQUIRE(local_fortran_output(igp, jgp) ==
                    testing_laplace.scalar_output_host(
                        ie, igp, jgp));
      }  // jgp
    }    // igp
  }      // end of for loop for elements

  std::cout
      << "simple_laplace_sl single level test finished.\n";

};  // end of TEST_CASE(..., "simple laplace")

TEST_CASE("Testing div_wk_sl()", "div_wk_sl") {
  constexpr const int elements = 1;

  compute_sphere_operator_test testing_divwk(elements);

  testing_divwk.run_functor_div_wk();
  auto geo_host   = Homme::create_mirror_view_and_copy(testing_divwk.geometry);
  auto deriv_host = Homme::create_mirror_view_and_copy(testing_divwk.ref_FE.get_deriv());

  for(int ie = 0; ie < elements; ie++) {
    Real local_fortran_output[NP][NP];

    HostViewUnmanaged<Real[2][NP][NP]> local_vector_input =
        Homme::subview(testing_divwk.vector_input_host,
                        ie);

    auto local_dinv     = Homme::create_mirror_view_and_copy(geo_host(ie).m_dinv);
    auto local_spheremp = Homme::create_mirror_view_and_copy(geo_host(ie).m_spheremp);

    Real vf[2][NP][NP];
    Real dvvf[NP][NP];
    Real dinvf[2][2][NP][NP];
    Real sphf[NP][NP];

    for(int _i = 0; _i < NP; _i++)
      for(int _j = 0; _j < NP; _j++) {
        dvvf[_i][_j] = deriv_host(_i, _j);
        sphf[_i][_j] = local_spheremp(_i, _j);
        for(int _d1 = 0; _d1 < 2; _d1++) {
          vf[_d1][_i][_j] = local_vector_input(_d1, _i, _j);
          for(int _d2 = 0; _d2 < 2; _d2++)
            dinvf[_d1][_d2][_i][_j] =
                local_dinv(_d1, _d2, _i, _j);
        }
      }

    divergence_sphere_wk_c_callable(
        &(vf[0][0][0]), &(dvvf[0][0]), &(sphf[0][0]),
        &(dinvf[0][0][0][0]),
        &(local_fortran_output[0][0]));

    for(int igp = 0; igp < NP; ++igp) {
      for(int jgp = 0; jgp < NP; ++jgp) {
        REQUIRE(
            !std::isnan(local_fortran_output[igp][jgp]));
        REQUIRE(
            !std::isnan(testing_divwk.scalar_output_host(
                ie, igp, jgp)));
        REQUIRE(local_fortran_output[igp][jgp] ==
                    testing_divwk.scalar_output_host(
                        ie, igp, jgp));
      }  // jgp
    }    // igp
  };     // end of elements loop

  std::cout << "div_wk single level test finished.\n";

}  // end of TEST_CASE(...,"divergence_sphere_wk")

TEST_CASE("Testing gradient_sphere_sl()",
          "gradient_sphere") {
  constexpr const int elements = 10;

  compute_sphere_operator_test testing_grad(elements);

  // running kokkos version of operator
  testing_grad.run_functor_gradient_sphere();

  auto geo_host   = Homme::create_mirror_view_and_copy(testing_grad.geometry);
  auto deriv_host = Homme::create_mirror_view_and_copy(testing_grad.ref_FE.get_deriv());

  for(int ie = 0; ie < elements; ie++) {
    Real local_fortran_output[2][NP][NP];

    HostViewUnmanaged<Real[NP][NP]> local_scalar_input =
        Homme::subview(testing_grad.scalar_input_host,
                        ie);

    auto local_dinv = Homme::create_mirror_view_and_copy(geo_host(ie).m_dinv);

    Real sf[NP][NP];
    Real dvvf[NP][NP];
    Real dinvf[2][2][NP][NP];

    // flipping arrays -- WRITE DOWN HOW THIS SHOULD BE DONE
    for(int _i = 0; _i < NP; _i++)
      for(int _j = 0; _j < NP; _j++) {
        sf[_i][_j] = local_scalar_input(_i, _j);
        dvvf[_i][_j] = deriv_host(_i, _j);
        for(int _d1 = 0; _d1 < 2; _d1++)
          for(int _d2 = 0; _d2 < 2; _d2++)
            dinvf[_d1][_d2][_i][_j] =
                local_dinv(_d1, _d2, _i, _j);
      }

    // running F version of operator
    gradient_sphere_c_callable(
        &(sf[0][0]), &(dvvf[0][0]), &(dinvf[0][0][0][0]),
        &(local_fortran_output[0][0][0]));

    // comparing answers from kokkos and F
    for(int igp = 0; igp < NP; ++igp)
      for(int jgp = 0; jgp < NP; ++jgp)
        for(int _d = 0; _d < 2; ++_d) {
          REQUIRE(!std::isnan(
              local_fortran_output[_d][igp][jgp]));
          REQUIRE(
              !std::isnan(testing_grad.vector_output_host(
                  ie, _d, igp, jgp)));
          REQUIRE(local_fortran_output[_d][igp][jgp] ==
                      testing_grad.vector_output_host(
                          ie, _d, igp, jgp));
        }  // end of comparing answers

  }  // end of loop for elements

  std::cout << "grad single level test finished.\n";

};  // end of TEST_CASE(..., "gradient_sphere")

// SHMEM ????
