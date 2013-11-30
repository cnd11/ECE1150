package Auth;

use strict;
use warnings;

use CGI qw/standard/;
use CGI::Session;
use DBI;

use Data::Dumper;

require 'db_config.pl';

our $cgi;
our $session;

sub new {
    my $class = shift;
    my $self  = {};
    
    $cgi = new CGI;
    $session = CGI::Session->new($cgi);

    unless ( $session->id eq $cgi->cookie( 'CGISESSID' ) ) {
	print "Set-cookie: " . $cgi->cookie( 'CGISESSID', $session->id ) . "\n";
    } 

    bless( $self, $class );
    return $self;
}

sub validate {
    my $self = shift;

    if ( &check_authorization( $self ) == 0 ) {
	return 0;
    }             
    elsif ($cgi->param( 'signIn' ) eq "Sign In" ){
	if ( &check_credentials( $self ) == 0 ) {
	    &add_authorization( $self );
	    return 0;
	}
    }

    &display_login_prompt( $self );

    return 1;
}

sub check_authorization {
    if( defined $session->param('logged_in') &&
	defined $session->param('uname') &&
	$session->param( 'logged_in') == 1) { 
	return 0;
    }

    return 1;
}

sub check_credentials {
    my $self = shift;
   
    my $dbfile = get_dbname();
    my $host = get_dbserver();
    my $user = get_username();
    my $password = get_password();
    
    my $dsn = "dbi:mysql:dbname=".$dbfile.";host=".$host;
    
    my $user_name = $cgi->param('uname');
    my $user_pass = $cgi->param('pword');
    my $errors = "";
 
    if(!defined $user_name || $user_name eq "")
    {
	$errors .= 'user=0';
    }
    
    if(!defined $user_pass || $user_pass eq "")
    {
	$errors .= ($errors eq "") ? 'pass=0' : '&pass=0';
    }
    
    if( $errors ne "" )
    {
	$self->{ 'error_string' } = $errors;

	print $errors;
	return 1;
    }
    
    my $dbh = DBI->connect($dsn, $user, $password, {
	                   PrintError => 0,
			   RaiseError => 1,
			   AutoCommit => 1,
			   FetchHashKeyName => 'NAME_lc',
			 });
    my $sql_get = 'SELECT * FROM users WHERE uname=' . "\'$user_name\'";
    my $sth = $dbh->prepare( $sql_get );
    $sth->execute();
    
    if( $DBI::rows <= 0 )
    {
	$self->{ 'error_string' } = 'register=1';
	return 1;
    }
    
    while( my $row_hr = $sth->fetchrow_hashref )
    {
	if( $row_hr->{'pword'} eq $user_pass )
	{
	    $sth->finish;
	    $dbh->disconnect;
	    return 0;
	}
    }
    
    $dbh->disconnect;
    
    $self->{ 'error_string' } = 'pass=1';
    
    return 1;
}

sub add_authorization {
    $session->param('logged_in', 1);
    $session->param('uname', $cgi->param('uname') );
}

sub display_login_prompt {
    my $self = shift;

    print $cgi->redirect('http://' . $ENV{SERVER_NAME} . '/cgi-bin/login?'.$self->{'error_string'});
}

sub print_stuff {
    print  $cgi->escapeHTML($session->is_new)."*<br>";
    
    print  $cgi->escapeHTML($session->param('uname'))."*<br>";
    print  $cgi->escapeHTML($session->param('logged_in'))."*<br>";
    print  $cgi->escapeHTML($session->id)."*<br>";
    print  $cgi->escapeHTML($session->is_new)."*<br>";
}

1;
