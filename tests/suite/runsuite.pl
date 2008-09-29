#!/usr/bin/perl
#
#	Test suite for rsvndump
#
# USAGE: ./runsuite.pl [--configure] [ARGS]
#


use Cwd;
use File::Path;


# Variables
$pwd = cwd;
$playground = "$pwd/playground";
$work = "$pwd/work";
$logs = "$pwd/logs";
@args = @ARGV;


# Compile rsvndump with debug information
print ">>> Compiling program ...";
chdir "../..";
if (@ARGV[0] eq "--configure") {
	system "CFLAGS=\"-DDEBUG -g\" ./configure --with-apr=/usr > /dev/null";
	shift @args;
}
system "make clean > /dev/null";
system "make -j2 > /dev/null";
if ($? != 0) {
	die "Failed\n";
}
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
@checks = ("./checkmultcopy.pl", "./checkcopy.pl", "./checkcopy2.pl", "./checklog.pl", "./checksub.pl");
@checkmsg = ("copying of single files", "copying of directories", "copying of directories (variant 2)", "log message dumping", "sub-path dumping");
$i = 0;
foreach $check (@checks) {
	print (">> Checking for $checkmsg[$i] ...\n");
	@cargs = ("$check", "$playground", "$work", "$logs", "@args");
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
