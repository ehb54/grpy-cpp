// std::thread backend for la::Parallel, used by the GRPY program.
//
// Copyright (C) 2026 the UltraScan project. Original work -- nothing here is translated
// from GRPY.f -- distributed under the GPLv3 as part of this program. See LICENSE.
#pragma once
#include <atomic>
#include <thread>
#include <vector>
#include "linalg.hpp"

namespace la {
struct StdThreads : Parallel {
   unsigned nthreads;
   explicit StdThreads( unsigned n = 0 )
      : nthreads( n ? n : std::max( 1u, std::thread::hardware_concurrency() ) ) {}
   void for_range( int n, const std::function<void( int )>& f ) override {
      if ( n <= 1 || nthreads <= 1 ) { for ( int i = 0; i < n; ++i ) f( i ); return; }
      std::atomic<int> next{0};
      auto worker = [ & ] { int i; while ( ( i = next.fetch_add( 1 ) ) < n ) f( i ); };
      std::vector<std::thread> ts;
      unsigned m = std::min<unsigned>( nthreads, (unsigned) n );
      for ( unsigned t = 0; t < m; ++t ) {
         ts.emplace_back( worker );
      }
      for ( auto& t : ts ) {
         t.join();
      }
   }
};
}  // namespace la
