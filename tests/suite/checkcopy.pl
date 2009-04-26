#!/usr/bin/perl
#
#	Test suite for rsvndump
#
# This is a simple script checking if copies of directories are handled
# correcly
# 
#	USAGE: checkcopy REPOS_BASE WORK LOGS [ARGS]
#


use Cwd;
use File::Copy;


# Variables 
$playground = $ARGV[0];
$work = $ARGV[1];
$logs = $ARGV[2];
$repo_host = "svn://localhost";
$repo_name = "copytest";
$temp_repo_name = "copytest_temp";
$pwd = cwd;
$log_file = "$logs/copytest.log";
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
system "svn cp src dest >> '$log_file'";
system "svn commit -m 'copied' >> '$log_file'";
chdir dest;
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
system "$pwd/rsvndump @args '$repo_host/$repo_name/dest' > dump 2>> '$log_file'";
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
