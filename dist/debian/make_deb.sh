#!/bin/sh
#
# .deb-Package generation script for rsvndump

if [ -z $1 ] 
then
	echo "Usage: make_deb.sh <version>"
	exit 1
fi


VERSION=$1
ARCH=`dpkg-architecture -l|grep DEB_HOST_ARCH=|sed "s/.*=\(.*\)\$/\1/"`

echo -n "Fetching tag $VERSION..."
svn export svn://slug/rsvndump/tags/rsvndump-$VERSION "rsvndump-$VERSION"
#cp -R ../../src/* "rsvndump-$VERSION"

cd rsvndump-$VERSION
echo 
echo -n "Removing unused files..."
rm -rf doc scripts web
cd ..

echo 
echo -n "Compressing source..."
tar -cjf "rsvndump-$VERSION.tar.bz2" "rsvndump-$VERSION"

echo "Running dh_make..."
cd "rsvndump-$VERSION"
yes|dh_make --single --copyright gpl --email jonas.gehring@boolsoft.org --packagename rsvndump -f "../rsvndump-$VERSION.tar.bz2"
cd ..

echo 
echo -n "Copying neccessary files.."
cp control copyright changelog rules "rsvndump-$VERSION/debian"
sed -i s/\$VERSION/$VERSION/ "rsvndump-$VERSION/debian/control"
sed -i s/\$ARCH/$ARCH/ "rsvndump-$VERSION/debian/control"

echo 
echo -n "Everything set up. Pres <ENTER> to generate the package: " 
read stdin
echo 

cd "rsvndump-$VERSION"
fakeroot dpkg-buildpackage
cd ..

echo 
echo "***Package generation finished***"
echo

echo 
echo -n "Removing temporary files..."
rm -rf "rsvndump-$VERSION/"
echo 

exit 0

