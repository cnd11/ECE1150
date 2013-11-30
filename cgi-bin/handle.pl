#!/usr/bin/perl -w 

use warnings;
use strict;

use CGI qw/:standard/; 
use CGI::Carp qw/fatalsToBrowser/;
use Auth;

my $cgi = new CGI;
my $auth = new Auth;

$auth->validate && exit 1;

print $cgi->redirect(-URL =>'http://' . $ENV{SERVER_NAME} . '/cgi-bin/home');
