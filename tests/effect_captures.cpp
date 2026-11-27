/*
  Check that moves whose move effect gains material are searched like captures.

  A sweep that removes a piece or a flip that changes the side of a piece wins
  material on the way, but the move itself does not capture on the destination
  square, so generate<CAPTURES>() does not produce it. Such a move has to be
  ordered with the captures and has to be searched by the quiescence search,
  otherwise it is only tried as a quiet move and pruned by the quiet move
  pruning. The test builds the move picker for a few positions and requires the
  effect captures to be part of the capture stage, to be ordered ahead of the
  quiet moves, and not to be returned twice.
*/

#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

#include "bitboard.h"
#include "evaluate.h"
#include "movegen.h"
#include "movepick.h"
#include "piece.h"
#include "position.h"
#include "psqt.h"
#include "thread.h"
#include "uci.h"
#include "variant.h"

using namespace Stockfish;

namespace {

struct EffectTest
{
  const char* variant;
  const char* fen;
  std::vector<const char*> gains; // effect captures
  const char* quietEffect;        // effect move without a gain on the way
};

const EffectTest positions[] = {
  // the tank on a4 sweeps the rook on e4 on its way to f4, g4, and h4
  { "tank", "7k/8/8/8/N3r3/8/8/K7 w - - 0 1", { "a4f4", "a4g4", "a4h4" }, "a4a5" },
  // the flipper on c1 flips the rook on f4 into a white rook
  { "flipper", "7k/8/8/8/5r2/8/8/K1A5 w - - 0 1", { "c1g5", "c1h6" }, "c1b2" },
};

bool contains(const std::vector<Move>& moves, Move m) {
  return std::find(moves.begin(), moves.end(), m) != moves.end();
}

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

  // Zeros everywhere, so that the history based ordering does not depend on
  // values left over from an earlier run
  static ButterflyHistory mainHistory;
  static LowPlyHistory lowPlyHistory;
  static GateHistory gateHistory;
  static CapturePieceToHistory captureHistory;
  static PieceToHistory continuationHistory;

  mainHistory.fill(0);
  lowPlyHistory.fill(0);
  gateHistory.fill(0);
  captureHistory.fill(0);
  continuationHistory.fill(0);

  const PieceToHistory* contHist[6] = { &continuationHistory, &continuationHistory,
                                        &continuationHistory, &continuationHistory,
                                        &continuationHistory, &continuationHistory };

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

      std::vector<Move> gains;
      for (const char* str : test.gains)
      {
          std::string uci = str;
          const Move m = UCI::to_move(pos, uci);
          if (m == MOVE_NONE)
          {
              std::cerr << "unknown move " << str << " for " << test.variant << " " << test.fen
                        << std::endl;
              return 1;
          }
          gains.push_back(m);
      }

      std::string quietUci = test.quietEffect;
      const Move quiet = UCI::to_move(pos, quietUci);
      if (quiet == MOVE_NONE)
      {
          std::cerr << "unknown move " << test.quietEffect << " for " << test.variant << " "
                    << test.fen << std::endl;
          return 1;
      }

      // The test positions have to show the gap, i.e. the moves have to win
      // material without being captures
      for (size_t i = 0; i < gains.size(); ++i)
      {
          const Move m = gains[i];
          if (pos.capture(m))
          {
              std::cerr << test.gains[i] << " is a capture in " << test.variant << " " << test.fen
                        << ", the test position does not show the gap" << std::endl;
              ++failures;
              continue;
          }

          if (pos.effect_value(m) <= VALUE_ZERO)
          {
              std::cerr << "effect_value() does not see the gain of " << test.gains[i] << " in "
                        << test.variant << " " << test.fen << std::endl;
              ++failures;
          }
      }

      if (pos.capture(quiet) || pos.effect_value(quiet) > VALUE_ZERO)
      {
          std::cerr << test.quietEffect << " gains material in " << test.variant << " " << test.fen
                    << ", the test position does not show the gap" << std::endl;
          ++failures;
      }

      // The quiescence search only gets the capture stages of the move picker
      std::vector<Move> qsearchMoves;
      {
          MovePicker mp(pos, MOVE_NONE, Depth(-1), nullptr, &gateHistory, &captureHistory,
                        nullptr, SQ_NONE);

          for (Move m = mp.next_move(); m != MOVE_NONE; m = mp.next_move())
              qsearchMoves.push_back(m);
      }

      for (size_t i = 0; i < gains.size(); ++i)
          if (!contains(qsearchMoves, gains[i]))
          {
              std::cerr << "the quiescence search does not search the effect capture "
                        << UCI::move(pos, gains[i]) << " in " << test.variant << " " << test.fen
                        << std::endl;
              ++failures;
          }

      if (contains(qsearchMoves, quiet))
      {
          std::cerr << "the quiescence search searches the effect move without gain "
                    << UCI::move(pos, quiet) << " in " << test.variant << " " << test.fen
                    << std::endl;
          ++failures;
      }

      // The main search orders the capture stages before the quiet moves
      std::vector<Move> searchMoves;
      {
          const Move killers[2] = { MOVE_NONE, MOVE_NONE };
          MovePicker mp(pos, MOVE_NONE, Depth(6), &mainHistory, &gateHistory, &lowPlyHistory,
                        &captureHistory, contHist, MOVE_NONE, killers, 0);

          for (Move m = mp.next_move(); m != MOVE_NONE; m = mp.next_move())
              searchMoves.push_back(m);
      }

      for (size_t i = 0; i < searchMoves.size(); ++i)
          for (size_t j = i + 1; j < searchMoves.size(); ++j)
              if (searchMoves[i] == searchMoves[j])
              {
                  std::cerr << "the search returns " << UCI::move(pos, searchMoves[i])
                            << " twice in " << test.variant << " " << test.fen << std::endl;
                  ++failures;
              }

      // The first move that neither captures nor gains by its effect is a quiet
      // move, so every effect capture has to be ordered before it
      size_t firstQuiet = searchMoves.size();
      for (size_t i = 0; i < searchMoves.size(); ++i)
          if (!pos.capture(searchMoves[i]) && pos.effect_value(searchMoves[i]) <= VALUE_ZERO)
          {
              firstQuiet = i;
              break;
          }

      for (size_t i = 0; i < gains.size(); ++i)
      {
          const auto found = std::find(searchMoves.begin(), searchMoves.end(), gains[i]);
          if (found == searchMoves.end())
          {
              std::cerr << "the search does not generate the effect capture "
                        << UCI::move(pos, gains[i]) << " in " << test.variant << " " << test.fen
                        << std::endl;
              ++failures;
          }
          else if (size_t(found - searchMoves.begin()) > firstQuiet)
          {
              std::cerr << "the search orders the effect capture " << UCI::move(pos, gains[i])
                        << " behind the quiet moves in " << test.variant << " " << test.fen
                        << std::endl;
              ++failures;
          }
      }
  }

  Threads.set(0);

  if (failures)
  {
      std::cerr << failures << " failure(s)" << std::endl;
      return 1;
  }

  std::cout << "effect capture testing OK" << std::endl;
  return 0;
}
