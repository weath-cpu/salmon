#!/bin/bash
# verify that invalid variant configuration is reported, in particular that
# invalid bent (multi-leg) piece notation such as "F>R" is rejected

error()
{
  echo "variant config testing failed on line $1"
  exit 1
}
trap 'error ${LINENO}' ERR

echo "variant config testing started"

config=$(mktemp)

cat > "$config" <<'EOF'
[badpiece1:chess]
customPiece1 = z:Z>R

[badpiece2:chess]
customPiece1 = z:F>RR

[badpiece3:chess]
customPiece1 = z:W>B

[badpiece4:chess]
customPiece1 = z:F>

[badpiece5:chess]
customPiece1 = z:F>R>R

[badpiece6:chess]
customPiece1 = z:cF>mR
EOF

output=$(./stockfish check "$config" 2>&1)

for variant in badpiece1 badpiece2 badpiece3 badpiece4 badpiece5 badpiece6; do
  if ! grep -q "Variant '$variant' rejected: Invalid bent piece notation" <<< "$output"; then
    echo "error: invalid notation of '$variant' was not rejected"
    rm -f "$config"
    exit 1
  fi
done

# The notation used by the example variants must still be accepted
if ./stockfish check variants.ini 2>&1 | grep -q "Invalid bent piece notation"; then
  echo "error: valid bent piece notation was rejected"
  rm -f "$config"
  exit 1
fi

rm -f "$config"

echo "variant config testing OK"
