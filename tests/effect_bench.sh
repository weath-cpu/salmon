#!/bin/bash
# Run the benchmark of the variants with user defined move effects and report
# the node counts, so that search changes in these variants are measurable.
# Needs a release engine build in the current directory, e.g.
#   ../tests/effect_bench.sh
#
# Debug builds abort in the tank bench for now, since attackers_to() misses the
# attacks of a piece type whose movement was overridden (see the note in the
# report of the move effect work), so use them only with a release build.

error()
{
  echo "effect bench failed on line $1"
  exit 1
}
trap 'error ${LINENO}' ERR

echo "effect bench started"

for variant in tank flipper converter extinction-effects; do
  nodes=$(printf "setoption name VariantPath value variants.ini\nbench $variant 16 1 13\nquit\n" \
          | ./stockfish 2>&1 | grep "Nodes searched  : " | awk '{print $4}')

  if [ -z "$nodes" ]; then
      echo "no node count for $variant, did the engine abort (debug build)?"
      exit 1
  fi

  echo "$variant: $nodes"
done

echo "effect bench OK"
