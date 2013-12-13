#!/usr/bin/perl -w 
print "Content-type: text/html\r\n\r\n";

use strict;
use warnings;
use Data::Dumper;
use DBI;

require 'db_config.pl';

my $start = times();

my $dbfile = get_dbname();
my $host = get_dbserver();
my $user = get_username();
my $password = get_password();

my $dsn = "dbi:mysql:dbname=".$dbfile.";host=".$host;

my $dbh = DBI->connect($dsn, $user, $password, {
    PrintError => 0,
    RaiseError => 1,
    AutoCommit => 1,
    FetchHashKeyName => 'NAME_lc',
		       }) ;

print "Time to connect: ". (times() - $start)."</br>";
$start = times();

my $sql = <<'END_SQL';
CREATE TABLE users (
    id    INTEGER AUTO_INCREMENT PRIMARY KEY,
    uname VARCHAR(100),
    pword VARCHAR(30)
)
END_SQL

#$dbh->do( $sql ) || die "meh";
#$dbh->disconnect;

#$dbh->do('INSERT INTO users (uname, pword) VALUES (?,?)',
#	 undef, 'BOB', '1234');
my $user = "\'BOB\'";
my $sql_get = 'SELECT * FROM users';
my $sth = $dbh->prepare($sql_get);
$sth->execute();

while( my @row = $sth->fetchrow_array )
{
    print $row[0]." ".$row[1]. " ". $row[2]."\n</br>";
}

$dbh->disconnect;

print "Time to execute: ". (times() - $start)."</br>";
=pod
my $dbh = DBI->connect( "dbi:mysql:", $user, $password);
print 1 == $dbh->do("create database $dbfile") ? "WORK\n" : "NOPE\n";
$dbh->disconnect;
=cut
