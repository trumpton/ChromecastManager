#!/usr/bin/perl
#
# search.pl
#
# This script performs a search on the given folders
#
# search - album then artist then track
# search genre - search for specific genre
# search artist - specific artist
# search album - specific album
#
# This will use the $ARCHIVE/translate.db file to 
# parse any query details before the search.  This is 
# useful if you have problems with the recognition
# of artists / tracks etc.
#
# The search script outputs a search.log file in the
# $ARCHIVE folder, so this file can be interrogated to
# see exactly what is being passed to the search.
#

use strict;
use Data::Dump qw(dump);

use CGI ;
my $cgi = CGI->new() ;

#######################################################################
##
## Configuration Settings
##

my $ARCHIVE="/mediadatabase/data" ;
my @FOLDERLIST=("Audio/Music", "Audio/Visits", "Audio/Radio") ;
my $TRANSLATION="translate.db" ;

my $HOSTIP=trim(`ifconfig -a | grep netmask | grep -v 127 | cut -d' ' -f10`) ;
my $SERVERPREFIX="http://$HOSTIP/media" ;

#######################################################################

#
# Extract search query
#

my $search ;
my $argc = @ARGV ;

if ($argc>0) {
  $search = $ARGV[0] ;
} else {
  $search = $cgi->param("q") ;
}

open LOG, ">", "$ARCHIVE/search.log" ;
print LOG "Search=$search\n" ;

my @list = loadtranslations("$ARCHIVE/$TRANSLATION") ;
$search = translate($search, @list) ;

print LOG "Translated=$search\n" ;

close LOG ;

#
# Perform searches
#

my $response = "" ;
chdir($ARCHIVE) ;

my $albumonly=0 ;
my $artistonly=0 ;
my $genreonly=0 ;

my ($ssearch, $type) = $search =~ /(.*)\W([a-z]*)$/i ;
if ($type eq "album") {

  $search = $ssearch ;
  $albumonly = 1 ;

} elsif ($type eq "artist") {

  $search = $ssearch ;
  $artistonly = 1 ;

} elsif ($type eq "genre" || $type eq "music") {
  $search = $ssearch ;
  $genreonly = 1 ;

}

foreach my $folder (@FOLDERLIST) {

  if ($response eq "") {
    $response=searchfolder($search, $genreonly, $albumonly, $artistonly, $folder) ;
  }

}

#
# Output
#

print "Content-type: text/plain\r\n\r\n$response";

#
# And exit
#

exit(0) ;


#
# Search folder and provide response
#

sub searchfolder {

  my ($search, $genreonly, $albumonly, $artistonly, $folder) = @_ ;

  my $response = "" ;

  # Read list

  open FH, "<", "$folder/mediafiles.db" ;
  my @list = <FH> ;
  close FH ;

  my @outputlist ;
  my $S = "\x03" ;

  $search =~ s/[\W\.\(\)\'\"]//g ;
  $search = lc($search) ;

  # Search for album

  if ( $genreonly==0 && $albumonly==0 ) {

    foreach my $entry (@list) {

      my $testentry = $entry ;
      $testentry =~ s/[ \t\n\r,\.\(\)\'\"]//g ;
      $testentry = lc($testentry) ;

      my ($tfile, $ttit, $tart, $talb, $tgen) = split($S, trim($testentry)) ;

      if ( $talb =~ /$search/ ) {
        $response = $response . "$SERVERPREFIX/$entry" ;
      }

    }

  }

  # Search for artist

  if ( $response eq "" && $genreonly==0 && $albumonly==0) {

    foreach my $entry (@list) {

      my $testentry = $entry ;
      $testentry =~ s/[ \t\n\r,\.\(\)\'\"]//g ;
      $testentry = lc($testentry) ;

      my ($tfile, $ttit, $tart, $talb, $tgen) = split($S, trim($testentry)) ;

      if ( $tart =~ /$search/ ) {
        $response = $response . "$SERVERPREFIX/$entry" ;
      }

    }

  }

  # Search for track

  if ( $response eq "" && $genreonly==0 && $artistonly==0 && $albumonly==0) {

    foreach my $entry ( @list ) {

      my $testentry = $entry ;
      $testentry =~ s/[ \t\n\r,\.\(\)\'\"]//g ;
      $testentry = lc($testentry) ;

      my ($tfile, $ttit, $tart, $talb, $tgen) = split($S, trim($testentry)) ;

      if ( $ttit =~ /$search/ ) {
        $response = $response . "$SERVERPREFIX/$entry" ;
      }

    }

  }

  # Search for Genre
  
  if ($response eq "") {

    foreach my $entry ( @list ) {

      my $testentry = $entry ;
      $testentry =~ s/[ \t\n\r,\.\(\)\'\"]//g ;
      $testentry = lc($testentry) ;

      my ($tfile, $ttit, $tart, $talb, $tgen) = split($S, trim($testentry)) ;

      if ( $tgen =~ /$search/ ) {
	$response = $response . "$SERVERPREFIX/$entry" ;
      }

    }

  }

  return $response ;

}


# Perl trim function to remove whitespace from the start and end of the string
sub trim($)
{
  my $string = shift;
  $string =~ s/^\s+//;
  $string =~ s/\s+$//;
  return $string;
}

sub translate
{
  my ($str, @list) = @_ ;

  my $llen = scalar(@list) ;

  for (my $i=0; $i<$llen; $i+=2) {
    my $from = $list[$i] ;
    my $to = $list[$i+1] ;
    $str =~ s/\b$from\b/$to/ig ;
  }

  return $str ;
}

sub loadtranslations
{
  my ($file) = @_ ;
  my @list ;

  open my $fh, $file ;

  while (my $line = <$fh>) {
    if ( $line =~ /#.*/ ) {
      # Ignore comments
    } else {
      my ($from,$to) = $line =~ /([^=]*)=(.*)/ ;
      $from = trim($from) ;
      $to = trim($to) ;
      if ($to ne "") {
        push @list, ($from,$to) ;
      }
    }
  }
  close $fh ;
  return @list ;
}
