-- 1. Create the User table
CREATE TABLE "User" (
    user_id BIGSERIAL PRIMARY KEY,
    username VARCHAR(32) NOT NULL,
    password VARCHAR(256) NOT NULL,
    info TEXT,
    created_at TIMESTAMP DEFAULT NOW()
);

-- Create the specific index for username as requested
CREATE INDEX idx_user_username ON "User"(username);


-- 2. Create the Match table
CREATE TABLE "Match" (
    match_id BIGSERIAL PRIMARY KEY,
    started_at TIMESTAMP NOT NULL,
    ended_at TIMESTAMP,
    winner_user_id BIGINT,
    is_draw BOOLEAN DEFAULT FALSE,
    log_path TEXT,

    -- Foreign Key to User table
    CONSTRAINT fk_match_winner
        FOREIGN KEY (winner_user_id)
        REFERENCES "User"(user_id),

    -- Constraint: started_at must be before or equal to ended_at
    CONSTRAINT chk_match_time
        CHECK (ended_at IS NULL OR started_at <= ended_at),

    -- Constraint: Logic for draw vs winner
    -- Note: This enforces that if is_draw is FALSE, there MUST be a winner.
    CONSTRAINT chk_match_outcome
        CHECK (
            (is_draw = TRUE AND winner_user_id IS NULL)
            OR
            (is_draw = FALSE AND winner_user_id IS NOT NULL)
        )
);


-- 3. Create the MatchParticipant table
CREATE TABLE "MatchParticipant" (
    match_id BIGINT NOT NULL,
    user_id BIGINT NOT NULL,
    role VARCHAR(12) DEFAULT 'player',
    team SMALLINT,
    turn_order SMALLINT,
    score INT DEFAULT 0,

    -- Composite Primary Key
    PRIMARY KEY (match_id, user_id),

    -- Foreign Keys
    CONSTRAINT fk_participant_match
        FOREIGN KEY (match_id)
        REFERENCES Match(match_id)
        ON DELETE CASCADE,

    CONSTRAINT fk_participant_user
        FOREIGN KEY (user_id)
        REFERENCES "User"(user_id),

    -- Optional: Limit role to specific values
    CONSTRAINT chk_participant_role
        CHECK (role IN ('player', 'bot'))
);