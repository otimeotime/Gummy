#include "MatchRecorder.hpp"

#include <pqxx/pqxx>

#include <cstdlib>
#include <iostream>

namespace {
std::string EnvOr(const char* key, const std::string& def) {
    const char* v = std::getenv(key);
    if (!v || !*v) return def;
    return std::string(v);
}
}

MatchRecorder::MatchRecorder() = default;

std::string MatchRecorder::BuildConnString() const {
    // Defaults match the existing service server hardcoded values.
    const std::string host = EnvOr("DB_HOST", "127.0.0.1");
    const std::string port = EnvOr("DB_PORT", "5432");
    const std::string dbname = EnvOr("DB_NAME", "gummydatabase");
    const std::string user = EnvOr("DB_USER", "postgres");
    const std::string pass = EnvOr("DB_PASS", "Hehehe123");

    return "host=" + host + " port=" + port + " dbname=" + dbname + " user=" + user + " password=" + pass;
}

bool MatchRecorder::SaveMatch(uint32_t matchId,
                              const std::string& startedAt,
                              const std::string& endedAt,
                              bool isDraw,
                              uint32_t winnerUserId,
                              const std::string& logPath,
                              const std::vector<MatchParticipantRecord>& participants) {
    try {
        pqxx::connection conn(BuildConnString());
        pqxx::work W(conn);

        // Upsert match row. Keep started_at stable if already present.
        // Constraints:
        // - if is_draw = TRUE => winner_user_id must be NULL
        // - if is_draw = FALSE => winner_user_id must be NOT NULL
        const bool draw = isDraw;
        if (draw) {
            W.exec_params(
                "INSERT INTO \"Match\" (match_id, started_at, ended_at, winner_user_id, is_draw, log_path) "
                "VALUES ($1, $2::timestamp, $3::timestamp, NULL, TRUE, $4) "
                "ON CONFLICT (match_id) DO UPDATE SET "
                "ended_at = EXCLUDED.ended_at, "
                "winner_user_id = NULL, "
                "is_draw = TRUE, "
                "log_path = EXCLUDED.log_path",
                (long long)matchId,
                startedAt,
                endedAt,
                logPath);
        } else {
            if (winnerUserId == 0) {
                throw std::runtime_error("SaveMatch: winnerUserId=0 for non-draw match");
            }
            W.exec_params(
                "INSERT INTO \"Match\" (match_id, started_at, ended_at, winner_user_id, is_draw, log_path) "
                "VALUES ($1, $2::timestamp, $3::timestamp, $4, FALSE, $5) "
                "ON CONFLICT (match_id) DO UPDATE SET "
                "ended_at = EXCLUDED.ended_at, "
                "winner_user_id = EXCLUDED.winner_user_id, "
                "is_draw = FALSE, "
                "log_path = EXCLUDED.log_path",
                (long long)matchId,
                startedAt,
                endedAt,
                (long long)winnerUserId,
                logPath);
        }

        // Replace participants for this match.
        W.exec_params("DELETE FROM \"MatchParticipant\" WHERE match_id = $1", (long long)matchId);
        for (const auto& p : participants) {
            if (p.userId == 0) continue;
            W.exec_params(
                "INSERT INTO \"MatchParticipant\" (match_id, user_id, is_player, team, turn_order, score) "
                "VALUES ($1, $2, $3, $4, $5, $6)",
                (long long)matchId,
                (long long)p.userId,
                p.isPlayer,
                p.team,
                p.turnOrder,
                p.score);
        }

        // Keep the BIGSERIAL sequence from falling behind when we insert explicit match_id values.
        // Note: pg_get_serial_sequence expects the table name as text; for quoted identifiers
        // we pass '"Match"' (double quotes inside the string literal).
        W.exec(
            "SELECT setval(pg_get_serial_sequence('\"Match\"','match_id'), "
            "GREATEST((SELECT COALESCE(MAX(match_id),0) FROM \"Match\"), 1), true)");

        W.commit();
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[MatchRecorder] Failed to save match " << matchId << ": " << e.what() << std::endl;
        return false;
    }
}
