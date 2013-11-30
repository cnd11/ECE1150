#!/usr/bin/perl -w

use warnings;
use strict;

use IO::Socket::INET;

my $server_port = 5432;

my $server = IO::Socket::INET->new( LocalPort => $server_port,
				    Type      => SOCK_STREAM,
				    Reuse     => 1,
				    Listen    => 10, ) # or SOMAXCONN(??)
    or die "Couldn't be a tcp server on port $server_port: $!\n";

while( my $client = $server->accept() )
{
    #$client is new connection
    print "HI!";
}

close( $server );
