/********************************************************************************
 * HOMMEXX 1.0: Copyright of Sandia Corporation
 * This software is released under the BSD license
 * See the file 'COPYRIGHT' in the HOMMEXX/src/share/cxx directory
 *******************************************************************************/

#ifndef HOMMEXX_PPM_REMAP_HPP
#define HOMMEXX_PPM_REMAP_HPP

#include "ErrorDefs.hpp"

#include "Kokkos_Concepts.hpp"

#include "RemapFunctor.hpp"

#include "Elements.hpp"
#include "KernelVariables.hpp"
#include "Types.hpp"
#include "ExecSpaceDefs.hpp"
#include "utilities/LoopsUtils.hpp"
#include "utilities/MathUtils.hpp"
#include "utilities/RangeView.hpp"
#include "utilities/SubviewUtils.hpp"
#include "utilities/SyncUtils.hpp"

#include "profiling.hpp"

namespace Homme {
namespace Remap {
namespace Ppm {

constexpr int NUM_GHOSTS = 2;

template<int PhysicalLength, int NumFrontGhosts, int NumBackGhosts>
struct GhostedColumnProps {
private:
  // This is private, cause the users need not to know these details
  static constexpr int MEMORY_ALIGNMENT = Kokkos::Impl::MEMORY_ALIGNMENT;
  static constexpr int REAL_SIZE        = sizeof(Real);

  static_assert (MEMORY_ALIGNMENT % REAL_SIZE  == 0, "Error! Real does not divide the Kokkos memory alignment. Big trouble...\n");

  // Either VECTOR_SIZE divides MEMORY_ALIGNMENT or viceversa. E.g.:
  //  - HSW: vector_size is 4, Kokkos sets memory_alignment to 8 doubles
  //  - XYZ: vector_size is 16, Kokkos sets memory_alignment to 8 doubles

  // If sizeof(Real) doesn't divide the memory alignment value for the
  // architecture, we're in trouble regardless
  static constexpr int REAL_ALIGNMENT   = MEMORY_ALIGNMENT / REAL_SIZE;

  // Number of ghost levels (possibly) used in the remap procedures
  static constexpr int NumFrontGhostsPacks = (NumFrontGhosts + VECTOR_SIZE - 1) / VECTOR_SIZE;
public:

  // Padding to improve memory access alignment. The padding allows the entry 0 (the first
  // after the ghosts) to be correctly aligned.
  // Note: we do NOT count ghosts in the padding
  static constexpr int InitialPadding = lcm(NumFrontGhostsPacks*VECTOR_SIZE,REAL_ALIGNMENT) - NumFrontGhosts;

  // Length of a column with ghosts on both sides. Notice that the number of elements added
  // at the beginning could be more than NumFrontGhosts, in case there is padding.
  static constexpr int length = InitialPadding + NumFrontGhosts + PhysicalLength + NumBackGhosts;

  static constexpr int first_entry_idx = -InitialPadding - NumFrontGhosts;
};

// Short name for views of data with top and/or bottom ghosts.
// NOTE: ValueType is there only to allow const/nonconst. The underlying type should *always* be Real.
template<typename ValueType, int PhysicalLength, int NumFrontGhosts, int NumBackGhosts>
using GhostedColumn = Ranged<ExecViewUnmanaged<ValueType[GhostedColumnProps<PhysicalLength,NumFrontGhosts,NumBackGhosts>::length]>,
                             GhostedColumnProps<PhysicalLength,NumFrontGhosts,NumBackGhosts>::first_entry_idx>;

template<typename ValueType, int NumColumns, int PhysicalLength, int NumFrontGhosts, int NumBackGhosts>
using GhostedColumns = Ranged<ExecViewUnmanaged<ValueType[NumColumns][GhostedColumnProps<PhysicalLength,NumFrontGhosts,NumBackGhosts>::length]>,
                              0,GhostedColumnProps<PhysicalLength,NumFrontGhosts,NumBackGhosts>::first_entry_idx>;

struct PpmSizes {
public:

  // Number of ghost levels (possibly) used in the remap procedures
  static constexpr int NUM_GHOSTS = 2;

  // Length of a column with ghosts on both sides. Notice that the ghosts at the beginning could be more
  // than NUM_GHOSTS, in case there is padding.
  static constexpr int GHOSTED_COLUMN_LENGTH = GhostedColumnProps<NUM_PHYSICAL_LEV,NUM_GHOSTS,NUM_GHOSTS>::length;

  static constexpr int GHOSTED_COLUMN_FIRST_IDX = GhostedColumnProps<NUM_PHYSICAL_LEV,NUM_GHOSTS,NUM_GHOSTS>::first_entry_idx;

  // // Location of the last front ghost and the first back ghost
  // static constexpr int LAST_FRONT_GHOST = INITIAL_PADDING - 1;
  // static constexpr int FIRST_BACK_GHOST = NUM_PHYSICAL_LEV + INITIAL_PADDING;

  // // Location of the first and last entries of the column (after the last front ghost and before the first back ghost)
  // static constexpr int FIRST_COLUMN_ENTRY = LAST_FRONT_GHOST + 1;
  // static constexpr int LAST_COLUMN_ENTRY  = FIRST_BACK_GHOST - 1;

  // Layers old thickness (cell centered): needs ghost cells, on both boundaries
  static constexpr int DP_OLD_COLUMN_LENGTH = GHOSTED_COLUMN_LENGTH;

  // Layers new thickness (cell centered)
  static constexpr int DP_NEW_COLUMN_LENGTH = NUM_PHYSICAL_LEV;

  // Length of mass columns
  static constexpr int MASS_COLUMN_LENGTH = NUM_PHYSICAL_LEV + 2;

  // Length of grid spacing array
  static constexpr int PPMDX_COLUMN_LENGTH = NUM_PHYSICAL_LEV + 2;

  // Length of grid spacing array
  static constexpr int DMA_COLUMN_LENGTH = NUM_PHYSICAL_LEV + 2;

  // Cumulative sum of old grid thickness (interface centered): needs a dummy at the end as an absolute maximum
  static constexpr int P_OLD_COLUMN_LENGTH = NUM_INTERFACE_LEV + 1;

  // Cumulative sum of new grid thickness (interface centered)
  static constexpr int P_NEW_COLUMN_LENGTH = NUM_INTERFACE_LEV;

  // Grid spacing: needs one ghost on each side
  static constexpr int DX_COLUMN_LENGTH = NUM_PHYSICAL_LEV + 2;

  // Old cell values: two ghosts on each side
  static constexpr int AO_COLUMN_LENGTH = GHOSTED_COLUMN_LENGTH;

  // Interface values: needs one ghost on the front
  static constexpr int AI_COLUMN_LENGTH = NUM_PHYSICAL_LEV + 1;
};

// This struct should be used in static checks, as
//   is_ppm_bc<BLAH>::value;
// This is a copied-and-pasted version of what Kokkos does for all the is_*<T> checks.
template<typename T>
struct is_ppm_bc {
private: 
  template< typename , typename = std::true_type >
  struct have : std::false_type {};

  template< typename U >
  struct have<U,typename std::is_same<typename std::remove_cv<U>::type, 
                                      typename std::remove_cv<typename U::ppm_bc_type>::type 
             >::type> : std::true_type {};
public:
  enum { value = is_ppm_bc::template have<T>::value };
};

// Corresponds to remap alg = 1
struct PpmMirrored {
  // Tag this struct as a ppm boundary condition
  typedef PpmMirrored     ppm_bc_type;

  static constexpr int fortran_remap_alg = 1;

  KOKKOS_INLINE_FUNCTION
  static void apply_ppm_boundary(
      GhostedColumn<Real,NUM_PHYSICAL_LEV,NUM_GHOSTS,NUM_GHOSTS> /* cell_means */,
      ExecViewUnmanaged<Real[3][NUM_PHYSICAL_LEV]> /* parabola_coeffs */) {}

  KOKKOS_INLINE_FUNCTION
  static void fill_cell_means_gs(
      KernelVariables &kv,
      GhostedColumn<Real,NUM_PHYSICAL_LEV,NUM_GHOSTS,NUM_GHOSTS> cell_means) {
    Kokkos::parallel_for(Kokkos::ThreadVectorRange(kv.team, NUM_GHOSTS),
                         [&](const int &k_0) {
      cell_means(-1 - k_0) = cell_means(k_0);
      cell_means(NUM_PHYSICAL_LEV + k_0) = cell_means(NUM_PHYSICAL_LEV - 1 - k_0);
    }); // end ghost cell loop
  }

  static constexpr const char *name() { return "Mirrored PPM"; }
};

// Corresponds to remap alg = 2
struct PpmFixedParabola {
  // Tag this struct as a ppm boundary condition
  typedef PpmFixedParabola     ppm_bc_type;

  static constexpr int fortran_remap_alg = 2;

  KOKKOS_INLINE_FUNCTION
  static void apply_ppm_boundary(
      GhostedColumn<const Real,NUM_PHYSICAL_LEV,NUM_GHOSTS,NUM_GHOSTS> cell_means,
      ExecViewUnmanaged<Real[3][NUM_PHYSICAL_LEV]> parabola_coeffs) {
    parabola_coeffs(0, 0) = cell_means(0);
    parabola_coeffs(0, 1) = cell_means(1);

    parabola_coeffs(0, NUM_PHYSICAL_LEV - 2) = cell_means(NUM_PHYSICAL_LEV - 2);
    parabola_coeffs(0, NUM_PHYSICAL_LEV - 1) = cell_means(NUM_PHYSICAL_LEV - 1);

    parabola_coeffs(1, 0) = 0.0;
    parabola_coeffs(1, 1) = 0.0;
    parabola_coeffs(2, 0) = 0.0;
    parabola_coeffs(2, 1) = 0.0;

    parabola_coeffs(1, NUM_PHYSICAL_LEV - 2) = 0.0;
    parabola_coeffs(1, NUM_PHYSICAL_LEV - 1) = 0.0;
    parabola_coeffs(2, NUM_PHYSICAL_LEV - 2) = 0.0;
    parabola_coeffs(2, NUM_PHYSICAL_LEV - 1) = 0.0;
  }

  KOKKOS_INLINE_FUNCTION
  static void fill_cell_means_gs(
      KernelVariables &kv,
      GhostedColumn<Real,NUM_PHYSICAL_LEV,NUM_GHOSTS,NUM_GHOSTS> cell_means) {
    PpmMirrored::fill_cell_means_gs(kv, cell_means);
  }

  static constexpr const char *name() { return "Fixed Parabola PPM"; }
};

// Corresponds to remap alg = 3
struct PpmFixedMeans {
  // Tag this struct as a ppm boundary condition
  typedef PpmFixedMeans     ppm_bc_type;

  static constexpr int fortran_remap_alg = 3;

  KOKKOS_INLINE_FUNCTION
  static void apply_ppm_boundary(
      GhostedColumn<const Real, NUM_PHYSICAL_LEV,NUM_GHOSTS,NUM_GHOSTS> /* cell_means */,
      ExecViewUnmanaged<Real[3][NUM_PHYSICAL_LEV]> /* parabola_coeffs */) {}

  KOKKOS_INLINE_FUNCTION
  static void fill_cell_means_gs(
      KernelVariables &kv,
      GhostedColumn<Real,NUM_PHYSICAL_LEV,NUM_GHOSTS,NUM_GHOSTS> cell_means) {
    Kokkos::parallel_for(Kokkos::ThreadVectorRange(kv.team, NUM_GHOSTS),
                         [&](const int &k_0) {
      cell_means(- 1 - k_0) = cell_means(0);
      cell_means(NUM_PHYSICAL_LEV + k_0) = cell_means(NUM_PHYSICAL_LEV - 1 );
    }); // end ghost cell loop
  }

  static constexpr const char *name() { return "Fixed Means PPM"; }
};

// Piecewise Parabolic Method stencil
template <typename PpmBcType>
struct PpmVertRemap : public VertRemapAlg {
  // Checking that the template argument is one of the supported ones
  static_assert(is_ppm_bc<PpmBcType>::value, "PpmVertRemap requires a valid PPM boundary condition. "
                                             "If you wrote your own boundary condition class, tag it as such: add "
                                             "'typedef YOUR_CLASS_NAME  ppm_bc_type' to your class public types.\n");

  explicit PpmVertRemap(const int num_elems, const int num_remap)
      : dpo("dpo", num_elems), pio("pio", num_elems), pin("pin", num_elems),
        ppmdx("ppmdx", num_elems), z2("z2", num_elems), kid("kid", num_elems),
        ao("a0", get_num_concurrent_teams<ExecSpace>(num_elems * num_remap)),
        mass_o("mass_o",get_num_concurrent_teams<ExecSpace>(num_elems * num_remap)),
        m_dma("dma", get_num_concurrent_teams<ExecSpace>(num_elems * num_remap)),
        m_ai("ai", get_num_concurrent_teams<ExecSpace>(num_elems * num_remap)),
        m_parabola_coeffs("Coefficients for the interpolating parabola",
                          get_num_concurrent_teams<ExecSpace>(num_elems * num_remap))
  {}

  KOKKOS_INLINE_FUNCTION
  void compute_grids_phase(
      KernelVariables &kv,
      ExecViewUnmanaged<const Scalar[NP][NP][NUM_LEV]> src_layer_thickness,
      ExecViewUnmanaged<const Scalar[NP][NP][NUM_LEV]> tgt_layer_thickness)
      const {
    compute_partitions(kv, src_layer_thickness, tgt_layer_thickness);
    compute_integral_bounds(kv);
  }

  KOKKOS_INLINE_FUNCTION
  void compute_remap_phase(KernelVariables &kv,
                           ExecViewUnmanaged<Scalar[NP][NP][NUM_LEV]> remap_var)
      const {
    // From here, we loop over tracers for only those portions which depend on
    // tracer data, which includes PPM limiting and mass accumulation
    // More parallelism than we need here, maybe break it up?
    Kokkos::parallel_for(Kokkos::TeamThreadRange(kv.team, NP * NP),
                         [&](const int &loop_idx) {
      const int igp = loop_idx / NP;
      const int jgp = loop_idx % NP;

      auto dpo_point_ranged = viewAsRanged<PpmSizes::GHOSTED_COLUMN_FIRST_IDX>(Homme::subview(dpo, kv.ie, igp, jgp));
      auto ao_point_ranged = viewAsRanged<PpmSizes::GHOSTED_COLUMN_FIRST_IDX>(Homme::subview(dpo, kv.team_idx, igp, jgp));
      auto remap_var_point = reinterpretView<Real>(Homme::subview(remap_var,igp,jgp));

      Kokkos::parallel_for(Kokkos::ThreadVectorRange(kv.team, NUM_PHYSICAL_LEV),
                           [&](const int k) {
        ao_point_ranged(k) = remap_var_point(k) / dpo_point_ranged(k);
      });

      PpmBcType::fill_cell_means_gs(kv, viewAsRanged<PpmSizes::GHOSTED_COLUMN_FIRST_IDX>(Homme::subview(ao, kv.team_idx, igp, jgp)));

      Dispatch<ExecSpace>::parallel_scan(
          kv.team, NUM_PHYSICAL_LEV,
          [=](const int &k, Real &accumulator, const bool last) {
            // Accumulate the old mass up to old grid cell interface locations
            // to simplify integration during remapping. Also, divide out the
            // grid spacing so we're working with actual tracer values and can
            // conserve mass.
            if (last) {
              mass_o(kv.team_idx, igp, jgp, k + 1) = accumulator;
            }
            // const int ilevel = k / VECTOR_SIZE;
            // const int ivector = k % VECTOR_SIZE;
            // accumulator += remap_var(igp, jgp, ilevel)[ivector];
            accumulator += remap_var_point(k);
          });

      Kokkos::single(Kokkos::PerThread(kv.team), [&]() {
        const int ilast = (NUM_PHYSICAL_LEV - 1) / VECTOR_SIZE;
        const int vlast = (NUM_PHYSICAL_LEV - 1) % VECTOR_SIZE;
        mass_o(kv.team_idx, igp, jgp, NUM_PHYSICAL_LEV + 1) =
            mass_o(kv.team_idx, igp, jgp, NUM_PHYSICAL_LEV) +
            remap_var(igp, jgp, ilast)[vlast];
      });

      // Reflect the real values across the top and bottom boundaries into
      // the ghost cells

      // Computes a monotonic and conservative PPM reconstruction
      compute_ppm(kv,
                  viewAsRanged<PpmSizes::GHOSTED_COLUMN_FIRST_IDX>(Homme::subview(ao, kv.team_idx, igp, jgp)),
                  viewAsRanged<0,PpmSizes::GHOSTED_COLUMN_FIRST_IDX>(Homme::subview(ppmdx, kv.ie, igp, jgp)),
                  viewAsRanged<-1>(Homme::subview(m_dma, kv.team_idx, igp, jgp)),
                  viewAsRanged<-1>(Homme::subview(m_ai, kv.team_idx, igp, jgp)),
                  Homme::subview(m_parabola_coeffs, kv.team_idx, igp, jgp));
      compute_remap(kv, Homme::subview(kid, kv.ie, igp, jgp),
                    Homme::subview(z2, kv.ie, igp, jgp),
                    Homme::subview(m_parabola_coeffs, kv.team_idx, igp, jgp),
                    Homme::subview(mass_o, kv.team_idx, igp, jgp),
                    Homme::subview(dpo, kv.ie, igp, jgp),
                    Homme::subview(remap_var, igp, jgp));
    }); // End team thread range
    kv.team_barrier();
  }

  KOKKOS_FORCEINLINE_FUNCTION
  Real compute_mass(const Real sq_coeff, const Real lin_coeff,
                    const Real const_coeff, const Real prev_mass,
                    const Real prev_dp, const Real x2) const {
    // This remapping assumes we're starting from the left interface of an
    // old grid cell
    // In fact, we're usually integrating very little or almost all of the
    // cell in question
    const Real x1 = -0.5;
    const Real integral =
        integrate_parabola(sq_coeff, lin_coeff, const_coeff, x1, x2);
    const Real mass = prev_mass + integral * prev_dp;
    return mass;
  }


  KOKKOS_FORCEINLINE_FUNCTION
  void compute_mass(ExecViewUnmanaged<Real[NUM_PHYSICAL_LEV]> mass,
                    const int k,
                    ExecViewUnmanaged<const int[NUM_PHYSICAL_LEV]> k_id,
                    ExecViewUnmanaged<const Real[NUM_PHYSICAL_LEV]> integral_bounds,
                    ExecViewUnmanaged<const Real[3][NUM_PHYSICAL_LEV]> parabola_coeffs,
                    GhostedColumn<const Real,NUM_PHYSICAL_LEV,NUM_GHOSTS,NUM_GHOSTS> prev_dp) const {
                    // ExecViewUnmanaged<const Real[PpmSizes::DP_OLD_COLUMN_LENGTH]> prev_dp) const {

      const int kk_cur_lev = k_id(k);
      assert(kk_cur_lev + 1 >= k);
      assert(kk_cur_lev < parabola_coeffs.extent_int(1));

      const Real x2_cur_lev = integral_bounds(k);
    
      mass(k) = compute_mass(
          parabola_coeffs(2, kk_cur_lev), parabola_coeffs(1, kk_cur_lev),
          parabola_coeffs(0, kk_cur_lev), mass(kk_cur_lev + 1),
          prev_dp(kk_cur_lev), x2_cur_lev);
  }

  template <typename ExecSpaceType = ExecSpace>
  KOKKOS_INLINE_FUNCTION typename std::enable_if<
      !Homme::OnGpu<ExecSpaceType>::value, void>::type
  compute_remap(
      KernelVariables &kv, ExecViewUnmanaged<const int[NUM_PHYSICAL_LEV]> k_id,
      ExecViewUnmanaged<const Real[NUM_PHYSICAL_LEV]> integral_bounds,
      ExecViewUnmanaged<const Real[3][NUM_PHYSICAL_LEV]> parabola_coeffs,
      ExecViewUnmanaged<Real[PpmSizes::MASS_COLUMN_LENGTH]> mass,
      ExecViewUnmanaged<const Real[PpmSizes::DP_OLD_COLUMN_LENGTH]> prev_dp,
      ExecViewUnmanaged<Real[NUM_PHYSICAL_LEV]> remap_var) const {

    // Compute tracer values on the new grid by integrating from the old cell
    // bottom to the new cell interface to form a new grid mass accumulation.
    // Store the mass in the integral bounds for that level
    // Then take the difference between accumulation at successive interfaces
    // gives the mass inside each cell. Since Qdp is supposed to hold the full
    // mass this needs no normalization.
    Kokkos::parallel_for(Kokkos::ThreadVectorRange(kv.team, NUM_PHYSICAL_LEV),
                         [&](const int k) {
      const int kk_cur_lev = k_id(k);
      assert(kk_cur_lev + 1 >= k);
      assert(kk_cur_lev < parabola_coeffs.extent_int(1));

      const Real x2_cur_lev = integral_bounds(k);
      // Repurpose the mass buffer to store the new mass.
      // WARNING: This may not be thread safe in future architectures which
      //          use this level of parallelism!!!
      mass(k) = compute_mass(
          parabola_coeffs(2, kk_cur_lev), parabola_coeffs(1, kk_cur_lev),
          parabola_coeffs(0, kk_cur_lev), mass(kk_cur_lev + 1),
          prev_dp(kk_cur_lev), x2_cur_lev);
    });

    remap_var(0) = mass(0);
    Kokkos::parallel_for(Kokkos::ThreadVectorRange(kv.team, 1, NUM_PHYSICAL_LEV),
                         [&](const int k) {
      remap_var(k) = mass(k) - mass(k - 1);
    }); // k loop
  }

  template <typename ExecSpaceType = ExecSpace>
  KOKKOS_INLINE_FUNCTION typename std::enable_if<
      Homme::OnGpu<ExecSpaceType>::value, void>::type
  compute_remap(
      KernelVariables &kv, ExecViewUnmanaged<const int[NUM_PHYSICAL_LEV]> k_id,
      ExecViewUnmanaged<const Real[NUM_PHYSICAL_LEV]> integral_bounds,
      ExecViewUnmanaged<const Real[3][NUM_PHYSICAL_LEV]> parabola_coeffs,
      ExecViewUnmanaged<Real[PpmSizes::MASS_COLUMN_LENGTH]> prev_mass,
      GhostedColumn<const Real,NUM_PHYSICAL_LEV,NUM_GHOSTS,NUM_GHOSTS> prev_dp,
      ExecViewUnmanaged<Real[NUM_PHYSICAL_LEV]> remap_var) const {
    // This duplicates work, but the parallel gain on CUDA is >> 2
    Kokkos::parallel_for(Kokkos::ThreadVectorRange(kv.team, NUM_PHYSICAL_LEV),
                         [&](const int k) {
      const Real mass_1 =
          (k > 0)
              ? compute_mass(
                    parabola_coeffs(2, k_id(k - 1)),
                    parabola_coeffs(1, k_id(k - 1)),
                    parabola_coeffs(0, k_id(k - 1)), prev_mass(k_id(k - 1) + 1),
                    prev_dp(k_id(k - 1)),
                    integral_bounds(k - 1))
              : 0.0;

      const Real x2_cur_lev = integral_bounds(k);

      const int kk_cur_lev = k_id(k);
      assert(kk_cur_lev + 1 >= k);
      assert(kk_cur_lev < parabola_coeffs.extent_int(1));

      const Real mass_2 = compute_mass(
          parabola_coeffs(2, kk_cur_lev), parabola_coeffs(1, kk_cur_lev),
          parabola_coeffs(0, kk_cur_lev), prev_mass(kk_cur_lev + 1),
          prev_dp(kk_cur_lev), x2_cur_lev);

      remap_var(k) = mass_2 - mass_1;
    }); // k loop
  }

  KOKKOS_INLINE_FUNCTION
  void compute_grids(
      KernelVariables &kv,
      const GhostedColumn<const Real,NUM_PHYSICAL_LEV,NUM_GHOSTS,NUM_GHOSTS> dx,
      // const ExecViewUnmanaged<const Real[PpmSizes::DP_OLD_COLUMN_LENGTH]> dx,
      const ExecViewUnmanaged<Real[10][PpmSizes::PPMDX_COLUMN_LENGTH]> grids)
      const {

    // Calculate grid-based coefficients for stage 1 of compute_ppm
    Kokkos::parallel_for(Kokkos::ThreadVectorRange(kv.team,NUM_PHYSICAL_LEV + 2),
                         [&](const int j) {
      grids(0, j) = dx(j) / (dx(j-1) + dx(j) + dx(j+1));

      grids(1, j) = (2.0*dx(j-1) + dx(j)) / (dx(j+1) + dx(j));

      grids(2, j) = (dx(j) + 2.0*dx(j+1)) / (dx(j-1) + dx(j));
    });

    // Caculate grid-based coefficients for stage 2 of compute_ppm
    Kokkos::parallel_for(Kokkos::ThreadVectorRange(kv.team,NUM_PHYSICAL_LEV + 1),
                         [&](const int j) {
      grids(3, j) = dx(j) / (dx(j) + dx(j+1));

      grids(4, j) = 1.0 / (dx(j-1) + dx(j) + dx(j+1) + dx(j+2));

      grids(5, j) = (2.0 * dx(j+1) * dx(j)) / (dx(j) + dx(j+1));

      grids(6, j) = (dx(j-1) + dx(j)) / (2.0 * dx(j) + dx(j+1));

      grids(7, j) = (dx(j+2) + dx(j+1)) / (2.0 * dx(j+1) + dx(j));

      grids(8, j) = dx(j) * (dx(j-1) + dx(j)) / (2.0 * dx(j) + dx(j+1));

      grids(9, j) = dx(j+1) * (dx(j+1) + dx(j+2)) / (dx(j) + 2.0 * dx(j+1));
    });
  }

  KOKKOS_INLINE_FUNCTION
  void compute_ppm(
      KernelVariables &kv,
      // input  views
      GhostedColumn<const Real, NUM_PHYSICAL_LEV,NUM_GHOSTS,NUM_GHOSTS> cell_means,
      GhostedColumns<const Real,10,NUM_PHYSICAL_LEV,1,1> dx,
      // buffer views
      GhostedColumn<Real,NUM_PHYSICAL_LEV,1,1> dma,
      GhostedColumn<Real,NUM_PHYSICAL_LEV,1,0> ai,
      // result view
      ExecViewUnmanaged<Real[3][NUM_PHYSICAL_LEV]> parabola_coeffs) const {

    // Stage 1: Compute dma for each cell, allowing a 1-cell ghost stencil below and above the domain
    Kokkos::parallel_for(Kokkos::ThreadVectorRange(kv.team, -1,NUM_PHYSICAL_LEV + 1),
                         [&](const int j) {
      if ((cell_means(j + 1) - cell_means(j)) *
          (cell_means(j) - cell_means(j - 1)) > 0.0) {
        Real da =
            dx(0, j) * (dx(1, j) * (cell_means(j + 1) -
                                    cell_means(j)) +
                        dx(2, j) * (cell_means(j) -
                                    cell_means(j - 1)));

        dma(j) = min(fabs(da),
                     2.0 * fabs(cell_means(j) -
                                cell_means(j - 1)),
                     2.0 * fabs(cell_means(j + 1) -
                                cell_means(j))) *
                 copysign(1.0, da);
      } else {
        dma(j) = 0.0;
      }
    });

    // Stage 2: Compute ai for each cell interface in the physical domain
    Kokkos::parallel_for(
        Kokkos::ThreadVectorRange(kv.team, -1, NUM_PHYSICAL_LEV),
        [&](const int j) {
          ai(j) = cell_means(j) +
                  dx(3, j) * (cell_means(j + 1) - cell_means(j)) +
                  dx(4, j) * (dx(5, j) * (dx(6, j) - dx(7, j)) * (cell_means(j + 1) - cell_means(j)) -
                              dx(8, j) * dma(j + 1) + dx(9, j) * dma(j));
        });

    Kokkos::parallel_for(Kokkos::ThreadVectorRange(kv.team, NUM_PHYSICAL_LEV),
                         [&](const int j_prev) {
      const int j = j_prev + 1;
      Real al = ai(j - 1);
      Real ar = ai(j);
      if ((ar - cell_means(j - 1)) *
              (cell_means(j - 1) - al) <=
          0.) {
        al = cell_means(j - 1);
        ar = cell_means(j - 1);
      }
      if ((ar - al) * (cell_means(j - 1) - (al + ar) / 2.0) >
          (ar - al) * (ar - al) / 6.0) {
        al = 3.0 * cell_means(j - 1) - 2.0 * ar;
      }
      if ((ar - al) * (cell_means(j - 1) - (al + ar) / 2.0) <
          -(ar - al) * (ar - al) / 6.0) {
        ar = 3.0 * cell_means(j - 1) - 2.0 * al;
      }

      // Computed these coefficients from the edge values
      // and cell mean in Maple. Assumes normalized
      // coordinates: xi=(x-x0)/dx

      assert(parabola_coeffs.data() != nullptr);
      assert(j - 1 < parabola_coeffs.extent_int(1));
      assert(2 < parabola_coeffs.extent_int(0));

      parabola_coeffs(0, j - 1) =
          1.5 * cell_means(j - 1) - (al + ar) / 4.0;
      parabola_coeffs(1, j - 1) = ar - al;
      parabola_coeffs(2, j - 1) =
          3.0 * (-2.0 * cell_means(j - 1) + (al + ar));
    });

    Kokkos::single(Kokkos::PerThread(kv.team), [&]() {
      PpmBcType::apply_ppm_boundary(cell_means, parabola_coeffs);
    });
  }

  KOKKOS_INLINE_FUNCTION
  void compute_partitions(
      KernelVariables &kv,
      ExecViewUnmanaged<const Scalar[NP][NP][NUM_LEV]> src_layer_thickness,
      ExecViewUnmanaged<const Scalar[NP][NP][NUM_LEV]> tgt_layer_thickness)
      const {
    Kokkos::parallel_for(Kokkos::TeamThreadRange(kv.team, NP * NP),
                         [&](const int &loop_idx) {
      const int igp = loop_idx / NP;
      const int jgp = loop_idx % NP;
      ExecViewUnmanaged<Real[NUM_INTERFACE_LEV+1]> pt_pio =
          Homme::subview(pio, kv.ie, igp, jgp);
      ExecViewUnmanaged<Real[NUM_INTERFACE_LEV]> pt_pin =
          Homme::subview(pin, kv.ie, igp, jgp);
      ExecViewUnmanaged<const Scalar[NUM_LEV]> pt_src_thickness =
          Homme::subview(src_layer_thickness, igp, jgp);
      ExecViewUnmanaged<const Scalar[NUM_LEV]> pt_tgt_thickness =
          Homme::subview(tgt_layer_thickness, igp, jgp);

      Dispatch<ExecSpace>::parallel_scan(
          kv.team, NUM_PHYSICAL_LEV,
          [=](const int &level, Real &accumulator, const bool last) {
            if (last) {
              pt_pio(level) = accumulator;
            }
            const int ilev = level / VECTOR_SIZE;
            const int vlev = level % VECTOR_SIZE;
            accumulator += pt_src_thickness(ilev)[vlev];
          });
      Dispatch<ExecSpace>::parallel_scan(
          kv.team, NUM_PHYSICAL_LEV,
          [=](const int &level, Real &accumulator, const bool last) {
            if (last) {
              pt_pin(level) = accumulator;
            }
            const int ilev = level / VECTOR_SIZE;
            const int vlev = level % VECTOR_SIZE;
            accumulator += pt_tgt_thickness(ilev)[vlev];
          });

      Kokkos::single(Kokkos::PerThread(kv.team), [&]() {
        const int ilast = (NUM_PHYSICAL_LEV - 1) / VECTOR_SIZE;
        const int vlast = (NUM_PHYSICAL_LEV - 1) % VECTOR_SIZE;
        pt_pio(NUM_PHYSICAL_LEV) =
            pt_pio(NUM_PHYSICAL_LEV - 1) + pt_src_thickness(ilast)[vlast];
        pt_pin(NUM_PHYSICAL_LEV) =
            pt_pin(NUM_PHYSICAL_LEV - 1) + pt_tgt_thickness(ilast)[vlast];
        // This is here to allow an entire block of k
        // threads to run in the remapping phase. It makes
        // sure there's an old interface value below the
        // domain that is larger.
        assert(fabs(pio(kv.ie, igp, jgp, NUM_PHYSICAL_LEV) -
                    pin(kv.ie, igp, jgp, NUM_PHYSICAL_LEV)) < 1.0);

        pt_pio(NUM_INTERFACE_LEV) =
            pt_pio(NUM_INTERFACE_LEV-1) + 1.0;

        // The total mass in a column does not change.
        // Therefore, the pressure of that mass cannot
        // either.
        pt_pin(NUM_PHYSICAL_LEV) = pt_pio(NUM_PHYSICAL_LEV);
      });

      auto dpo_col = GhostedColumn<Real,NUM_PHYSICAL_LEV,NUM_GHOSTS,NUM_GHOSTS>
                      (Homme::subview(dpo,kv.ie,igp,jgp));
      Kokkos::parallel_for(Kokkos::ThreadVectorRange(kv.team, NUM_PHYSICAL_LEV),
                           [&](const int &k) {
        int ilevel = k / VECTOR_SIZE;
        int ivector = k % VECTOR_SIZE;
        // dpo(kv.ie, igp, jgp, k + PpmSizes::INITIAL_PADDING) =
        dpo_col(k) = src_layer_thickness(igp, jgp, ilevel)[ivector];
      });
    });
    kv.team_barrier();
    // Fill in the ghost regions with mirrored values.
    // if vert_remap_q_alg is defined, this is of no
    // consequence.
    // Note that the range of k makes this completely parallel,
    // without any data dependencies
    Kokkos::parallel_for(Kokkos::TeamThreadRange(kv.team, NP * NP),
                         [&](const int &loop_idx) {
      const int igp = loop_idx / NP;
      const int jgp = loop_idx % NP;
      auto dpo_col = GhostedColumn<Real,NUM_PHYSICAL_LEV,NUM_GHOSTS,NUM_GHOSTS>
                      (Homme::subview(dpo,kv.ie,igp,jgp));
      Kokkos::parallel_for(Kokkos::ThreadVectorRange(kv.team, NUM_GHOSTS),
                           [&](const int &k) {
        // dpo(kv.ie, igp, jgp, PpmSizes::INITIAL_PADDING - 1 - k) =
        //     dpo(kv.ie, igp, jgp, k + PpmSizes::INITIAL_PADDING);
        // dpo(kv.ie, igp, jgp,
        //     NUM_PHYSICAL_LEV + PpmSizes::INITIAL_PADDING + k) =
        //     dpo(kv.ie, igp, jgp,
        //         NUM_PHYSICAL_LEV + PpmSizes::INITIAL_PADDING - 1 - k);
        dpo_col( - 1 - k) = dpo_col(k);
        dpo_col(NUM_PHYSICAL_LEV + k) = dpo_col(NUM_PHYSICAL_LEV - 1 - k);
      });
    });
    kv.team_barrier();
  }

  KOKKOS_INLINE_FUNCTION
  void compute_integral_bounds(KernelVariables &kv) const {
    Kokkos::parallel_for(Kokkos::TeamThreadRange(kv.team, NP * NP),
                         [&](const int &loop_idx) {
      const int igp = loop_idx / NP;
      const int jgp = loop_idx % NP;
      auto point_dpo = GhostedColumn<Real,NUM_PHYSICAL_LEV,NUM_GHOSTS,NUM_GHOSTS>
                      (Homme::subview(dpo,kv.ie,igp,jgp));
      Kokkos::parallel_for(Kokkos::ThreadVectorRange(kv.team, NUM_PHYSICAL_LEV),
                           [&](const int k) {
        // Compute remapping intervals once for all
        // tracers. Find the old grid cell index in which
        // the k-th new cell interface resides. Then
        // integrate from the bottom of that old cell to
        // the new interface location. In practice, the
        // grid never deforms past one cell, so the search
        // can be simplified by this. Also, the interval
        // of integration is usually of magnitude close to
        // zero or close to dpo because of minimial
        // deformation. Numerous tests confirmed that the
        // bottom and top of the grids match to machine
        // precision, so set them equal to each other.
        int kk = k + 1;
        // This reduces the work required to find the index where this
        // fails at, and is typically less than NUM_PHYSICAL_LEV^2 Since
        // the top bounds match anyway, the value of the coefficients
        // don't matter, so enforcing kk <= NUM_PHYSICAL_LEV doesn't
        // affect anything important
        //
        // Note that because we set
        // pio(:, :, :, NUM_PHYSICAL_LEV + 1) = pio(:, :, :,
        // NUM_PHYSICAL_LEV) + 1.0 and pin(:, :, :, NUM_PHYSICAL_LEV) =
        // pio(:, :, :, NUM_PHYSICAL_LEV) this loop ensures kk <=
        // NUM_PHYSICAL_LEV + 2 Furthermore, since we set pio(:, :, :,
        // 0) = 0.0 and pin(:, :, :, 0) = 0.0 kk must be incremented at
        // least once
        assert(pio(kv.ie, igp, jgp, PpmSizes::P_OLD_COLUMN_LENGTH - 1) >
               pin(kv.ie, igp, jgp, k + 1));
        while (pio(kv.ie, igp, jgp, kk - 1) <= pin(kv.ie, igp, jgp, k + 1)) {
          kk++;
          assert(kk - 1 < pio.extent_int(3));
        }

        kk--;
        // This is to keep the indices in bounds.
        if (kk == PpmSizes::P_NEW_COLUMN_LENGTH) {
          kk = PpmSizes::P_NEW_COLUMN_LENGTH - 1;
        }
        // kk is now the cell index we're integrating over.

        // Save kk for reuse
        kid(kv.ie, igp, jgp, k) = kk - 1;
        // PPM interpolants are normalized to an independent coordinate
        // domain
        // [-0.5, 0.5].
        assert(kk >= k);
        assert(kk < pio.extent_int(3));
        z2(kv.ie, igp, jgp, k) =
            (pin(kv.ie, igp, jgp, k + 1) -
             (pio(kv.ie, igp, jgp, kk - 1) + pio(kv.ie, igp, jgp, kk)) * 0.5) /
            // dpo(kv.ie, igp, jgp, kk + 1 + PpmSizes::INITIAL_PADDING - NUM_GHOSTS);
            point_dpo(kk - 1);
      });

      ExecViewUnmanaged<Real[10][PpmSizes::PPMDX_COLUMN_LENGTH]> point_ppmdx =
          Homme::subview(ppmdx, kv.ie, igp, jgp);
      compute_grids(kv, point_dpo, point_ppmdx);
    });
  }

  KOKKOS_FORCEINLINE_FUNCTION Real
  integrate_parabola(const Real sq_coeff, const Real lin_coeff,
                     const Real const_coeff, Real x1, Real x2) const {
    return (const_coeff * (x2 - x1) + lin_coeff * (x2 * x2 - x1 * x1) / 2.0) +
           sq_coeff * (x2 * x2 * x2 - x1 * x1 * x1) / 3.0;
  }

  ExecViewManaged<Real * [NP][NP][PpmSizes::DP_OLD_COLUMN_LENGTH]> dpo;
  // pio corresponds to the points in each layer of the source (old) layer thickness
  ExecViewManaged<Real * [NP][NP][PpmSizes::P_OLD_COLUMN_LENGTH]> pio;
  // pin corresponds to the points in each layer of the target (new) layer thickness
  ExecViewManaged<Real * [NP][NP][PpmSizes::P_NEW_COLUMN_LENGTH]> pin;
  ExecViewManaged<Real * [NP][NP][10][PpmSizes::PPMDX_COLUMN_LENGTH]> ppmdx;
  ExecViewManaged<Real * [NP][NP][NUM_PHYSICAL_LEV]> z2;
  ExecViewManaged<int * [NP][NP][NUM_PHYSICAL_LEV]> kid;

  ExecViewManaged<Real * [NP][NP][PpmSizes::AO_COLUMN_LENGTH]>      ao;
  ExecViewManaged<Real * [NP][NP][PpmSizes::MASS_COLUMN_LENGTH]>    mass_o;
  ExecViewManaged<Real * [NP][NP][PpmSizes::DMA_COLUMN_LENGTH]>     m_dma;
  ExecViewManaged<Real * [NP][NP][PpmSizes::AI_COLUMN_LENGTH]>      m_ai;
  ExecViewManaged<Real * [NP][NP][3][NUM_PHYSICAL_LEV]>             m_parabola_coeffs;
};

} // namespace Ppm
} // namespace Remap
} // namespace Homme

#endif // HOMMEXX_PPM_REMAP_HPP
