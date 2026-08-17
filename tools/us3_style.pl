#!/usr/bin/env perl
# Apply the UltraScan III coding standards (v2.0) to C++ sources.
#
#   tools/us3_style.pl [--check] <file>...
#
# Rules applied, in this order:
#   1. tabs expanded, trailing blanks removed
#   2. indentation rescaled 4 -> 3 spaces (a continuation line shifts by the same
#      absolute amount as the statement it continues, so alignment survives)
#   3. a space after a control keyword: if (, for (, while (, switch (, catch (
#   4. no space between a function name and its opening parenthesis
#   5. a space just inside ( ) and [ ], except for a (Type) cast, which stays tight
#      and takes a space after it -- SOMO writes (QString) "red"
#   6. braces around a single-statement if / for / while body
#
# String literals, character literals, comments and preprocessor lines are never
# rewritten: every match is found on a masked copy of the line, where those regions
# are blanked out, and applied to the real line by position.
#
# Rule 6 finds the body by SCANNING for the parenthesis that closes the condition,
# not by a regular expression -- `if ( a ) return( f( x ) );` defeats any greedy
# pattern, and mis-splitting it silently produces code that still compiles.
#
# --check reports violations and exits non-zero without editing.
use strict;
use warnings;

my @CONTROL = qw( if for while switch catch );
# Keywords that are not function calls: a space after them is correct, and removing it
# turns `return (a)*b` into `return(a)*b`, which reads as a call.
my %KEYWORD  = map { $_ => 1 } qw( return throw new delete case co_return co_yield );
my %IS_CONTROL = map { $_ => 1 } @CONTROL;
my %TYPE_WORD  = map { $_ => 1 } qw( int unsigned long short char double float bool void
                                     size_t ssize_t uint8_t int64_t uint64_t );

# ---------------------------------------------------------------------------------------
# mask: a same-length copy of the line with string/char literal contents and comments
# blanked, so a pattern can never match inside them.
# ---------------------------------------------------------------------------------------
sub mask {
   my ( $line ) = @_;
   my $out = "";
   my $i   = 0;
   my $n   = length $line;
   while ( $i < $n ) {
      my $ch = substr( $line, $i, 1 );
      if ( $ch eq '"' || $ch eq "'" ) {
         my $quote = $ch;
         $out .= $ch;
         $i++;
         while ( $i < $n ) {
            my $c = substr( $line, $i, 1 );
            if ( $c eq "\\" ) {
               $out .= "  ";
               $i   += 2;
               next;
            }
            if ( $c eq $quote ) {
               $out .= $c;
               $i++;
               last;
            }
            $out .= " ";
            $i++;
         }
         next;
      }
      if ( $ch eq "/" && substr( $line, $i + 1, 1 ) eq "/" ) {
         $out .= " " x ( $n - $i );
         return $out;
      }
      $out .= $ch;
      $i++;
   }
   return $out;
}

# Split a line into its code and its trailing // comment. A brace has to be appended to
# the CODE -- appended to the whole line it lands inside the comment, where the compiler
# never sees it and the braces silently stop balancing.
sub split_comment {
   my ( $line ) = @_;
   # Scanned directly rather than read off mask(): mask() blanks the "//" itself, so a
   # real comment and a "//" inside a string literal look identical there.
   my $at = -1;
   my $i  = 0;
   my $n  = length $line;
   while ( $i < $n ) {
      my $ch = substr( $line, $i, 1 );
      if ( $ch eq '"' || $ch eq "'" ) {
         my $quote = $ch;
         $i++;
         while ( $i < $n ) {
            my $c = substr( $line, $i, 1 );
            $i += ( $c eq "\\" ) ? 2 : 1;
            last if $c eq $quote;
         }
         next;
      }
      if ( $ch eq "/" && substr( $line, $i + 1, 1 ) eq "/" ) {
         $at = $i;
         last;
      }
      $i++;
   }
   return ( $line, "" ) if $at < 0;
   my $code = substr( $line, 0, $at );
   my $cmt  = substr( $line, $at );
   $code =~ s/\s+$//;
   return ( $code, $cmt );
}

# Index of the parenthesis closing the one at $open, or -1 if the line does not close it.
sub match_paren {
   my ( $masked, $open ) = @_;
   my $depth = 0;
   for my $i ( $open .. length( $masked ) - 1 ) {
      my $c = substr( $masked, $i, 1 );
      $depth++ if $c eq "(";
      if ( $c eq ")" ) {
         $depth--;
         return $i if $depth == 0;
      }
   }
   return -1;
}

# Spans of (Type) casts, which keep tight parentheses.
sub cast_spans {
   my ( $line, $masked ) = @_;
   my @spans;
   while ( $masked =~ /\(([^()]*)\)/g ) {
      my $inner = $1;
      my $start = $-[ 0 ];
      my $end   = $+[ 0 ];
      next if $inner !~ /\S/ || $inner =~ /[,;]/;
      next if $inner !~ /^\s*(?:const\s+)?(?:unsigned\s+|signed\s+|long\s+|short\s+)*
                          [A-Za-z_][A-Za-z0-9_:]*\s*\**\s*&?\s*$/x;
      my ( $word ) = $inner =~ /^\s*(?:const\s+)?(\w+)/;
      next unless defined $word;
      next unless $TYPE_WORD{ $word } || $inner =~ /::/ || $inner =~ /\*\s*$/;
      my $after_at = $end;
      $after_at++ while substr( $line, $after_at, 1 ) eq " ";
      my $after = substr( $line, $after_at, 1 );
      my $before_at = $start - 1;
      $before_at-- while $before_at >= 0 && substr( $line, $before_at, 1 ) eq " ";
      my $before = $before_at >= 0 ? substr( $line, $before_at, 1 ) : "";
      next unless length $after && ( $after =~ /[A-Za-z0-9_(&*"]/ );
      next if $before =~ /[A-Za-z0-9_>]/;
      push @spans, [ $start, $end ];
   }
   return @spans;
}

sub in_spans {
   my ( $i, $spans ) = @_;
   for my $s ( @$spans ) {
      return 1 if $i >= $s->[ 0 ] && $i < $s->[ 1 ];
   }
   return 0;
}

# ---------------------------------------------------------------------------------------
# Rules 3-5, on one line.
# ---------------------------------------------------------------------------------------
sub fix_spacing {
   my ( $line ) = @_;
   return $line if $line =~ /^\s*#/;               # preprocessor
   my $masked = mask( $line );
   return $line if $masked !~ /\S/;                # comment-only line

   # rules 3 and 4: the gap between an identifier and its '('
   my @edits;
   while ( $masked =~ /\b(\w+)( *)\(/g ) {
      my ( $name, $gap ) = ( $1, $2 );
      next if $KEYWORD{ $name };
      my $want = $IS_CONTROL{ $name } ? " " : "";
      push @edits, [ $-[ 2 ], $+[ 2 ], $want ] if $gap ne $want;
   }
   for my $e ( reverse @edits ) {
      substr( $line, $e->[ 0 ], $e->[ 1 ] - $e->[ 0 ] ) = $e->[ 2 ];
   }

   # rule 5: a space just inside ( ) and [ ]
   $masked = mask( $line );
   my @casts = cast_spans( $line, $masked );
   my $out   = "";
   my $n     = length $line;
   for my $i ( 0 .. $n - 1 ) {
      my $ch = substr( $line,   $i, 1 );
      my $mc = substr( $masked, $i, 1 );
      if ( ( $mc eq "(" || $mc eq "[" ) && !in_spans( $i, \@casts ) ) {
         my $next = $i + 1 < $n ? substr( $masked, $i + 1, 1 ) : "";
         $out .= $ch;
         my $rest = substr( $line, $i + 1 );
         $out .= " " if $next ne " " && $next ne ")" && $next ne "]" && $next ne "" && $rest =~ /\S/;
         next;
      }
      if ( ( $mc eq ")" || $mc eq "]" ) && !in_spans( $i, \@casts ) ) {
         my $prev = $i > 0 ? substr( $masked, $i - 1, 1 ) : "";
         $out .= " " if $prev ne " " && $prev ne "(" && $prev ne "[" && $prev ne ""
                        && length( $out ) && substr( $out, -1 ) ne " ";
         $out .= $ch;
         next;
      }
      $out .= $ch;
   }
   $line = $out;

   # a cast takes a space before the value it converts
   $masked = mask( $line );
   for my $s ( reverse cast_spans( $line, $masked ) ) {
      my $end = $s->[ 1 ];
      substr( $line, $end, 0 ) = " " if $end < length( $line ) && substr( $line, $end, 1 ) ne " ";
   }
   return $line;
}

# ---------------------------------------------------------------------------------------
# Rule 2.
# ---------------------------------------------------------------------------------------
sub reindent {
   my ( $lines ) = @_;
   my @indents = map { /^( *)/ ? length( $1 ) : 0 } grep { /\S/ } @$lines;
   my @positive = grep { $_ > 0 } @indents;
   my $unit = 0;
   for my $i ( @positive ) {
      $unit = $i if $unit == 0 || $i < $unit;
   }
   return [ map { my $l = $_; $l =~ s/\s+$//; $l } @$lines ] if $unit == 3;

   my @out;
   my $depth      = 0;
   my $last_delta = 0;
   for my $line ( @$lines ) {
      if ( $line !~ /\S/ ) {
         push @out, "";
         next;
      }
      my ( $lead ) = $line =~ /^( *)/;
      my $old  = length $lead;
      my $text = $line;
      $text =~ s/^ +//;
      $text =~ s/\s+$//;
      my $new;
      if ( $depth > 0 || $text =~ m{^\*} ) {
         $new = $old + $last_delta;                # continuation, or a block comment body
      } else {
         $new        = int( $old * 3 / 4 + 0.5 );
         $last_delta = $new - $old;
      }
      $new = 0 if $new < 0;
      push @out, ( " " x $new ) . $text;
      my $m = mask( $line );
      $depth += ( $m =~ tr/(// ) - ( $m =~ tr/)// );
      $depth = 0 if $depth < 0;
   }
   return \@out;
}

# ---------------------------------------------------------------------------------------
# Rule 6. Returns ( kind, condition_end ) where kind is "inline", "next" or "" .
# ---------------------------------------------------------------------------------------
sub braceless {
   my ( $line ) = @_;
   my $masked = mask( $line );
   return ( "", 0 ) if $masked !~ /^\s*(if|for|while)\s*\(/;
   return ( "", 0 ) if $masked =~ /\{/;            # already opens a body
   my $open  = index( $masked, "(" );
   my $close = match_paren( $masked, $open );
   return ( "", 0 ) if $close < 0;                 # multi-line condition
   my $rest = substr( $masked, $close + 1 );
   return ( "next",   $close ) if $rest !~ /\S/;
   return ( "inline", $close ) if $rest =~ /;\s*$/ && $rest !~ /^\s*\)/;
   return ( "", 0 );
}

sub add_braces {
   my ( $lines ) = @_;
   my @out;
   my $i = 0;
   while ( $i < @$lines ) {
      my $line = $lines->[ $i ];
      my ( $kind, $close ) = braceless( $line );
      my ( $indent ) = $line =~ /^( *)/;

      if ( $kind eq "inline" ) {
         my ( $code, $cmt ) = split_comment( $line );
         my $head = substr( $code, 0, $close + 1 );
         # ONLY the first statement is the body. `if ( d <= 0 ) d = 0; d = sqrt( d );` is
         # two statements and the second one is NOT guarded -- bracing both changes what
         # the program computes, silently and in a way that still compiles. This is the
         # exact bug the braces rule exists to prevent, so the tool must not create it.
         my $mcode = mask( $code );
         my $depth = 0;
         my $semi  = -1;
         for my $p ( $close + 1 .. length( $mcode ) - 1 ) {
            my $c = substr( $mcode, $p, 1 );
            $depth++ if $c eq "(";
            $depth-- if $c eq ")";
            if ( $c eq ";" && $depth == 0 ) {
               $semi = $p;
               last;
            }
         }
         if ( $semi < 0 ) {
            push @out, $line;
            $i++;
            next;
         }
         my $body = substr( $code, $close + 1, $semi - $close );
         my $rest = substr( $code, $semi + 1 );
         $body =~ s/^\s+//;
         $rest =~ s/^\s+//;
         $rest =~ s/\s+$//;
         push @out, "$head {" . ( $cmt ne "" && $rest eq "" ? "  $cmt" : "" );
         push @out, "$indent   $body";
         push @out, "$indent}";
         push @out, "$indent$rest" . ( $cmt ne "" ? "  $cmt" : "" ) if $rest ne "";
         $i++;
         next;
      }
      if ( $kind eq "next" && $i + 1 < @$lines ) {
         my $next   = $lines->[ $i + 1 ];
         my $nmask  = mask( $next );
         my $simple = $nmask =~ /;\s*$/
                      && ( $nmask =~ tr/(// ) == ( $nmask =~ tr/)// )
                      && $nmask !~ /\{/
                      && $next =~ /\S/;
         if ( $simple ) {
            my ( $code, $cmt ) = split_comment( $line );
            push @out, "$code {" . ( $cmt ne "" ? "  $cmt" : "" );
            push @out, $next;
            push @out, "$indent}";
            $i += 2;
            next;
         }
         # A multi-line body: brace it and shift the whole statement in by nothing --
         # the continuation lines keep their alignment, which is what makes them readable.
         my $j = $i + 1;
         my $depth = 0;
         my $ok    = 0;
         while ( $j < @$lines ) {
            my $m = mask( $lines->[ $j ] );
            $depth += ( $m =~ tr/(// ) - ( $m =~ tr/)// );
            if ( $depth <= 0 && $m =~ /;\s*$/ ) {
               $ok = 1;
               last;
            }
            last if $m =~ /\{/;                    # not a plain statement -- leave it alone
            $j++;
         }
         if ( $ok ) {
            my ( $code, $cmt ) = split_comment( $line );
            push @out, "$code {" . ( $cmt ne "" ? "  $cmt" : "" );
            push @out, @{$lines}[ $i + 1 .. $j ];
            push @out, "$indent}";
            $i = $j + 1;
            next;
         }
      }
      push @out, $line;
      $i++;
   }
   return \@out;
}

# ---------------------------------------------------------------------------------------
sub check_file {
   my ( $path, $lines ) = @_;
   my $bad = 0;
   for my $n ( 0 .. $#$lines ) {
      my $line = $lines->[ $n ];
      my $m    = mask( $line );
      if ( $line =~ /\t/ ) {
         print "$path:@{[ $n + 1 ]}: tab\n";
         $bad++;
      }
      if ( length( $line ) > 132 ) {
         print "$path:@{[ $n + 1 ]}: @{[ length $line ]} cols\n";
         $bad++;
      }
      for my $kw ( @CONTROL ) {
         if ( $m =~ /\b$kw\(/ ) {
            print "$path:@{[ $n + 1 ]}: $kw(\n";
            $bad++;
         }
      }
      my ( $kind ) = braceless( $line );
      if ( $kind ) {
         print "$path:@{[ $n + 1 ]}: unbraced body\n";
         $bad++;
      }
   }
   return $bad;
}

my $checking = 0;
my @files;
for my $a ( @ARGV ) {
   if ( $a eq "--check" ) {
      $checking = 1;
   } else {
      push @files, $a;
   }
}
die "usage: us3_style.pl [--check] <file>...\n" unless @files;

my $violations = 0;
for my $path ( @files ) {
   open( my $fh, "<", $path ) or die "cannot read $path: $!\n";
   my @lines = map { my $l = $_; chomp $l; $l =~ s/\t/        /g; $l } <$fh>;
   close $fh;

   if ( $checking ) {
      $violations += check_file( $path, \@lines );
      next;
   }
   my $out = reindent( \@lines );
   $out = [ map { fix_spacing( $_ ) } @$out ];
   $out = add_braces( $out );
   open( my $wh, ">", $path ) or die "cannot write $path: $!\n";
   print $wh join( "\n", @$out ), "\n";
   close $wh;
   print "formatted $path\n";
}

if ( $checking ) {
   print "$violations violation(s)\n";
   exit( $violations ? 1 : 0 );
}
exit 0;
