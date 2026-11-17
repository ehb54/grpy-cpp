// test_eta_zeta.cpp -- checks core::intrinsic_fixed (the fixed-body intrinsic viscosity
// about the viscosity centre) against a direct evaluation, on touching-bead shapes and
// random clusters.
//
//   1. the closed-form minimum equals (1/10) tr ADD with T rebuilt about the centre it
//      returns (hydro() called with that point as the reference);
//   2. moving the centre off that point in any direction raises (1/10) tr ADD;
//   3. the result is the same whatever the model's position (it is translated);
//   4. prints, for each shape, eta_zeta / eta_0 and two neighbours of it:
//        eta_oo / eta_0          (the body free to translate and rotate)
//        eta_tfree / eta_0       (free to translate, held against rotation)
//
// Exit status 0 when 1-3 hold to 1e-9 relative.
//
// Copyright (C) 2026 the UltraScan project. Original work -- nothing here is translated
// from GRPY.f -- distributed under the GPLv3 as part of this program. See LICENSE.
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>
#include "grpy_core.hpp"

using namespace core;
using Eigen::Matrix;

namespace {

std::vector<Bead> shape( const std::string& s ) {
   std::vector<Bead> b;
   auto add = [ & ]( double x, double y, double z ) { b.push_back( { x, y, z, 1.0, 1.0 } ); };
   const double pi = 4 * std::atan( 1.0 );
   if ( s == "L2" ) { add( -1, 0, 0 ); add( 1, 0, 0 ); }
   if ( s == "L5" ) { for ( int i = 0; i < 5; ++i ) add( 2.0 * i, 0, 0 ); }
   if ( s == "R3" || s == "R4" ) {
      int n = s == "R3" ? 3 : 4;
      double R = 1.0 / std::sin( pi / n );
      for ( int i = 0; i < n; ++i ) {
         add( R * std::cos( 2 * pi * i / n ), R * std::sin( 2 * pi * i / n ), 0 );
      }
   }
   if ( s == "P4" ) {
      double c = 1.0 / std::sqrt( 2.0 );
      add( c, c, c ); add( c, -c, -c ); add( -c, c, -c ); add( -c, -c, c );
   }
   if ( s == "P6" ) {
      double c = std::sqrt( 2.0 );
      add( c, 0, 0 ); add( -c, 0, 0 ); add( 0, c, 0 ); add( 0, -c, 0 ); add( 0, 0, c ); add( 0, 0, -c );
   }
   if ( s == "P8" ) {
      for ( int i = 0; i < 8; ++i ) {
         add( i & 1 ? 1 : -1, i & 2 ? 1 : -1, i & 4 ? 1 : -1 );
      }
   }
   if ( s == "L3bent" ) { add( 0, 0, 0 ); add( 2, 0, 0 ); add( 0, 2, 0 ); }   // an L: no symmetry centre
   if ( s == "rand20" ) {   // fixed pseudo-random, unequal radii, non-overlapping
      unsigned u = 12345;
      auto rnd = [ & ]() { u = u * 1103515245u + 12345u; return ( ( u >> 8 ) & 0xffff ) / 65535.0; };
      while ( b.size() < 20 ) {
         double r = 0.6 + 0.8 * rnd(), x = 12 * rnd(), y = 9 * rnd(), z = 6 * rnd();
         bool ok = true;
         for ( auto& o : b ) {
            double dd = std::sqrt( ( x-o.x )*( x-o.x ) + ( y-o.y )*( y-o.y ) + ( z-o.z )*( z-o.z ) );
            if ( dd < r + o.r + 0.05 ) { ok = false; break; }
         }
         if ( ok ) {
            b.push_back( { x, y, z, r, 1.0 } );
         }
      }
   }
   if ( s == "randinv" ) {   // centrosymmetric and nothing else: 10 random beads plus their inversions
      unsigned u = 777;
      auto rnd = [ & ]() { u = u * 1103515245u + 12345u; return ( ( u >> 8 ) & 0xffff ) / 65535.0; };
      while ( b.size() < 20 ) {
         double r = 0.6 + 0.8 * rnd(), x = 1 + 8 * rnd(), y = -4 + 8 * rnd(), z = -3 + 6 * rnd();
         bool ok = std::sqrt( x*x + y*y + z*z ) > r + 0.05;   // clear of its own image
         for ( auto& o : b ) {
            for ( double sg : { 1.0, -1.0 } ) {
               double dd = std::sqrt( ( sg*x-o.x )*( sg*x-o.x ) + ( sg*y-o.y )*( sg*y-o.y ) + ( sg*z-o.z )*( sg*z-o.z ) );
               if ( dd < r + o.r + 0.05 ) { ok = false; }
            }
         }
         if ( ok ) { b.push_back( { x, y, z, r, 1.0 } ); b.push_back( { -x, -y, -z, r, 1.0 } ); }
      }
   }
   return b;
}

double trdd( const Matrix<double,11,11>& AR ) {
   double t = 0;
   for ( int k = 0; k < 5; ++k ) {
      t += AR( 6+k, 6+k );
   }
   return t / 10.0;
}

// strain block with translation freed (force-free), rotation held: reference-independent
double tfree( const Matrix<double,11,11>& AR ) {
   Matrix3d A = AR.topLeftCorner<3,3>();
   Matrix<double,3,5> C = AR.block<3,5>( 0, 6 );
   Matrix<double,5,5> S = AR.block<5,5>( 6, 6 ) - C.transpose() * A.ldlt().solve( C );
   return S.trace() / 10.0;
}

}  // namespace

int main() {
   int fails = 0;
   auto check = [ & ]( bool ok, const char* what, const std::string& s, double a, double b ) {
      if ( !ok ) { std::printf( "FAIL %s %s: %.15g vs %.15g\n", s.c_str(), what, a, b ); ++fails; }
   };
   std::printf( "%-7s %12s %12s %12s %12s  %-30s\n", "shape", "zeta/eta0", "oo/eta0", "tfree/eta0", "zeta-tfree",
                "viscosity centre - centroid" );
   for ( std::string s : { "L2", "L5", "R3", "R4", "P4", "P6", "P8", "L3bent", "rand20", "randinv" } ) {
      std::vector<Bead> b = shape( s );
      const int N = (int) b.size();
      Vector3d O = Vector3d::Zero();
      Matrix<double,11,11> AR = hydro( b, N, O );
      Vector3d d;
      double z = intrinsic_fixed( AR, d );
      // 1. direct: rebuild about d
      Matrix<double,11,11> ARd = hydro( b, N, d );
      double zd = trdd( ARd );
      check( std::fabs( zd / z - 1 ) < 1e-9, "closed form vs direct", s, z, zd );
      // 2. a minimum
      for ( int m = 0; m < 3; ++m ) {
         for ( double sg : { -1.0, 1.0 } ) {
            Vector3d p = d; p( m ) += sg * 0.05;
            double zp = trdd( hydro( b, N, p ) );
            check( zp > z * ( 1 + 1e-12 ), "not a minimum", s, zp, z );
         }
      }
      // 3. translation: same value, centre moves with the model
      std::vector<Bead> t = b;
      const Vector3d sh( 3.7, -2.1, 5.3 );
      for ( auto& x : t ) { x.x += sh( 0 ); x.y += sh( 1 ); x.z += sh( 2 ); }
      Vector3d dt;
      double zt = intrinsic_fixed( hydro( t, N, O ), dt );
      check( std::fabs( zt / z - 1 ) < 1e-9, "translation changes eta_zeta", s, z, zt );
      check( ( dt - d - sh ).norm() < 1e-7, "centre does not translate", s, ( dt - sh ).norm(), d.norm() );
      // 4. neighbours
      double e0 = intrinsic_high( AR ) + brown_ew( AR ), eoo = intrinsic_high( AR ), etf = tfree( AR );
      Vector3d c = Vector3d::Zero();
      for ( auto& x : b ) {
         c += Vector3d( x.x, x.y, x.z ) / N;
      }
      Vector3d off = d - c;
      std::printf( "%-7s %12.6f %12.6f %12.6f %+12.3e  %9.5f %9.5f %9.5f\n", s.c_str(), z / e0, eoo / e0, etf / e0,
                   ( z - etf ) / e0, off( 0 ), off( 1 ), off( 2 ) );
   }
   std::printf( fails ? "%d FAILED\n" : "all checks pass\n", fails );
   return fails ? 1 : 0;
}
