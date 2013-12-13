#!/use/bin/perl -w

use warnings;
use strict;

use IO::Socket::INET;

my $remote_host = "169.254.0.2";
my $remote_port = 1500;

my $socket = socket_factory();

my $ans = <$socket>;

print $ans."\n";

close( $socket );

sub socket_factory {
    return  IO::Socket::INET->new( PeerAddr => $remote_host,
				   PeerPort => $remote_port,
				   Proto    => "tcp",
				   Type     => SOCK_STREAM, )
	or die "Couldn't connect to $remote_host:$remote_port : $!\n";
}
