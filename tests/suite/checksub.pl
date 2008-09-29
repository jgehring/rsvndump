#!/usr/bin/perl
#
#	Test suite for rsvndump
#
# This is a simple script checking if sub-paths are dumped correctly
# 
#	USAGE: checksub REPOS_BASE WORK LOGS [ARGS]
#


use Cwd;
use File::Copy;


# Variables 
$playground = $ARGV[0];
$work = $ARGV[1];
$logs = $ARGV[2];
$repo_host = "svn://localhost";
$repo_name = "subtest";
$temp_repo_name = "subtest_temp";
$pwd = cwd;
$log_file = "$logs/subtest.log";
@args = @ARGV;
shift @args;
shift @args;
shift @args;


# Create repository
print "\n";
print "> Creating repositories ...";
chdir $playground;
mkdir $repo_name;
mkdir $temp_repo_name;
system "svnadmin create $repo_name";
system "svnadmin create $temp_repo_name";
chdir $pwd;
copy("anonymous.conf", "$playground/$repo_name/conf/svnserve.conf") or die $!;
copy("anonymous.conf", "$playground/$temp_repo_name/conf/svnserve.conf") or die $!;
print " done\n";


# Let's go to work
print "> Committing test files ...";
chdir $work;
system "svn co '$repo_host/$repo_name' > '$log_file'";
chdir $repo_name;

# Simple test with one file that will be copied to another directory
mkdir src;
system "svn add src >> '$log_file'";
chdir src;
$file = "test";
system "touch '$file' >> '$log_file'";
system "svn add '$file' >> '$log_file'";
for ($i = 0; $i < 2; $i++) {
	open(FILE, ">>$file");
	print FILE "another test line...\n"; 
	close(FILE);
	system "svn commit -m 'bla' >> '$log_file'";
}
chdir "..";
mkdir dest;
system "svn add dest >> '$log_file'";
system "svn commit -m 'bla' >> '$log_file'";
chdir dest;
system "svn cp ../src/$file . >> '$log_file'";
system "svn commit -m 'bla' >> '$log_file'";
# With local modification
for ($i = 0; $i < 2; $i++) {
	open(FILE, ">>$file");
	print FILE "another test line...\n"; 
	close(FILE);
	system "svn commit -m 'bla' >> '$log_file'";
}
print " done\n";

# Dump
print "> Running rsvndump ...";
system "$pwd/rsvndump -o dump @args '$repo_host/$repo_name/dest/$file' 2>> '$log_file'";
if ($? != 0) {
	exit 1;
}
print " done\n";

# Check
print "> Checking if the dump can be loaded ...";
system "cat dump| svnadmin load '$playground/$temp_repo_name' >> '$log_file'";
if ($? != 0) {
	exit 1;
}
print " ok\n";
print "> (Please validate the dump manually) ...";
print " ok\n";

chdir $pwd;
exit 0;
