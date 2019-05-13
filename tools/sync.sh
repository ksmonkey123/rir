#!/bin/bash

set -e

CURRENT_DIR=`pwd`
SCRIPTPATH=`cd $(dirname "$0") && pwd`
if [ ! -d $SCRIPTPATH ]; then
    echo "Could not determine absolute dir of $0"
    echo "Maybe accessed with symlink"
fi
SRC_DIR=`cd ${SCRIPTPATH}/.. && pwd`
. "${SCRIPTPATH}/script_include.sh"


if [[ "$OSTYPE" == "darwin"* ]]; then
    USING_OSX=1
fi

git submodule update --init

# check the .git of the rjit directory
test -d ${SRC_DIR}/.git
IS_GIT_CHECKOUT=$?

if [ $IS_GIT_CHECKOUT -eq 0 ]; then
    ${SRC_DIR}/tools/install_hooks.sh
fi

function build_r {
    NAME=$1
    R_DIR="${SRC_DIR}/external/${NAME}"

    cd $R_DIR

    if [[ $(git diff --shortstat 2> /dev/null | tail -n1) != "" ]]; then
        echo "$NAME repo is dirty"
        exit 1
    fi

    # unpack cache of recommended packages
    cd src/library/Recommended/
    tar xf ../../../../custom-r/cache_recommended.tar
    cd ../../..
    tools/rsync-recommended || true

    if [ ! -f $R_DIR/Makefile ]; then
        echo "-> configure gnur"
        cd $R_DIR
        if [ $USING_OSX -eq 1 ]; then
            # Mac OSX
            F77="gfortran -arch x86_64" FC="gfortran -arch x86_64" CXXFLAGS="-g3 -O2" CFLAGS="-g3 -O2" ./configure --enable-R-shlib --with-internal-tzcode --with-ICU=no || cat config.log
        else
            CXXFLAGS="-g3 -O2" CFLAGS="-g3 -O2" ./configure --with-ICU=no
        fi
    fi
    
    if [ ! -f $R_DIR/doc/FAQ ]; then
        cd $R_DIR
        touch doc/FAQ
    fi
    if [ ! -f $R_DIR/SVN-REVISION ]; then

        # R must either be built from a svn checkout, or from the tarball generated by make dist
        # this is a workaround to build it from a git mirror
        # see https://github.com/wch/r-source/wiki/Home/6d35777dcb772f86371bf221c194ca0aa7874016#building-r-from-source
        echo -n 'Revision: ' > SVN-REVISION
        # get the latest revision that is not a rir patch
        REV=$(git log --grep "git-svn-id" -1 --format=%B | grep "^git-svn-id" | sed -E 's/^git-svn-id: https:\/\/svn.r-project.org\/R\/[^@]*@([0-9]+).*$/\1/')
        # can fail on shallow checkouts, so let's put the last known there
        if [ "$REV" == "" ]; then
          REV='74948'
        fi
        echo $REV >> SVN-REVISION
        echo -n 'Last Changed Date: ' >> SVN-REVISION
        REV_DATE=$(git log --grep "git-svn-id" -1 --pretty=format:"%ad" --date=iso | cut -d' ' -f1)
        # can fail on shallow checkouts, so let's put the last known there
        if [ "$REV_DATE" == "" ]; then
          REV_DATE='2018-07-02'
        fi
        echo $REV_DATE >> SVN-REVISION

        rm -f non-tarball
    fi
}

build_r custom-r
