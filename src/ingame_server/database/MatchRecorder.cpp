#include "MatchRecorder.hpp"

#include <pqxx/pqxx>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>

namespace {
std::string EnvOr(const char* key, const std::string& def) {
    const char* v = std::getenv(key);
    if (!v || !*v) return def;
    return std::string(v);
}

struct EloUpdate {
    long long userId = 0;
    int oldElo = 0;
    int newElo = 0;
    int delta = 0;
};

double ClampDeltaForDivision(double delta) {
    // The spec mentions `min(min(10,delta),1)` which would allow division by 0 when delta=0.
    // We interpret the intent as clamping delta to [1, 10] for the denominator.
    const double clamped = std::min(10.0, std::max(0.0, delta));
    return std::max(1.0, clamped);
}

double ClampDeltaForBonus(double delta) {
    return std::min(10.0, std::max(0.0, delta));
}

std::pair<EloUpdate, EloUpdate> ComputeEloDeltas(bool isDraw,
                                                 long long userAId,
                                                 int eloA,
                                                 long long userBId,
                                                 int eloB,
                                                 long long winnerUserId) {
    const double delta = std::fabs((double)eloA - (double)eloB);
    const double denom = ClampDeltaForDivision(delta);
    const double deltaBonus = ClampDeltaForBonus(delta);

    EloUpdate a{userAId, eloA, eloA, 0};
    EloUpdate b{userBId, eloB, eloB, 0};

    if (!isDraw) {
        const double change = 50.0 / denom;
        const int rounded = (int)std::lround(change);
        if (winnerUserId == userAId) {
            a.delta = +rounded;
            b.delta = -rounded;
        } else {
            a.delta = -rounded;
            b.delta = +rounded;
        }
    } else {
        // Higher-elo loses: 10/denom + min(10, delta). Lower-elo gains same.
        const double change = 10.0 / denom + deltaBonus;
        const int rounded = (int)std::lround(change);
        if (eloA == eloB) {
            // Tie: treat as zero-sum but no clear higher/lower; simplest is no change.
            a.delta = 0;
            b.delta = 0;
        } else if (eloA > eloB) {
            a.delta = -rounded;
            b.delta = +rounded;
        } else {
            a.delta = +rounded;
            b.delta = -rounded;
        }
    }

    a.newElo = std::max(0, a.oldElo + a.delta);
    b.newElo = std::max(0, b.oldElo + b.delta);
    return {a, b};
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

        // Idempotency: if this match row was already finalized (ended_at set), do not apply ELO again.
        bool alreadyFinalized = false;
        {
            const auto existing = W.exec_params(
                "SELECT ended_at IS NOT NULL AS finalized FROM \"Match\" WHERE match_id = $1",
                (long long)matchId);
            if (!existing.empty()) {
                alreadyFinalized = existing[0][0].as<bool>(false);
            }
        }

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

        // Apply ELO changes (2-player matches) exactly once per finalized match.
        if (!alreadyFinalized) {
            long long p0 = 0;
            long long p1 = 0;
            for (const auto& p : participants) {
                if (!p.isPlayer) continue;
                if (p.userId == 0) continue;
                if (p0 == 0) {
                    p0 = (long long)p.userId;
                } else if (p1 == 0 && (long long)p.userId != p0) {
                    p1 = (long long)p.userId;
                }
            }

            if (p0 != 0 && p1 != 0) {
                // Lock both user rows so concurrent matches can't interleave updates.
                const auto rows = W.exec_params(
                    "SELECT user_id, elo FROM \"User\" WHERE user_id IN ($1, $2) FOR UPDATE",
                    p0,
                    p1);

                int elo0 = 300;
                int elo1 = 300;
                for (const auto& r : rows) {
                    const long long uid = r[0].as<long long>();
                    const int elo = r[1].as<int>(300);
                    if (uid == p0) elo0 = elo;
                    if (uid == p1) elo1 = elo;
                }

                const auto updates = ComputeEloDeltas(
                    isDraw,
                    p0,
                    elo0,
                    p1,
                    elo1,
                    (long long)winnerUserId);

                // For non-draw matches, ensure the provided winner is one of the participants.
                if (!isDraw) {
                    if ((long long)winnerUserId != p0 && (long long)winnerUserId != p1) {
                        throw std::runtime_error("SaveMatch: winnerUserId not among participants");
                    }
                }

                W.exec_params("UPDATE \"User\" SET elo = $1 WHERE user_id = $2", updates.first.newElo, updates.first.userId);
                W.exec_params("UPDATE \"User\" SET elo = $1 WHERE user_id = $2", updates.second.newElo, updates.second.userId);
            }
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
