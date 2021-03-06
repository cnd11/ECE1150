#!/usr/bin/perl -w

use strict;
use warnings;
use diagnostics;

use CGI qw/standard/;
use CGI::Session;
use CGI::Carp qw/croak fatalsToBrowser/;
use Data::Dumper;

use IO::Socket::INET;

my $remote_host = "192.168.1.41";
my $remote_port = 1501;

my $cgi = CGI->new;
my $session = CGI::Session->new;

print $cgi->header;

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
    
    my $operation = "1";
    my $user_name = $session->param('uname');
    my $requested_path = "$user_name/";

    my $search_term = "n";
    my $new_filename = "n";

    my $socket = IO::Socket::INET->new( PeerAddr => $remote_host,
					PeerPort => $remote_port,
					Proto    => "tcp",
					Type     => SOCK_STREAM, )
	
	or print "Couldn't connect to $remote_host:$remote_port : $!\n";   

=pod
    print $operation . " " . defined $operation."\n";
    print $requested_path . " " . defined $requested_path."\n";
    print $file_name . " " . defined $file_name."\n";
    print $user_name . " " . defined $user_name."\n";
    print $search_term . " " . defined $search_term."\n";
    print $new_filename . " " . defined $new_filename."\n";
=cut
    if(defined $socket) {
	$socket->send( $operation . "\n" .
		       $requested_path. "\n" .
		       $file_name."\n" .
		       $user_name . "\n" .
		       $search_term . "\n" .
		       $new_filename . "n\n" )
	    or print "Cannot upload " . $!;

	my $res;
	$socket->read($res, 3);

	close( $socket );

	my $path_file = "/$requested_path$file_name";

print <<NEWENTRY;
	      <tr id="R$path_file">
		<td id="link">
		  <a id="$path_file" target="_blank" download>$path_file</a>
		</td>
		<td>
		  <a class="request" onClick="fetch_js(['$path_file'],[fetch_file])"> Request </a> 
		</td>
		<td>
		  <a class="search" onClick="search_file(['search_term', '$path_file'],[search_file_res])" > Search </a> 
		</td>
		<td>
		  <a class="rename" onClick="rename_file(['rename_term', '$path_file'],[rename_file_res])" > Rename </a> 
		</td>
		<td>
		  <a class="delete" onClick="delete_file(['$path_file'],[delete_file_res])" > Delete </a> 
		</td>
	      </tr>
NEWENTRY
    }
}
