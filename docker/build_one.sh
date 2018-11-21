#!/bin/bash

. builditall_functions

TMPDIR=$(mktemp -d)
trap 'rm -fr ${TMPDIR}' 0
touch $TMPDIR/failures
touch $TMPDIR/successes

case $1 in
  centos)
    buildcentos
    ;;
  suse)
    buildsuse
    ;;
  ubuntu)
    buildubuntu
    ;;
  debian)
    builddebian
    ;;
  fedora)
    buildfedora
    ;;
  *)
    echo "you must specify a build"
    exit 1
    ;;
esac

wait
printf "\nSuccesses:\n"
cat $TMPDIR/successes
printf "\nFailures:\n"
cat $TMPDIR/failures

test ! -s $TMPDIR/failures
