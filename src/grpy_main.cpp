// GRPY command-line program -- a drop-in replacement for the Fortran GRPY.exe.
//
// ---------------------------------------------------------------------------------------
// PROVENANCE AND COPYRIGHT
//
//   GRPY -- Copyright (C) 2017 Pawel Jan Zuk
//   "This library is free software; you can redistribute it and/or modify it under the
//    terms of the GNU General Public License version 3" (GRPY.f header)
//
//   C++ port and extensions -- Copyright (C) 2026 the UltraScan project
//
// MODIFIED FROM GRPY.f IN 2026 (GPLv3 section 5a):
//   * the command line, the progress banner and the report reproduced as the original's,
//     so this program is a drop-in replacement for it;
//   * thread count, single precision, out-of-core and extended-precision reporting added
//     as environment variables, leaving the published command line untouched;
//   * failures reported on stderr with a non-zero exit, where the original aborted or
//     produced zeros.
//
// Copyright in the original work remains with its author; copyright in the new material
// is the UltraScan project's. The combined work is GPLv3, as a derivative work must be.
//
// Cite: Zuk, P. J., Cichocki, B. and Szymczak, P., "GRPY: an accurate bead method for
// calculation of hydrodynamic properties of rigid biomacromolecules", Biophys. J.
// 115:782-800 (2018).
// ---------------------------------------------------------------------------------------
//
// The four invocations are those of the original, and the output is compared byte for byte
// against it by tests/run.sh:
//
//   grpy <file>          GRPY-native input
//   grpy -e <file>       GRPY-native input, plus the progress banner on stdout
//   grpy -u <file>       us-somo .bead_model input
//   grpy -d <file>       hydro++ control file; each model's report goes to <output>-GRPY.dat
//
// Behaviour beyond the original is confined to the environment, so that the command line
// stays exactly as it was:
//
//   GRPY_THREADS=<n>     worker threads (default: all cores)
//   GRPY_SINGLE=1        single-precision storage and factorization (half the memory)
//   GRPY_OOC=<dir>       spill the tiled matrix to <dir> so RAM stays bounded
//   GRPY_HP=1            report in extended precision (ES24.15E3), for validation
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "grpy_api.hpp"
#include "grpy_io.hpp"
#include "grpy_report.hpp"
#include "parallel_std.hpp"

namespace {

// Resolved once: the environment is read here and nowhere else.
grpy::Options options_from_environment() {
   grpy::Options opt;
   opt.single = std::getenv( "GRPY_SINGLE" ) != nullptr;
   const char* ooc = std::getenv( "GRPY_OOC" );
   if ( ooc == nullptr ) {
      ooc = std::getenv( "GRPY_OOC_DIR" );         // the name SOMO uses
   }
   if ( ooc != nullptr ) {
      opt.ooc_dir = ooc;
   }
   return opt;
}

unsigned threads_from_environment() {
   const char* n = std::getenv( "GRPY_THREADS" );
   return n != nullptr ? (unsigned) std::atoi( n ) : 0u;   // 0 = all cores
}

// The usage text of GRPY.f, reproduced exactly: it is what a caller parsing our output
// on a bad invocation will have been written against.
void write_usage( std::ostream& o ) {
   grpy::ff::lstr( o, "ERROR: wrong input specified" );
   grpy::ff::lstr( o, "please execute GRPY program in the following way:" );
   grpy::ff::blank( o );
   grpy::ff::lstr( o, "1) ./GRPY.exe <input file>      --  input in the GRPY format" );
   grpy::ff::blank( o );
   grpy::ff::lstr( o, "2) ./GRPY.exe -e <input file>   --  input in the GRPY format"
                      " to additionally display program progress information" );
   grpy::ff::blank( o );
   grpy::ff::lstr( o, "3) ./GRPY.exe -d <input file>   --  input in the hydro++10 format" );
   grpy::ff::blank( o );
   grpy::ff::lstr( o, "4) ./GRPY.exe -u <input file>   --  input in the us-somo .bead_model format" );
}

// One model. `progress` is empty except in -e mode. The report is returned rather than
// written, because in -e mode the banner has to finish before the report begins.
grpy::Results solve( const grpy::NativeInput& in, const grpy::ProgressFn& progress ) {
   la::StdThreads par( threads_from_environment() );
   grpy::Solver   solver( par, options_from_environment() );
   return solver.run( in.beads, in.params, progress );
}

void solve_and_write( const grpy::NativeInput& in, std::ostream& report_to,
                      const grpy::ProgressFn& progress ) {
   report_to << solve( in, progress ).report;
}

// -e: the driver-level banner of GRPY.f, with the factorization reporting its own
// progress from inside (the original emitted that from within its bundled LAPACK).
int run_native_with_banner( const std::string& file ) {
   grpy::write_progress( std::cout, 0, "READING DATA" );
   grpy::NativeInput in = grpy::read_native_file( file );
   grpy::write_progress( std::cout, 1, "CALCULATING RC AND RG" );
   grpy::write_progress( std::cout, 3, "CONSTRUCTING MATRICES" );
   grpy::Results r = solve( in,
                            []( int pct, const char* stage ) {
                               grpy::write_progress( std::cout, pct, stage );
                            } );
   grpy::write_progress( std::cout, 98, "CALCULATING PROPERTIES" );
   grpy::write_progress( std::cout, 100, "COMPLETE" );
   std::cout << "\r" << std::string( 40, ' ' );
   std::cout << r.report;
   return 0;
}

// -d: a batch of models, each reported to its own file. The two stdout lines per model
// are the original's, and callers do parse them.
int run_hydro_batch( const std::string& file ) {
   std::string  dir;
   const size_t slash = file.find_last_of( '/' );
   if ( slash != std::string::npos ) {
      dir = file.substr( 0, slash + 1 );
   }
   std::vector<grpy::io::HydroJob> jobs = grpy::io::read_hydro_control( file );
   for ( const grpy::io::HydroJob& job : jobs ) {
      std::cout << " particle name: " << job.part_name << "\n";
      std::cout << " output file name: " << job.output_name << "-GRPY.dat" << "\n";

      grpy::NativeInput in;
      double            units = 1;
      in.beads         = grpy::io::read_hydro_coordinates( dir + job.coordinate_name, units );
      in.params        = job.params;
      in.params.units  = units;

      const std::string out_path = dir + job.output_name + "-GRPY.dat";
      std::ofstream     out( out_path );
      if ( !out ) {
         throw la::Error( "GRPY: cannot write the report file '" + out_path + "'." );
      }
      solve_and_write( in, out, {} );
      std::cout << "\n";
   }
   return 0;
}

}  // namespace

int main( int argc, char** argv ) {
   try {
      if ( argc == 2 ) {
         grpy::NativeInput in = grpy::read_native_file( argv[ 1 ] );
         solve_and_write( in, std::cout, {} );
         return 0;
      }
      if ( argc == 3 ) {
         const std::string flag = argv[ 1 ];
         const std::string file = argv[ 2 ];
         if ( flag == "-e" ) {
            return run_native_with_banner( file );
         }
         if ( flag == "-u" ) {
            grpy::NativeInput in = grpy::io::read_bead_model( file );
            solve_and_write( in, std::cout, {} );
            return 0;
         }
         if ( flag == "-d" ) {
            return run_hydro_batch( file );
         }
      }
      write_usage( std::cout );
      return 1;
   } catch ( const std::exception& e ) {
      // The Fortran aborted or produced zeros here. A caller that runs this as a
      // subprocess needs a non-zero exit and a readable reason instead.
      std::cout.flush();
      std::fprintf( stderr, "%s\n", e.what() );
      return 2;
   }
}
