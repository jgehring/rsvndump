#!/usr/bin/perl
#
#	Test suite for rsvndump
#
# This is a simple script checking if log messages are extracted
# correctly.
#
#	USAGE: checklog REPOS_BASE WORK LOGS [ARGS]
#


use Cwd;
use File::Copy;


# Variables 
$playground = $ARGV[0];
$work = $ARGV[1];
$logs = $ARGV[2];
$repo_host = "svn://localhost";
$repo_name = "logtest";
$pwd = cwd;
$log_file = "$logs/logtest.log";
@messages = ("First log message", "Second log message", "Some other characters: äöü!§\$%&/()", "M\nu\nlti\nli\nne", "Escapi\tng");
$delim = "------------------------------------------------------------------------";
@args = @ARGV;
shift @args;
shift @args;
shift @args;


# Create repository
print "\n";
print "> Creating repository ...";
chdir $playground;
mkdir $repo_name;
system "svnadmin create $repo_name";
chdir $pwd;
copy("anonymous.conf", "$playground/$repo_name/conf/svnserve.conf") or die $!;
print " done\n";


# Let's go to work
print "> Committing test files ...";
chdir $work;
system "svn co '$repo_host/$repo_name' > '$log_file'";
chdir $repo_name;

# Simple test with one file and a set of different log messages
$file = "test";
system "touch '$file' >> '$log_file'";
system "svn add '$file' >> '$log_file'";
foreach $msg (@messages) { 
	open(FILE, ">>$file");
	print FILE "another test line...\n"; 
	close(FILE);
	system "svn commit -m '$msg' >> '$log_file'";
}
print " done\n";


# Dump and compare log messages
print "> Running rsvndump ...";
system "$pwd/rsvndump @args '$repo_host/$repo_name' > dump 2>> '$log_file'";
print " done\n";
print "> Comparing log messages ...";
$temp_repo = "logtest_temp";
mkdir "$playground/$temp_repo";
system "svnadmin create '$playground/$temp_repo'";
system "cat dump | svnadmin load '$playground/$temp_repo' >> '$log_file'";
system "svn -r 0:HEAD log $repo_host/$temp_repo >> $log_file";
open(PIPE, "svn -r 0:HEAD log '$repo_host/$temp_repo' |");
$i = 0;
while (<PIPE>) {
	# A little hack to get log message from svn log
	/$delim/ and <PIPE> and <PIPE> and next;
	$comment = $_;
	while (<PIPE>) {
		if (/$delim/) {
			<PIPE> and <PIPE> and last;
		} else {
			$comment = $comment . $_;
		}
	}
	chomp $comment;
	if ($comment ne $messages[$i]) {
		print "ERROR: Log message '$messages[$i]' was not dumped correctly (dumped as '$comment')\n";
		die $!;
	}
	++$i;
}
close(PIPE);
print " ok\n";

chdir $pwd;
exit 0;
