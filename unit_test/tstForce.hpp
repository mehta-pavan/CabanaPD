/****************************************************************************
 * Copyright (c) 2022 by Oak Ridge National Laboratory                      *
 * All rights reserved.                                                     *
 *                                                                          *
 * This file is part of CabanaPD. CabanaPD is distributed under a           *
 * BSD 3-clause license. For the licensing terms see the LICENSE file in    *
 * the top-level directory.                                                 *
 *                                                                          *
 * SPDX-License-Identifier: BSD-3-Clause                                    *
 ****************************************************************************/

/****************************************************************************
 * Copyright (c) 2018-2021 by the Cabana authors                            *
 * All rights reserved.                                                     *
 *                                                                          *
 * This file is part of the Cabana library. Cabana is distributed under a   *
 * BSD 3-clause license. For the licensing terms see the LICENSE file in    *
 * the top-level directory.                                                 *
 *                                                                          *
 * SPDX-License-Identifier: BSD-3-Clause                                    *
 ****************************************************************************/

#include <gtest/gtest.h>

#include <Cabana_Core.hpp>
#include <Kokkos_Core.hpp>

#include <CabanaPD_config.hpp>

#include <CabanaPD_Force.hpp>
#include <CabanaPD_Input.hpp>
#include <CabanaPD_Particles.hpp>

namespace Test
{

struct LinearTag
{
};
struct QuadraticTag
{
};

//---------------------------------------------------------------------------//
// Note: all of these reference calculations assume uniform volume and a full
// particle neighborhood.
//---------------------------------------------------------------------------//

//---------------------------------------------------------------------------//
// Get the PMB strain energy density (at the center point).
double computeReferenceStrainEnergyDensity( CabanaPD::PMBModel model,
                                            const int m, const double s0 )
{
    double W = 0.0;
    double dx = model.delta / m;
    double vol = dx * dx * dx;
    for ( int i = -m; i < m + 1; ++i )
        for ( int j = -m; j < m + 1; ++j )
            for ( int k = -m; k < m + 1; ++k )
            {
                double xi_x = dx * i;
                double xi_y = dx * j;
                double xi_z = dx * k;
                double xi = sqrt( xi_x * xi_x + xi_y * xi_y + xi_z * xi_z );

                if ( xi > 0.0 && xi < model.delta + 1e-14 )
                {
                    W += 0.25 * model.c * s0 * s0 * xi * vol;
                    // std::cout << W << " W\n";
                }
            }
    return W;
}

// Get the PMB force (at one point).
double computeReferenceForceX( CabanaPD::PMBModel model, const int m,
                               const double s0 )
{
    double f = 0.0;
    double dx = model.delta / m;
    double vol = dx * dx * dx;
    for ( int i = -m; i < m + 1; ++i )
        for ( int j = -m; j < m + 1; ++j )
            for ( int k = -m; k < m + 1; ++k )
            {
                double xi_x = dx * i;
                double xi_y = dx * j;
                double xi_z = dx * k;
                double eta_u = s0 * xi_x * xi_x;
                double rx = xi_x + eta_u;
                double ry = xi_y;
                double rz = xi_z;
                // Assumes zero y/z components.
                double r = sqrt( rx * rx + ry * ry + rz * rz );
                double xi = sqrt( xi_x * xi_x + xi_y * xi_y + xi_z * xi_z );

                if ( xi > 0.0 && xi < model.delta + 1e-14 )
                    f += model.c * s0 * vol * rx / r;
            }
    return f;
}

// Get the LPS strain energy density (at one point).
double computeReferenceStrainEnergyDensity( CabanaPD::LPSModel model,
                                            const int m, const double s0 )
{
    double W = 0.0;
    double dx = model.delta / m;
    double vol = dx * dx * dx;

    // Weighted volume.
    double weighted_volume = 0.0;
    for ( int i = -m; i < m + 1; ++i )
        for ( int j = -m; j < m + 1; ++j )
            for ( int k = -m; k < m + 1; ++k )
            {
                double xi_x = dx * i;
                double xi_y = dx * j;
                double xi_z = dx * k;
                double xi = sqrt( xi_x * xi_x + xi_y * xi_y + xi_z * xi_z );
                if ( xi > 0.0 && xi < model.delta + 1e-14 )
                    weighted_volume += 1.0 / xi * xi * xi * vol;
            }
    // Dilatation and count neighbors.
    double theta = 0.0;
    double num_neighbors = 0.0;
    for ( int i = -m; i < m + 1; ++i )
        for ( int j = -m; j < m + 1; ++j )
            for ( int k = -m; k < m + 1; ++k )
            {
                double xi_x = dx * i;
                double xi_y = dx * j;
                double xi_z = dx * k;
                double xi = sqrt( xi_x * xi_x + xi_y * xi_y + xi_z * xi_z );

                if ( xi > 0.0 && xi < model.delta + 1e-14 )
                {
                    theta +=
                        3.0 / weighted_volume * 1.0 / xi * s0 * xi * xi * vol;

                    num_neighbors += 1.0;
                }
            }
    // Strain energy.
    for ( int i = -m; i < m + 1; ++i )
        for ( int j = -m; j < m + 1; ++j )
            for ( int k = -m; k < m + 1; ++k )
            {
                double xi_x = dx * i;
                double xi_y = dx * j;
                double xi_z = dx * k;
                double xi = sqrt( xi_x * xi_x + xi_y * xi_y + xi_z * xi_z );

                if ( xi > 0.0 && xi < model.delta + 1e-14 )
                {
                    W += ( 1.0 / num_neighbors ) * 0.5 * model.theta_coeff /
                             3.0 * ( theta * theta ) +
                         0.5 * ( model.s_coeff / weighted_volume ) * 1.0 / xi *
                             s0 * s0 * xi * xi * vol;
                }
            }
    return W;
}

template <class ModelType>
auto createParticles( LinearTag, ModelType model, const double s0 )
{
    std::array<double, 3> box_min = { -1.0, -1.0, -1.0 };
    std::array<double, 3> box_max = { 1.0, 1.0, 1.0 };
    double delta = model.delta;
    int nc = static_cast<int>( ( box_max[0] - box_min[0] ) / delta );
    std::array<int, 3> num_cells = { nc, nc, nc };

    // Create particles based on the mesh.
    using ptype = CabanaPD::Particles<TEST_MEMSPACE>;
    ptype particles( TEST_EXECSPACE{}, box_min, box_max, num_cells, 0 );

    auto x = particles.slice_x();
    auto u = particles.slice_u();
    auto v = particles.slice_v();
    auto init_functor = KOKKOS_LAMBDA( const int pid )
    {
        for ( int d = 0; d < 3; d++ )
        {
            u( pid, d ) = s0 * x( pid, d );
            v( pid, d ) = 0.0;
        }
    };
    particles.update_particles( TEST_EXECSPACE{}, init_functor );
    return particles;
}

// Currently unused.
template <class ModelType>
auto createParticles( QuadraticTag, ModelType model, const double s0 )
{
    std::array<double, 3> box_min = { -1.0, -1.0, -1.0 };
    std::array<double, 3> box_max = { 1.0, 1.0, 1.0 };
    int nc = static_cast<int>( ( box_max[0] - box_min[0] ) / model.delta );
    std::array<int, 3> num_cells = { nc, nc, nc };

    // Create particles based on the mesh.
    using ptype = CabanaPD::Particles<TEST_MEMSPACE>;
    ptype particles( TEST_EXECSPACE{}, box_min, box_max, num_cells, 0 );

    auto x = particles.slice_x();
    auto u = particles.slice_u();
    auto v = particles.slice_v();
    auto init_functor = KOKKOS_LAMBDA( const int pid )
    {
        for ( int d = 0; d < 3; d++ )
        {
            u( pid, d ) = 0.0;
            v( pid, d ) = 0.0;
        }
        u( pid, 0 ) = s0 * x( pid, 0 ) * x( pid, 0 );
    };
    particles.update_particles( TEST_EXECSPACE{}, init_functor );
    return particles;
}

template <class HostParticleType, class TestTag, class ModelType>
void checkResults( HostParticleType aosoa_host, double local_min[3],
                   double local_max[3], TestTag test_tag, ModelType model,
                   const double s0, const int boundary_width, const double Phi )
{
    double delta = model.delta;
    // double ref_f = computeReferenceForceX( model, 1, s0 );
    double ref_W = computeReferenceStrainEnergyDensity( model, 1, s0 );
    double ref_Phi = 0.0;
    auto f_host = Cabana::slice<0>( aosoa_host );
    auto x_host = Cabana::slice<1>( aosoa_host );
    auto W_host = Cabana::slice<2>( aosoa_host );
    auto vol_host = Cabana::slice<3>( aosoa_host );
    auto theta_host = Cabana::slice<4>( aosoa_host );
    // Check the results: avoid the system boundary for per particle values.
    for ( std::size_t p = 0; p < aosoa_host.size(); ++p )
    {
        if ( x_host( p, 0 ) > local_min[0] + delta * boundary_width &&
             x_host( p, 0 ) < local_max[0] - delta * boundary_width &&
             x_host( p, 1 ) > local_min[1] + delta * boundary_width &&
             x_host( p, 1 ) < local_max[1] - delta * boundary_width &&
             x_host( p, 2 ) > local_min[2] + delta * boundary_width &&
             x_host( p, 2 ) < local_max[2] - delta * boundary_width )
        {
            checkParticle( test_tag, model, s0, f_host( p, 0 ), f_host( p, 1 ),
                           f_host( p, 2 ), -1, W_host( p ), ref_W );
        }
        checkTheta( model, test_tag, s0, theta_host( p ) );

        // Check total sum of strain energy matches per particle sum.
        ref_Phi += W_host( p ) * vol_host( p );
    }

    EXPECT_NEAR( Phi, ref_Phi, 1e-5 );
}

template <class ModelType>
void checkParticle( LinearTag tag, ModelType model, const double s0,
                    const double fx, const double fy, const double fz,
                    const double, const double W, const double ref_W )
{
    EXPECT_LE( fx, 1e-13 );
    EXPECT_LE( fy, 1e-13 );
    EXPECT_LE( fz, 1e-13 );

    // Check strain energy (all should be equal for fixed stretch).
    EXPECT_FLOAT_EQ( W, ref_W );

    // Check energy with analytical value (not very close for small m).
    checkAnalyticalStrainEnergy( tag, model, s0, W );
}

// Currently unused.
template <class ModelType>
void checkParticle( QuadraticTag tag, ModelType model, const double s0,
                    const double fx, const double fy, const double fz,
                    const double ref_f, const double, const double )
{
    // Check force in x with discretized result.
    EXPECT_FLOAT_EQ( fx, ref_f );

    // Check force in x with analytical value (not very close for small m).
    checkAnalyticalForce( tag, model, s0, fx );

    // Check force: other components should be zero.
    EXPECT_LE( fy, 1e-13 );
    EXPECT_LE( fz, 1e-13 );
}

// Currently unused.
void checkAnalyticalStrainEnergy( LinearTag, CabanaPD::PMBModel model,
                                  const double s0, const double W )
{
    // Very large error for small m. This is basically a check for order of
    // magnitude.
    double threshold = W * 0.50;
    double analytical_W = 9.0 / 2.0 * model.K * s0 * s0;
    EXPECT_NEAR( W, analytical_W, threshold );
}

// Currently unused.
void checkAnalyticalStrainEnergy( LinearTag, CabanaPD::LPSModel, const double,
                                  const double )
{
    // FIXME: add check for LPS
}

// Currently unused.
void checkAnalyticalForce( QuadraticTag, CabanaPD::PMBModel model,
                           const double s0, const double fx )
{
    double threshold = fx * 0.50;
    double analytical_f = 18.0 / 5.0 * model.K * s0;
    EXPECT_NEAR( fx, analytical_f, threshold );
}

// Currently unused.
void checkAnalyticalForce( QuadraticTag, CabanaPD::LPSModel model,
                           const double s0, const double fx )
{
    double threshold = fx * 0.50;
    double analytical_f = 2.0 * ( model.K + 4.0 / 3.0 * model.G ) * s0;
    EXPECT_NEAR( fx, analytical_f, threshold );
}

void checkTheta( CabanaPD::PMBModel, LinearTag, const double,
                 const double theta )
{
    EXPECT_FLOAT_EQ( 0.0, theta );
}

void checkTheta( CabanaPD::LPSModel, LinearTag, const double s0,
                 const double theta )
{
    EXPECT_FLOAT_EQ( 3 * s0, theta );
}

void checkTheta( CabanaPD::PMBModel, QuadraticTag, const double, const double )
{
}

void checkTheta( CabanaPD::LPSModel, QuadraticTag, const double, const double )
{
}

//---------------------------------------------------------------------------//
template <class ModelType, class TestTag>
void testForce( ModelType model, const double boundary_width,
                const TestTag test_tag, const double s0 )
{
    auto particles = createParticles( test_tag, model, s0 );

    // This needs to exactly match the mesh spacing to compare with the single
    // particle calculation.
    CabanaPD::Force<TEST_EXECSPACE, ModelType> force( true, model );

    double mesh_min[3] = { particles.ghost_mesh_lo[0],
                           particles.ghost_mesh_lo[1],
                           particles.ghost_mesh_lo[2] };
    double mesh_max[3] = { particles.ghost_mesh_hi[0],
                           particles.ghost_mesh_hi[1],
                           particles.ghost_mesh_hi[2] };
    using verlet_list =
        Cabana::VerletList<TEST_MEMSPACE, Cabana::FullNeighborTag,
                           Cabana::VerletLayout2D, Cabana::TeamOpTag>;
    // Add to delta to make sure neighbors are found.
    auto x = particles.slice_x();
    verlet_list neigh_list( x, 0, particles.n_local, model.delta + 1e-14, 1.0,
                            mesh_min, mesh_max );

    auto f = particles.slice_f();
    auto W = particles.slice_W();
    auto vol = particles.slice_vol();
    auto theta = particles.slice_theta();
    auto m = particles.slice_m();
    force.initialize( particles, neigh_list, Cabana::SerialOpTag() );
    compute_force( force, particles, neigh_list, Cabana::SerialOpTag() );

    auto Phi =
        compute_energy( force, particles, neigh_list, Cabana::SerialOpTag() );

    // Make a copy of final results on the host
    std::size_t num_particle = x.size();
    using HostAoSoA = Cabana::AoSoA<
        Cabana::MemberTypes<double[3], double[3], double, double, double>,
        Kokkos::HostSpace>;
    HostAoSoA aosoa_host( "host_aosoa", num_particle );
    auto f_host = Cabana::slice<0>( aosoa_host );
    auto x_host = Cabana::slice<1>( aosoa_host );
    auto W_host = Cabana::slice<2>( aosoa_host );
    auto vol_host = Cabana::slice<3>( aosoa_host );
    auto theta_host = Cabana::slice<4>( aosoa_host );
    Cabana::deep_copy( f_host, f );
    Cabana::deep_copy( x_host, x );
    Cabana::deep_copy( W_host, W );
    Cabana::deep_copy( vol_host, vol );
    Cabana::deep_copy( theta_host, theta );

    double local_min[3] = { particles.local_mesh_lo[0],
                            particles.local_mesh_lo[1],
                            particles.local_mesh_lo[2] };
    double local_max[3] = { particles.local_mesh_hi[0],
                            particles.local_mesh_hi[1],
                            particles.local_mesh_hi[2] };

    checkResults( aosoa_host, local_min, local_max, test_tag, model, s0,
                  boundary_width, Phi );
}

//---------------------------------------------------------------------------//
// TESTS
//---------------------------------------------------------------------------//
// FIXME: Improvements
//  1. Increase m
//  2. Add single point force check with quadratic or check all neighbor
//     contributions to force
TEST( TEST_CATEGORY, test_force_pmb )
{
    double delta = 2.0 / 15.0;
    double K = 1.0;
    CabanaPD::PMBModel model( delta, K );
    testForce( model, 1.1, LinearTag{}, 0.1 );
}
TEST( TEST_CATEGORY, test_force_linear_pmb )
{
    double delta = 2.0 / 15.0;
    double K = 1.0;
    CabanaPD::LinearPMBModel model( delta, K );
    testForce( model, 1.1, LinearTag{}, 0.1 );
}
TEST( TEST_CATEGORY, test_force_lps )
{
    double delta = 2.0 / 15.0;
    double K = 1.0;
    double G = 0.5;
    CabanaPD::LPSModel model( delta, K, G );
    testForce( model, 2.1, LinearTag{}, 0.1 );
}
TEST( TEST_CATEGORY, test_force_linear_lps )
{
    double delta = 2.0 / 15.0;
    double K = 1.0;
    double G = 3 / 5. * K;
    CabanaPD::LinearLPSModel model( delta, K, G );
    testForce( model, 2.1, LinearTag{}, 0.1 );
}
} // end namespace Test
