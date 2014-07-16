#!/usr/bin/perl

use warnings;
use strict;

open my $fh, $ARGV[0] || "-"
    or die $!;

my @data;

/^\s*\@([\da-f]+)\s+([\da-f]+)\s*$/i
    and $data[hex($1) / 2] = hex $2
        while <$fh>;

print pack "v*", @data;
