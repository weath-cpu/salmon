#!/bin/bash
# Check that moves whose move effect gains material are ordered with the captures
# and searched by the quiescence search instead of being treated as quiet moves.
# Needs an engine build in the current directory, so pass the same options as
# used for it, e.g.
#   ../tests/effect_captures.sh ARCH=x86-64 largeboards=yes debug=yes

error()
{
  echo "effect capture testing failed on line $1"
  exit 1
}
trap 'error ${LINENO}' ERR

echo "effect capture testing started"

make -s effect_captures_test "$@"
./effect_captures_test variants.ini
rm -f effect_captures_test effect_captures_test.o

echo "effect capture testing OK"
