#!/usr/bin/env perl
#
#	A short script that diffs incremental output with the normal one.
#


# Some constants. Too lazy to create arguments of them
my $steps = 1;


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


# Fetch commits for incremental dump
print(">> Fetching logs... ");
system("svn log --xml $url | grep -E 'revision=\"[0-9]{1,}\">' | tac > .dumps/revisions") == 0 || die $!;
print("done\n");


# Run incremental dumps
print(">> Preforming incremental dump... ");
open(in, "< .dumps/revisions") || die $!;
my $last = 0;
my $n = 0;
while (<in>) {
	my $rev;
	if (m/\"([0-9]*)\"/) {
		$rev = $1;
	}

	# Stick to given step width
	if (($n % $steps) != 0) {
		next;
	}

	system("../src/rsvndump --incremental --no-incremental-header -r $last:$rev $args $url >> .dumps/incremental 2>> .dumps/incremental.log") == 0 || die $!;
	$last = $rev;
}
close(in);
print("done\n");
