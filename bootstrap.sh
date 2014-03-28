#!/bin/sh

die() {
	echo " *** $1 failed with error code $?"
	exit 1
}

cd "`dirname $0`"

set -x
mkdir -p m4

AUTOMAKE="automake --foreign" autoreconf --verbose --force --install || die autoreconf

set +x

echo
echo Success.

