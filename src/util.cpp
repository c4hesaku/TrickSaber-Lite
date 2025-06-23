#include "util.hpp"
#include "main.hpp"
#include "logger.hpp"
#include "metacore/shared/game.hpp"


namespace TrickUtils {

void Utils::DisableScoreSubmission() {
    MetaCore::Game::SetScoreSubmission(MOD_ID, false); // 💫
}

}