# Copyright 1999-2008 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: $

DESCRIPTION="Performs a remote dump of a subversion repository"
HOMEPAGE="http://saubue.boolsoft.org/projects/rsvndump"
SRC_URI="http://saubue.boolsoft.org/stuff/rsvndump-0.4.tar.gz"

LICENSE="GPL-3"
SLOT="0"
KEYWORDS="arm x86"
IUSE=""

DEPEND=">=dev-util/subversion-1.4"
RDEPEND="${DEPEND}"


src_compile() {
	./configure || die "./configure failed"
	emake || die "emake failed"
}

src_install() {
	einstall || die "einstall failed" 
}

