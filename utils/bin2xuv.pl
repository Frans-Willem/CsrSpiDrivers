#!/usr/bin/perl

use warnings;
use strict;

open my $fh, $ARGV[0] || "-"
    or die $!;

my $i = 0;
undef $/;
map { printf "@%06x %04x\r\n", $i++, $_ } unpack "v*", <$fh>;
