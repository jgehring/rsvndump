#!/usr/bin/env perl
#
#	A short script that diffs incremental output with the normal one.
#


# Some constants. Too lazy to create arguments of them
my $steps = 10;


# Print help if neccessary
sub print_help() {
	print("USAGE: $0 [args] <url>\n");
}


# Parse arguments
my $url = pop();
if (!$url) {
	print_help();
	exit(1);
}
my $args = join(" ", @ARGV);

# Cleanup
system("rm -rf .dumps");
mkdir(".dumps");


# Get HEAD revision
open(in, "svn info $url |");
while (<in>) {
	if (m/Revision: (\d*)/) {
		$head = int($1);
	}
}
close(in);


# Run normal, non-incremental dump
print(">> Preforming normal dump... ");
system("../src/rsvndump $args $url > .dumps/normal 2> .dumps/normal.log") == 0 || die $!;
print("done\n");


# Run incremental dumps
print(">> Preforming normal dump... ");
my $start = 0;
my $end = $steps;
while (system("../src/rsvndump --incremental -r $start:$end $args $url >> .dumps/incremental 2>> .dumps/incremental.log") == 0) {
	$start = $end+1;
	$end += $steps;
	if ($end > $head) {
		$end = $head;
	}
}
print("done\n");
