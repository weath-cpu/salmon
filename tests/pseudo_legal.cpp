/*
  Check the contract between Position::pseudo_legal() and Position::legal() for
  moves of pieces with a morph move effect.

  A morph replaces the type of the pieces it passes, which can expose the own
  king or rescue it from a check, so such moves have to be judged completely
  instead of being accepted as pseudo legal. The test generates the pseudo
  legal moves of a few positions and requires both functions to agree for them,
  and it checks the number of legal moves so that a wrong agreement (e.g. both
  rejecting a legal move) is caught as well.
*/

#include <iostream>
#include <string>
#include <vector>

#include "bitboard.h"
#include "evaluate.h"
#include "movegen.h"
#include "piece.h"
#include "position.h"
#include "psqt.h"
#include "thread.h"
#include "uci.h"
#include "variant.h"

using namespace Stockfish;

namespace {

struct PositionTest
{
  const char* variant;
  const char* fen;
  int legalMoves;
};

// The legal move counts are the perft depth 1 results of the same positions
// used in tests/perft.sh
const PositionTest positions[] = {
  { "converter", "7k/8/8/8/8/3Cp3/8/4K3 w - - 0 1", 14 }, // morph would check e1
  { "converter", "7k/8/8/8/8/3C1n2/8/4K3 w - - 0 1", 7 }, // morph resolves the check
  { "converter", "k7/8/8/8/8/8/3p4/K3C3 w - - 0 1", 16 },
  { "flipper", "7k/8/8/A7/1b5b/8/8/4K3 w - - 0 1", 3 },
  { "tank", "7k/8/8/8/Nb2n3/8/8/4K3 w - - 0 1", 7 },
  { "extinction-effects", "K6k/8/8/8/T1r5/8/8/2Q5 w - - 0 1", 20 },
  { "extinction-effects", "1k6/8/4Q3/8/2b5/8/8/5A1K w - - 0 1", 20 },
};

} // namespace


int main(int argc, char* argv[]) {

  if (argc < 2)
  {
      std::cerr << "usage: " << argv[0] << " <variant configuration>" << std::endl;
      return 1;
  }

  pieceMap.init();
  variants.init();
  UCI::init(Options);
  PSQT::init(variants.find(Options["UCI_Variant"])->second);
  Bitboards::init();
  Position::init();
  Threads.set(1);
  Eval::NNUE::init();

  Options["VariantPath"] = std::string(argv[1]);

  int failures = 0;

  for (const auto& test : positions)
  {
      auto it = variants.find(test.variant);
      if (it == variants.end())
      {
          std::cerr << "unknown variant: " << test.variant << std::endl;
          return 1;
      }

      // Initialize the piece move tables of the variant, like selecting it in the engine
      UCI::init_variant(it->second);

      Position pos;
      StateInfo st;
      pos.set(it->second, test.fen, false, &st, Threads.main());

      std::vector<Move> pseudoLegalMoves;
      if (pos.checkers())
          for (const auto& m : MoveList<EVASIONS>(pos))
              pseudoLegalMoves.push_back(m);
      else
          for (const auto& m : MoveList<NON_EVASIONS>(pos))
              pseudoLegalMoves.push_back(m);

      for (Move m : pseudoLegalMoves)
      {
          if (!pos.morph_piece(type_of(pos.moved_piece(m))))
              continue;

          const bool pseudoLegal = pos.pseudo_legal(m);
          const bool legal = pos.legal(m);
          if (pseudoLegal == legal)
              continue;

          std::cerr << "pseudo_legal() and legal() disagree for " << test.variant << " "
                    << test.fen << " " << UCI::move(pos, m) << ": pseudo_legal=" << pseudoLegal
                    << " legal=" << legal << std::endl;
          ++failures;
      }

      int legalMoves = int(MoveList<LEGAL>(pos).size());
      if (legalMoves != test.legalMoves)
      {
          std::cerr << "unexpected number of legal moves for " << test.variant << " " << test.fen
                    << ": " << legalMoves << " instead of " << test.legalMoves << std::endl;
          ++failures;
      }
  }

  Threads.set(0);

  if (failures)
  {
      std::cerr << failures << " failure(s)" << std::endl;
      return 1;
  }

  std::cout << "pseudo legal contract testing OK" << std::endl;
  return 0;
}
