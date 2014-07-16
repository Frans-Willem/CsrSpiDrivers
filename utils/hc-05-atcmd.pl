#!/usr/bin/perl

use strict;
use warnings;

my @cmds = qw(
    AT
    AT+VERSION?
    AT+ADDR?
    AT+NAME?
    AT+ROLE?
    AT+CLASS?
    AT+IAC?
    AT+INQM? 
    AT+PSWD? 
    AT+UART? 
    AT+CMODE?
    AT+BIND? 
    AT+POLAR? 
    AT+IPSCAN? 
    AT+SNIFF? 
    AT+SENM? 
    AT+ADCN? 
    AT+MRAD? 
    AT+STATE?
    AT+INQ
);

my $dev = shift @ARGV;
@cmds = (shift @ARGV)
    if @ARGV;

unless ($dev) {
    print "usage: $0 <tty> [AT_COMMAND]\n";
    exit 1;
}

open my $fh, "+<", $dev
    or die "$!";

for my $cmd (@cmds) {
    print $fh "$cmd\r\n";
    print "==> $cmd\n";

    while (<$fh>) {
        chomp;
        s/[\r\n]$//gms;
        print "<== $_\n";
        last
            if $_ eq "OK" or $_ eq "FAIL" or /^ERROR:/;
    }
}

close $fh;
