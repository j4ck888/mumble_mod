#! /usr/bin/perl -w

use strict;
use warnings;
use Carp;
use Switch;
use Archive::Tar;
use LWP::UserAgent;

my %files;
my $ver;

system("rm murmur-*");

foreach my $pro ("main.pro", "speexbuild/speexbuild.pro", "src/mumble/mumble.pro", "src/murmur/murmur.pro", "src/mumble.pri") {
  open(F, $pro) or croak "Failed to open $pro";
  print "Processing $pro\n";
  while(<F>) {
    chomp();
    if (/^\s*(\w+)\s*?[\+\-\s]=\s*(.+)$/) {
      my ($var,$value)=(lc $1,$2);
      switch ($var) {
        case "version" {
          croak "Versions don't match" if (defined($ver) && ($ver ne $value));
          $ver=$value;
        }
      }
    }
  }
  close(F);
}

if ($#ARGV < 0) {
  open(F, "git describe origin/master|");
  $ver = "";
  while (<F>) {
    chomp();
    $ver .= $_;
  }
  close(F);
  print "REVISION $ver\n";
} else {
  $ver=$ARGV[0];
}

system("/usr/local/Trolltech/Qt-4.6.1/bin/qmake CONFIG+=static CONFIG+=no-client -recursive");
system("make distclean");
unlink("src/murmur/Murmur.h");
unlink("src/murmur/Murmur.cpp");
unlink("src/murmur/Mumble.pb.h");
unlink("src/murmur/Mumble.pb.cc");
system("PKG_CONFIG_PATH=/usr/local/lib/pkgconfig/:/usr/lib/pkgconfig:/usr/local/ssl/lib/pkgconfig /usr/local/Trolltech/Qt-4.6.1/bin/qmake CONFIG+=static CONFIG+=no-client -recursive");
system("make");
system("strip release/murmurd");

$files{"murmur.x86"}="release/murmurd";
$files{"LICENSE"}="installer/gpl.txt";
$files{"README"}="README";
$files{"CHANGES"}="CHANGES";
$files{"murmur.pl"}="scripts/murmur.pl";
$files{"weblist.pl"}="scripts/weblist.pl";
$files{"icedemo.php"}="scripts/icedemo.php";
$files{"weblist.php"}="scripts/weblist.php";
$files{"murmur.ini"}="scripts/murmur.ini";
$files{"Murmur.ice"}="src/murmur/Murmur.ice";

my $tar = new Archive::Tar();
my $blob;
my $dir="murmur-static_x86-$ver/";

foreach my $file (sort keys %files) {
  print "Adding $file\n";
  open(F, $files{$file}) or croak "Missing $file";
  sysread(F, $blob, 1000000000);
  my %opts;
  $opts{'uid'}=0;
  $opts{'gid'}=0;
  $opts{'uname'}='root';
  $opts{'gname'}='root';
  if (($file =~ /\.x86$/) || ($file =~ /\.pl$/)) {
    $opts{'mode'}=0555;
  } elsif (($file =~ /\.ini$/)) {
    $opts{'mode'}=0644;
  } else {
    $opts{'mode'}=0444;
  }
  $tar->add_data($dir . $file, $blob, \%opts);
  close(F);
}

$tar->write("murmur-static_x86-${ver}.tar");
system("bzip2 -9 murmur-static_x86-${ver}.tar");
system("/usr/bin/scp","-4","murmur-static_x86-${ver}.tar.bz2", "slicer\@mumble.hive.no:/var/www/snapshot/");
system("/usr/bin/ssh","-4","slicer\@mumble.hive.no","/mumble/snapshot.pl");
