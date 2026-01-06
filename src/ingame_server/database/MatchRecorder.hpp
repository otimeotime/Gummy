#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct MatchParticipantRecord {
    uint32_t userId = 0;
    bool isPlayer = true;
    int team = 0;
    int turnOrder = 0;
    int score = 0;
};

class MatchRecorder {
public:
    MatchRecorder();

    // Saves one row in "Match" and N rows in "MatchParticipant".
    // Uses matchId as the database match_id (BIGSERIAL) via explicit insert/upsert.
    bool SaveMatch(uint32_t matchId,
                   const std::string& startedAt,
                   const std::string& endedAt,
                   bool isDraw,
                   uint32_t winnerUserId,
                   const std::string& logPath,
                   const std::vector<MatchParticipantRecord>& participants);

private:
    std::string BuildConnString() const;
};
