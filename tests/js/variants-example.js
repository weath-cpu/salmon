// Example for using a user-defined variant with custom pieces through ffish.js.
//
// Build the module first (see README.md), then run from the repository root:
//
//     node tests/js/variants-example.js
//
// The script loads src/variants.ini and uses its "tank" variant, whose piece is
// defined with the unblocked ("~") rider modifier and the "sweep" move effect:
// all pieces between origin and destination are removed by the move.

const fs = require('fs');
const path = require('path');

// The emscripten 1.39 module used for ffish.js loads its .wasm file from disk,
// but falls back to fetch() when Node provides a global one (Node >= 18), which
// can not handle local paths. Turn it off so the file is read normally.
if (typeof globalThis.fetch === 'function')
  delete globalThis.fetch;

const ffish = require('./ffish.js');

function main() {
  const configPath = path.join(__dirname, '..', '..', 'src', 'variants.ini');
  ffish.loadVariantConfig(fs.readFileSync(configPath, 'utf8'));

  // The tank replaces the knight, and is in this position on e1.
  const board = new ffish.Board('tank', 'k7/8/8/8/4r3/3pp3/2P5/K3N3 w - - 0 1');

  const tankMoves = board.legalMoves().split(' ').filter(move => move.startsWith('e1'));
  console.log('tank moves:', tankMoves.join(' '));

  // e1e4 captures the rook on e4 and removes the black pawn on e3 on the way
  if (!board.push('e1e4'))
    throw new Error('ffish.js rejected the sweeping move e1e4');

  const fen = board.fen();
  board.delete();
  console.log('after e1e4:', fen);

  // The pawn on d3 is still there, the pawn on e3 is not: that is the sweep.
  const expected = 'k7/8/8/8/4N3/3p4/2P5/K7 b - - 0 1';
  if (fen !== expected)
    throw new Error(`sweep not applied, expected ${expected}, got ${fen}`);

  console.log('sweep OK');

  // The flipper is defined with the "flip" move effect: e1c3 passes the black
  // pawn on d2 and turns it into a white pawn.
  const flipper = new ffish.Board('flipper', 'k7/8/8/8/8/8/3p4/K3A3 w - - 0 1');

  if (!flipper.push('e1c3'))
    throw new Error('ffish.js rejected the flipping move e1c3');

  const flippedFen = flipper.fen();
  flipper.delete();
  console.log('after e1c3 (flipper):', flippedFen);

  const expectedFlip = 'k7/8/8/8/8/2A5/3P4/K7 b - - 0 1';
  if (flippedFen !== expectedFlip)
    throw new Error(`flip not applied, expected ${expectedFlip}, got ${flippedFen}`);

  console.log('flip OK');
}

if (typeof ffish.Board === 'function')
  main();
else
  ffish['onRuntimeInitialized'] = main;
