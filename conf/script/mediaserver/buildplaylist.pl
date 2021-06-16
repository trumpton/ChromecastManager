#!/usr/bin/perl
#
# buildplaylist.sh
#
# This script accepts the relative name of a folder (under $ARCHIVE)
# and for each mp3 file identified, runs id3v2 to extract the metadata
# which it stores in a mediafiles.db file in the same folder.
#  

use strict;

#######################################################################
##
## Configuration Settings
## 

my $S = "\x03" ;
my $ARCHIVE = "/httpserverfiles/media" ;
my @FOLDERLIST = ("Audio/Music", "Audio/Visits", "Audio/Radio") ;

#######################################################################

# Fetch files

chdir($ARCHIVE) ;

my $search ;
my $argc = @ARGV ;
  
if ($argc>0) {
  @FOLDERLIST=@ARGV ;
}

foreach my $folder (@FOLDERLIST) {

  my @files = getmp3files($folder) ;

  # Fetch ID3 Info

  my @id3list ;
  foreach my $file (@files) {
    my $id3info = getid3info($file) ;
    if ( "$id3info" ne "" ) {
      push @id3list, $id3info ;
    } 
  }

  # Sort files list

  @id3list = sort @id3list ;

  # And adjust entry order
  # (track number no longer needed as file now sorted)

  for (my $i=0; $i<scalar(@id3list); $i++) {
    my ($album, $track, $artist, $title, $file) = split(/$S/,$id3list[$i]) ;
    $id3list[$i] = $file . $S . $title . $S . $artist . $S . $album ;
  }

  # And output

  open FH, ">", "$ARGV[0]/mediafiles.db" ;
  foreach my $entry (@id3list) {
    my ($file, $title, $artist, $album) = split(/$S/,$entry) ;
    print "OUTPUTTING $file ($title)\n" ;
    print FH "$entry\n" ;
  }
  close FH ;

}

exit(1) ;


#
# Get list of mp3 files to process
#

sub getmp3files {
 
  my ($folder) = @_ ;	

  my @files ;

  opendir my $DH, $folder ;
  
  while (readdir($DH)) {

    my $entry = $_ ;

    if ( -d "$folder/$entry" && $entry ne "." && $entry ne ".." ) {
      push @files, getmp3files("$folder/$entry") ;
    } elsif ( -f "$folder/$entry" && $entry =~ /\.mp3$/ ) {
      push @files, "$folder/$entry" ;
    }
  }

  closedir $DH ;

  return @files ;
}

#
# Extract id3 info from file
#

sub getid3info {

  my ($file) = @_ ;

  my $filepath = $ARCHIVE . "/" . $file ;
  my $id3output = `id3v2 -l "$filepath"` ;

  my ($TPE) =  $id3output =~ m/TPE1[^:]*: (.*)$/m ;
  my ($ALB) = $id3output =~ m/TALB[^:]*: (.*)$/m ;
  my ($TIT) = $id3output =~ m/TIT2[^:]*: (.*)$/m ;
  my ($TRK) = $id3output =~ m/TRCK[^:]*: ([0-9]*).*$/m ;
  my ($COM) = $id3output =~ m/COMM.*\): (.*)$/m ;

  $TRK = substr("0000" . $TRK, -4, 4) ;

  if ( $COM ne "Index" && $TPE ne "") {
    print "PROCESSING: $file ($TIT)\n" ;
    return $ALB . $S . $TRK . $S . $TPE . $S . $TIT . $S . $file ;
  } else {
    print "SKIPPING: $filepath\n" ;
    return "" ;
  }

}

