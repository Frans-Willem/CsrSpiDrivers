#!/usr/bin/perl

use strict;
use warnings;

my $dev = shift @ARGV;
my $cmd = shift @ARGV || "AT+VERSION";

unless ($dev) {
    print "usage: $0 <tty> [AT_COMMAND]\n";
    exit 1;
}

open my $fh, "+<", $dev
    or die "$!";

syswrite $fh, $cmd;
print "==> $cmd\n";

my $nread = sysread $fh, my $resp, 1000;
die "$!"
    unless defined $nread;
print "<== $resp\n";

close $fh;
