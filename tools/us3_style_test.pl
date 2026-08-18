#!/usr/bin/env perl
# Regression tests for us3_style.pl.
#
#   tools/us3_style_test.pl
#
# The property that matters here is AGREEMENT between the two code paths: whatever --check
# calls a violation, the formatter must actually fix, and whatever the formatter leaves
# alone, --check must not report. They drifted once -- --check reported a brace-on-its-own-
# line body as unbraced, which the formatter correctly declined to touch, so --check could
# never reach zero on a file written in that style.
use strict;
use warnings;
use File::Temp qw( tempdir );

my $tool = ( $0 =~ m{^(.*)/} ? $1 : "." ) . "/us3_style.pl";
my $dir  = tempdir( CLEANUP => 1 );
my $fail = 0;

sub write_file {
   my ( $path, $text ) = @_;
   open( my $fh, ">", $path ) or die "cannot write $path: $!\n";
   print $fh $text;
   close $fh;
}

sub slurp {
   my ( $path ) = @_;
   open( my $fh, "<", $path ) or die "cannot read $path: $!\n";
   local $/;
   return <$fh>;
}

sub check_count {
   my ( $text ) = @_;
   my $p = "$dir/c.cpp";
   write_file( $p, $text );
   my @out = grep { /unbraced body/ } split( /\n/, `perl $tool --check $p 2>/dev/null` );
   return scalar @out;
}

sub formatted {
   my ( $text ) = @_;
   my $p = "$dir/f.cpp";
   write_file( $p, $text );
   system( "perl $tool $p >/dev/null 2>&1" );
   return slurp( $p );
}

sub ok {
   my ( $desc, $got, $want ) = @_;
   if ( $got eq $want ) {
      printf( "  %-58s OK\n", $desc );
   } else {
      printf( "  %-58s FAIL (got %s, want %s)\n", $desc, $got, $want );
      $fail++;
   }
}

my $allman = <<'EOF';
void f( int x )
{
   if ( x > 0 )
   {
      a();
   }
   for ( int i = 0; i < 3; ++i )
   {
      b();
   }
}
EOF

my $unbraced = <<'EOF';
void f( int x )
{
   if ( x < 0 ) a();
   while ( x-- )
      b();
}
EOF

print "[us3_style]\n";
ok( "a brace on its own line is not an unbraced body", check_count( $allman ),   0 );
ok( "a genuinely unbraced body is still reported",     check_count( $unbraced ), 2 );
ok( "the formatter leaves brace-on-own-line alone",    formatted( $allman ),     $allman );

# Agreement: after formatting, --check must be satisfied. This is what fails when the two
# paths disagree -- the formatter reports nothing left to do while --check still objects.
ok( "--check reaches zero after formatting",           check_count( formatted( $unbraced ) ), 0 );
ok( "formatting is idempotent",                        formatted( formatted( $unbraced ) ),
                                                       formatted( $unbraced ) );

# A trailing comment must not swallow the brace.
# A braced initializer list in the BODY is not a body brace. Testing the whole line for '{'
# exempted these, and they read as clean -- two such lines reached a merge before this.
my $initlist = "void f()\n{\n   for ( int i : v ) out.push_back( {a[ i ], b[ i ]} );\n}\n";
ok( "a braced init list in the body is not a body brace", check_count( $initlist ), 1 );
ok( "the formatter braces it",
    ( formatted( $initlist ) =~ /for \( int i : v \) \{/ ? 1 : 0 ), 1 );
ok( "--check reaches zero after formatting it", check_count( formatted( $initlist ) ), 0 );

# A brace that really does open the body, on the same line, is still exempt.
my $samebrace = "void f()\n{\n   if ( a ) { b(); }\n}\n";
ok( "a real same-line body brace is still exempt", check_count( $samebrace ), 0 );

my $cmt = "void f()\n{\n   if ( a ) b();   // note\n}\n";
ok( "a braced body keeps its trailing comment outside", ( formatted( $cmt ) =~ /\{\s+\/\/ note/ ? 1 : 0 ), 1 );

print $fail ? "FAILED ($fail)\n" : "ALL PASS (0 failures)\n";
exit( $fail ? 1 : 0 );
