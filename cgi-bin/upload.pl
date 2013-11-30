#!/usr/bin/perl -w

use strict;
use warnings;
use diagnostics;

use CGI qw/standard/;
use CGI::Session;
use CGI::Carp qw/croak fatalsToBrowser/;
use Data::Dumper;

my $cgi = CGI->new;
my $session = CGI::Session->new;

print "Content-type: text/html\r\n\r\n";

if( $cgi->param('upload_file'))
{
    my $upload = $cgi->upload('upload_file');
    my $info = $cgi->uploadInfo($upload);
   
    my $file_name = $cgi->param('upload_file');
    my $dir = $session->param('uname');
    my $path = "/var/www/data/$dir/$file_name";
    
    if( !(-d "/var/www/data/$dir") ) {
	mkdir "/var/www/data/$dir" or die "Couldn't create dir!";
    }    

    open( SAVE, ">$path") or croak "Unable to open $path $!";
    binmode( SAVE );
    print SAVE $_ while ( <$upload> );
    close( $upload );
    close( SAVE );
    
    print "<a href='/data/$file_name' target='_blank' download>$file_name</a> <br>";
 #   print "<li>Bytes " . length($data) . "<br>\n";
  #  print "<li>Content type " . $info->{'Content-Type'} . "<br>\n";
   # print "<li>Content disposition " . $info->{'Content-Disposition'} . "<br>\n";
    #print "<li>". ""  . "<br>\n";
    
}
