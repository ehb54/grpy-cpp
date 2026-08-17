// GRPY input readers -- the .bead_model (-u) and hydro++ (-d) formats.
//
// ---------------------------------------------------------------------------------------
// PROVENANCE AND COPYRIGHT
//
// These readers are a translation into C++ of the input-handling path of GRPY.f:
//
//   GRPY -- Copyright (C) 2017 Pawel Jan Zuk
//   "This library is free software; you can redistribute it and/or modify it under the
//    terms of the GNU General Public License version 3" (GRPY.f header)
//
// Cite: Zuk, P. J., Cichocki, B. and Szymczak, P., "GRPY: an accurate bead method for
// calculation of hydrodynamic properties of rigid biomacromolecules", Biophys. J.
// 115:782-800 (2018).
//
// This file, like the rest of this repository, is distributed under the GPLv3. See LICENSE.
// ---------------------------------------------------------------------------------------
//
// The GRPY-native reader lives in grpy_api.hpp (read_native_file) because the in-process
// API needs it too; the two formats below are used only by the command-line program.
//
// Fortran list-directed reads split on whitespace AND commas, and take the first field of
// a record -- reproduced here by tokenize()/first_double(). Every field is checked: the
// original read unchecked, so a truncated or non-numeric file became zeros, which surfaced
// much later as a division by a zero radius or a meaningless result.
#pragma once
#include <cmath>
#include <fstream>
#include <string>
#include <vector>
#include "grpy_api.hpp"

namespace grpy {
namespace io {

// Fortran list-directed field separation: blanks, tabs and commas all separate.
inline std::vector<std::string> tokenize( const std::string& line ) {
   std::vector<std::string> out;
   std::string              cur;
   for ( char c : line ) {
      if ( c == ' ' || c == '\t' || c == ',' || c == '\r' ) {
         if ( !cur.empty() ) {
            out.push_back( cur );
            cur.clear();
         }
      } else {
         cur += c;
      }
   }
   if ( !cur.empty() ) {
      out.push_back( cur );
   }
   return out;
}

// One record, first field, as a double -- the shape of every scalar read in GRPY.f.
inline double first_double( std::istream& f, const std::string& path, const char* field, int& lineno ) {
   std::string line;
   if ( !std::getline( f, line ) ) {
      throw la::Error( "GRPY: input file '" + path + "' ends before the " + field
                       + " field (line " + std::to_string( lineno + 1 ) + ")." );
   }
   ++lineno;
   std::vector<std::string> t = tokenize( line );
   if ( t.empty() ) {
      throw la::Error( "GRPY: input file '" + path + "' line " + std::to_string( lineno )
                       + ": expected a number for " + field + ", found an empty record." );
   }
   try {
      return std::stod( t[ 0 ] );
   } catch ( const std::exception& ) {
      throw la::Error( "GRPY: input file '" + path + "' line " + std::to_string( lineno )
                       + ": expected a number for " + field + ", found '" + t[ 0 ] + "'." );
   }
}

// A bead's coordinates and radius must be usable by the mobility assembly, which divides
// by the radius; a non-finite coordinate poisons the whole matrix.
inline void check_bead( const Bead& b, const std::string& path, int lineno, int index ) {
   if ( !( b.radius > 0 ) || !std::isfinite( b.radius )
        || !std::isfinite( b.x ) || !std::isfinite( b.y ) || !std::isfinite( b.z ) ) {
      throw la::Error( "GRPY: input file '" + path + "' line " + std::to_string( lineno )
                       + ": bead " + std::to_string( index )
                       + " has a non-finite coordinate or a radius that is not positive ("
                       + std::to_string( b.radius ) + ")." );
   }
}

// us-somo .bead_model (-u): "NN vbar", then NN records "x y z radius mw", then 8 skipped
// records, then a record whose text after ':' is the unit exponent UN -> UNITS = 10^(2-UN).
// Temperature, viscosity and density are not in the file; GRPY hardcodes 20 C, 0.01 P, 1.
inline NativeInput read_bead_model( const std::string& path ) {
   std::ifstream f( path );
   if ( !f ) {
      throw la::Error( "GRPY: cannot open the input file '" + path + "'." );
   }
   NativeInput in;
   std::string line;
   int         lineno = 0;

   if ( !std::getline( f, line ) ) {
      throw la::Error( "GRPY: input file '" + path + "' is empty." );
   }
   ++lineno;
   std::vector<std::string> head = tokenize( line );
   if ( head.size() < 2 ) {
      throw la::Error( "GRPY: input file '" + path + "' line 1: expected 'bead count vbar', found '"
                       + line + "'." );
   }
   const int N    = (int) std::stod( head[ 0 ] );
   in.params.vbar = std::stod( head[ 1 ] );
   if ( N < 1 || N > 100000000 ) {
      throw la::Error( "GRPY: input file '" + path + "' declares an implausible bead count ("
                       + std::to_string( N ) + ")." );
   }

   double summed_mw = 0;
   in.beads.reserve( (size_t) N );
   for ( int i = 0; i < N; ++i ) {
      if ( !std::getline( f, line ) ) {
         throw la::Error( "GRPY: input file '" + path + "' declares " + std::to_string( N )
                          + " beads but contains only " + std::to_string( i ) + "." );
      }
      ++lineno;
      std::vector<std::string> t = tokenize( line );
      if ( t.size() < 5 ) {
         throw la::Error( "GRPY: input file '" + path + "' line " + std::to_string( lineno )
                          + ": expected 'x y z radius mw', found '" + line + "'." );
      }
      Bead b{};
      b.x      = std::stod( t[ 0 ] );
      b.y      = std::stod( t[ 1 ] );
      b.z      = std::stod( t[ 2 ] );
      b.radius = std::stod( t[ 3 ] );
      b.mw     = std::stod( t[ 4 ] );
      check_bead( b, path, lineno, i + 1 );
      summed_mw += b.mw;
      in.beads.push_back( b );
   }
   in.params.mw = summed_mw;

   for ( int i = 0; i < 8; ++i ) {                 // 8 records skipped by GRPY.f
      if ( !std::getline( f, line ) ) {
         throw la::Error( "GRPY: input file '" + path + "' ends before the unit record." );
      }
      ++lineno;
   }
   if ( !std::getline( f, line ) ) {
      throw la::Error( "GRPY: input file '" + path + "' ends before the unit record." );
   }
   ++lineno;
   const size_t colon = line.find( ':' );
   if ( colon == std::string::npos || colon + 2 > line.size() ) {
      throw la::Error( "GRPY: input file '" + path + "' line " + std::to_string( lineno )
                       + ": expected a unit record containing ':', found '" + line + "'." );
   }
   const int un    = std::stoi( line.substr( colon + 2 ) );
   in.params.units = std::pow( 10.0, 2 - un );

   in.params.temperature_C = 20.0;
   in.params.eta           = 0.01;
   in.params.rho           = 1.0;
   in.params.input_label   = "us-somo";
   return in;
}

// hydro++ coordinate file (-d, per model): UNITS, bead count, then "x y z radius" records.
// The physical scalars come from the control file, not from here.
inline std::vector<Bead> read_hydro_coordinates( const std::string& path, double& units ) {
   std::ifstream f( path );
   if ( !f ) {
      throw la::Error( "GRPY: cannot open the coordinate file '" + path + "'." );
   }
   int lineno = 0;
   units      = first_double( f, path, "unit", lineno );

   const double count = first_double( f, path, "bead count", lineno );
   if ( count < 1 || count > 1e8 || count != std::floor( count ) ) {
      throw la::Error( "GRPY: coordinate file '" + path + "' declares an implausible bead count ("
                       + std::to_string( count ) + ")." );
   }
   const int         N = (int) count;
   std::vector<Bead> beads;
   std::string       line;
   beads.reserve( (size_t) N );
   for ( int i = 0; i < N; ++i ) {
      if ( !std::getline( f, line ) ) {
         throw la::Error( "GRPY: coordinate file '" + path + "' declares " + std::to_string( N )
                          + " beads but contains only " + std::to_string( i ) + "." );
      }
      ++lineno;
      std::vector<std::string> t = tokenize( line );
      if ( t.size() < 4 ) {
         throw la::Error( "GRPY: coordinate file '" + path + "' line " + std::to_string( lineno )
                          + ": expected 'x y z radius', found '" + line + "'." );
      }
      Bead b{};
      b.x      = std::stod( t[ 0 ] );
      b.y      = std::stod( t[ 1 ] );
      b.z      = std::stod( t[ 2 ] );
      b.radius = std::stod( t[ 3 ] );
      b.mw     = 0;
      check_bead( b, path, lineno, i + 1 );
      beads.push_back( b );
   }
   return beads;
}

// One model block of a hydro++ control file. PARTNAME is Fortran CHARACTER*30, so it is
// truncated to 30 characters and trailing blanks are trimmed before it is displayed.
struct HydroJob {
   std::string part_name;
   std::string output_name;
   std::string coordinate_name;
   PhysParams  params;
};

// hydro++ control file (-d): blocks of part name, output name, coordinate file, ICASE,
// T, eta, Mw, vbar, rho, then six skipped records. A record beginning with '*' ends the run.
inline std::vector<HydroJob> read_hydro_control( const std::string& path ) {
   std::ifstream f( path );
   if ( !f ) {
      throw la::Error( "GRPY: cannot open the control file '" + path + "'." );
   }
   std::vector<HydroJob> jobs;
   std::string           line;
   int                   lineno = 0;

   while ( std::getline( f, line ) ) {
      ++lineno;
      if ( !line.empty() && line[ 0 ] == '*' ) {
         break;
      }
      HydroJob job;
      job.part_name = line.substr( 0, 30 );
      while ( !job.part_name.empty()
              && ( job.part_name.back() == ' ' || job.part_name.back() == '\r'
                   || job.part_name.back() == '\t' ) ) {
         job.part_name.pop_back();
      }

      auto next_name = [ & ]( const char* what ) -> std::string {
         if ( !std::getline( f, line ) ) {
            throw la::Error( "GRPY: control file '" + path + "' ends before the " + what
                             + " record (line " + std::to_string( lineno + 1 ) + ")." );
         }
         ++lineno;
         std::vector<std::string> t = tokenize( line );
         if ( t.empty() ) {
            throw la::Error( "GRPY: control file '" + path + "' line " + std::to_string( lineno )
                             + ": expected a " + what + ", found an empty record." );
         }
         return t[ 0 ];
      };

      job.output_name     = next_name( "output file name" );
      job.coordinate_name = next_name( "coordinate file name" );
      next_name( "ICASE" );                        // read and discarded, as in GRPY.f

      job.params.temperature_C = first_double( f, path, "temperature", lineno );
      job.params.eta           = first_double( f, path, "eta", lineno );
      job.params.mw            = first_double( f, path, "Mw", lineno );
      job.params.vbar          = first_double( f, path, "vbar", lineno );
      job.params.rho           = first_double( f, path, "rho", lineno );
      job.params.input_label   = "hydro++";

      for ( int i = 0; i < 6; ++i ) {              // 6 records skipped by GRPY.f
         if ( !std::getline( f, line ) ) {
            throw la::Error( "GRPY: control file '" + path + "' ends inside the block for '"
                             + job.part_name + "'." );
         }
         ++lineno;
      }
      jobs.push_back( job );
   }
   if ( jobs.empty() ) {
      throw la::Error( "GRPY: control file '" + path + "' contains no model blocks." );
   }
   return jobs;
}

}  // namespace io
}  // namespace grpy
