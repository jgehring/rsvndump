#!/bin/sh
#
# .deb-Package generation script for rsvndump

if [ -z $1 ] 
then
	echo "Usage: make_deb.sh <version>"
	exit 1
fi


PROGRAM="rsvndump"
VERSION=$1
ARCH=`dpkg-architecture -l|grep DEB_HOST_ARCH=|sed "s/.*=\(.*\)\$/\1/"`
MIRROR="http://rsvndump.sourceforge.net"

#echo -n "Fetching tag $VERSION..."
#svn export svn://slug/rsvndump/tags/rsvndump-$VERSION "rsvndump-$VERSION"
##cp -R ../../src/* "rsvndump-$VERSION"
#
#cd rsvndump-$VERSION
#echo 
#echo -n "Removing unused files..."
#rm -rf doc scripts web scripts tests dist
#rm -rf TODO README
#cd ..
#
#echo 
#echo -n "Compressing source..."
#tar -cjf "rsvndump-$VERSION.tar.bz2" "rsvndump-$VERSION"

echo "Fetching release from ${MIRROR}..."
wget -q "${MIRROR}/${PROGRAM}-${VERSION}.tar.bz2" || exit 1
tar -xjf "${PROGRAM}-${VERSION}.tar.bz2"

echo "Removing unused files..."
cd "${PROGRAM}-${VERSION}"
rm -rf TODO
cd ..

echo "Running dh_make..."
cd "${PROGRAM}-$VERSION"
yes|dh_make --single --copyright gpl --email jonas.gehring@boolsoft.org --packagename ${PROGRAM} -f "../${PROGRAM}-$VERSION.tar.bz2"
cd ..

echo 
echo -n "Copying neccessary files.."
cp control copyright changelog rules "${PROGRAM}-$VERSION/debian"
sed -i s/\$ARCH/$ARCH/ "${PROGRAM}-$VERSION/debian/control"

echo 
echo -n "Everything set up. Pres <ENTER> to generate the package: " 
read stdin
echo 

cd "${PROGRAM}-$VERSION"
fakeroot dpkg-buildpackage
cd ..

echo 
echo "***Package generation finished***"
echo

echo 
echo -n "Removing temporary files..."
rm -rf "${PROGRAM}-$VERSION/"
echo 

exit 0

