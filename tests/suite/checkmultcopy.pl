#!/usr/bin/perl
#
#	Test suite for rsvndump
#
# This is a simple script checking if multiple copies via renames work. If this
# test fails, it is likely that copy-from revision numbers are not calculated
# correctly
#
#	USAGE: checkmultcopy REPOS_BASE WORK LOGS [ARGS]
#


use Cwd;
use File::Copy;


# Variables 
$playground = $ARGV[0];
$work = $ARGV[1];
$logs = $ARGV[2];
$repo_host = "svn://localhost";
$repo_name = "copymultcopy";
$temp_repo_name = "copymultcopy_temp";
$pwd = cwd;
$log_file = "$logs/copymultcopy.log";
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

# Use two files and rename every single one 5 times
mkdir src;
system "svn add src >> '$log_file'";
chdir src;
$file1 = "testfile1";
$file2 = "testfile2";
open(FILE, ">$file1");
print FILE "this is file 1...\n"; 
close(FILE);
open(FILE, ">$file2");
print FILE "this is file 2...\n"; 
close(FILE);
system "svn add '$file1' '$file2' >> '$log_file'";
system "svn commit -m 'blubb' >> '$log_file'";
system "svn mv '$file1' '$file1 0' >> '$log_file'";
system "svn commit -m 'blubb' >> '$log_file'";
for ($i = 1; $i < 5; $i++) {
	$j = $i-1;
	system "svn mv '$file1 $j' '$file1 $i' >> '$log_file'";
	system "svn commit -m 'blubb' >> '$log_file'";
}
system "svn mv '$file2' '$file2 0' >> '$log_file'";
system "svn commit -m 'blubb' >> '$log_file'";
for ($i = 1; $i < 5; $i++) {
	$j = $i-1;
	system "svn mv '$file2 $j' '$file2 $i' >> '$log_file'";
	system "svn commit -m 'blubb' >> '$log_file'";
}
print " done\n";

# Dump
print "> Running rsvndump ...";
system "$pwd/rsvndump @args '$repo_host/$repo_name' > dump 2>> '$log_file'";
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
