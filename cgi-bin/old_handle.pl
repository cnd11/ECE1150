#!/usr/bin/perl -w 

use warnings;
use strict;

use CGI qw/:standard cgi-lib/; # load CGI routines
use Data::Dumper;
use DBI;

require 'db_config.pl';

my $q = CGI->new;

my $dbfile = get_dbname();
my $host = get_dbserver();
my $user = get_username();
my $password = get_password();

my $dsn = "dbi:mysql:dbname=".$dbfile.";host=".$host;

my $user_name = $q->param('uname');
my $user_pass = $q->param('pword');
my $errors = "";

if(!defined $user_name || $user_name eq "")
{
    $errors .= 'user=0';
}

if(!defined $user_pass || $user_pass eq "")
{
    $errors .= ($errors eq "") ? 'pass=0' : '&pass=0';
}

if( $errors ne "")
{
    print $q->redirect('http://' . $ENV{SERVER_NAME} . "/cgi-bin/login?$errors");
}

my $dbh = DBI->connect($dsn, $user, $password, {
    PrintError => 0,
    RaiseError => 1,
    AutoCommit => 1,
    FetchHashKeyName => 'NAME_lc',
		       }) ;
my $sql_get = 'SELECT * FROM users WHERE uname=' . "\'$user_name\'";
my $sth = $dbh->prepare( $sql_get );
$sth->execute();

if( $DBI::rows <= 0 )
{
    print $q->redirect('http://' . $ENV{SERVER_NAME} . '/cgi-bin/login?register=1');
}

while( my $row_hr = $sth->fetchrow_hashref )
{
    if( $row_hr->{'pword'} ne $user_pass )
    {
    	print $q->redirect('http://' . $ENV{SERVER_NAME} . '/cgi-bin/login?pass=1');
    } else {
	print "Content-type: text/html\r\n\r\n";
	print "WELCOME " . $row_hr->{'uname'} ;
	print " *".($DBI::rows)."*";
    }
}

$dbh->disconnect;

1;

#foreach my $key (keys %ENV){print $key . " : " . $ENV{$key} . "<br><br>";}

