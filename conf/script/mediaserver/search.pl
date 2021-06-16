#!/usr/bin/perl
#
# search.pl
#
# This script performs a search on the given folders
#
#
#

use strict;

use CGI ;
my $cgi = CGI->new() ;

#######################################################################
##
## Configuration Settings
##

my $S = "\x03" ;
my $ARCHIVE = "/httpserverfiles/media" ;
my @FOLDERLIST = ("Audio/Music", "Audio/Visits", "Audio/Radio") ;

my $HOSTIP = trim(`ifconfig -a | grep netmask | grep -v 127 | cut -d' ' -f10`) ;
my $SERVERPREFIX = "http://$HOSTIP/media" ;

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

#
# Perform searches
#

my $response = "" ;
chdir($ARCHIVE) ;

foreach my $folder (@FOLDERLIST) {

  if ($response eq "") {
    $response=searchfolder($folder) ;
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

  my ($folder) = @_ ;

  my $response = "" ;

  # Read list

  open FH, "<", "$folder/mediafiles.db" ;
  my @list = <FH> ;
  close FH ;

  my @outputlist ;

  $search =~ s/[\W\.\(\)\'\"]//g ;
  $search = lc($search) ;

  # Search for artist / album

  foreach my $entry (@list) {

    my $testentry = $entry ;
    $testentry =~ s/[ \t\n\r,\.\(\)\'\"]//g ;
    $testentry = lc($testentry) ;

    my ($tfile, $ttit, $tart, $talb) = split($S, trim($testentry)) ;

    if ( $tart =~ /$search/ || $talb =~ /$search/ ) {
      $response = $response . "$SERVERPREFIX/$entry" ;
    }

  }

  # Search for track

  if ( $response eq "" ) {

    foreach my $entry ( @list ) {

      my $testentry = $entry ;
      $testentry =~ s/[ \t\n\r,\.\(\)\'\"]//g ;
      $testentry = lc($testentry) ;


      my ($tfile, $ttit, $tart, $talb) = split($S, trim($testentry)) ;

      if ( $ttit =~ /$search/ ) {
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
