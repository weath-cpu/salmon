#!/bin/bash
# Check that Position::pseudo_legal() does not accept a move that
# Position::legal() rejects, in particular a move with a morph move effect that
# would expose the own king. Needs an engine build in the current directory,
# so pass the same options as used for it, e.g.
#   ../tests/pseudo_legal.sh ARCH=x86-64 largeboards=yes debug=yes

error()
{
  echo "pseudo legal testing failed on line $1"
  exit 1
}
trap 'error ${LINENO}' ERR

echo "pseudo legal testing started"

make -s pseudo_legal_test "$@"
./pseudo_legal_test variants.ini
rm -f pseudo_legal_test pseudo_legal_test.o

echo "pseudo legal testing OK"
