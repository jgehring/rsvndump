#!/usr/bin/perl
#
#	Test suite for rsvndump
#


use Cwd;
use File::Path;


# Variables
$pwd = cwd;
$playground = "$pwd/playground";
$work = "$pwd/work";
$logs = "$pwd/logs";


# Compile rsvndump with debug information
print ">>> Compiling program ...";
chdir "../..";
system "CFLAGS=\"-DDEBUG -g\" ./configure > /dev/null";
system "make clean > /dev/null";
system "make > /dev/null";
system "cp src/rsvndump $pwd";
chdir $pwd;
print " done\n";


print ">>> Setting up playground repository ...";
# Create playground repository
rmtree($playground, 0, 0);
mkdir $playground or die $!;

# Start svnserve on the repository
system "killall -q svnserve";
system "svnserve -d -r $playground";
print " done\n";


print ">>> Setting up work and log directories ...";
# Create work folder
rmtree($work, 0, 0);
mkdir $work or die $!;

# Create logs folder
rmtree($logs, 0, 0);
mkdir $logs or die $!;
print " done\n";


# Run a set of checks 
print "\n";
@checks = ("./checklog.pl", "checksub.pl");
@checkmsg = ("log message dumping", "sub-path dumping");
$i = 0;
foreach $check (@checks) {
	print (">> Checking for $checkmsg[$i] ...\n");
	@cargs = ("$check", "$playground", "$work", "$logs");
	$rv = system @cargs; 
	if ($rv == "0") {
		print "\n>> ok\n";
	} else {
		print "\n>> failed\n" and die $!;
	}
	print "\n";
	++$i;
}

exit 0;
