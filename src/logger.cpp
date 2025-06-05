#include "logger.hpp"

Paper::ConstLoggerContext<12UL> getLogger() {
    static auto fastContext = Paper::Logger::WithContext<"TriickSaber">();
    return fastContext;
}