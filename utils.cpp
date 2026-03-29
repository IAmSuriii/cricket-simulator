/*
 * utils.cpp
 
 * Implements the helper functions declared in utils.h.

 */

#include "utils.h"

// ─ elapsed_us 
/*
 * Reads CLOCK_MONOTONIC and returns microseconds elapsed
 * since match_start_time (set at the start of each innings).
 */
long elapsed_us() {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (now.tv_sec  - match_start_time.tv_sec)  * 1000000L +
           (now.tv_nsec - match_start_time.tv_nsec) / 1000L;
}

// ─ ts 
/*
 * Builds a "[T+XXXXms] " prefix used by every cout log line
 * so readers can see the wall-clock order of events.
 */
string ts() {
    long us = elapsed_us();
    long ms = us / 1000;
    ostringstream oss;
    oss << "[T+" << setw(6) << ms << "ms] ";
    return oss.str();
}

// ─ signal_stroke_done 
/*
 * The batsman_thread calls this after every delivery path
 * (normal hit, wide, run, or dismissal).  It sets stroke_done
 * and wakes umpire_thread which is blocked on umpire_cv.
 */
void signal_stroke_done() {
    pthread_mutex_lock(&umpire_mutex);
    stroke_done = true;
    pthread_cond_signal(&umpire_cv);
    pthread_mutex_unlock(&umpire_mutex);
}

// ─ reset_runout_state 
/*
 * Clears every flag used by the run-out / deadlock resolution
 * protocol so a fresh run-call episode can begin cleanly.
 * Called by batsman_thread just before initiating a run-call.
 */
void reset_runout_state() {
    striker_wants_end2    = false;
    nonstriker_wants_end1 = false;
    striker_blocked       = false;
    nonstriker_blocked    = false;
    runout_decided        = false;
    nonstriker_out        = false;
    nonstriker_run_flag   = false;
    runout_episode_done   = false;
}

// ─ reset_innings 
/*
 * Full reset called by main() between the two innings.
 * Zeroes all score/wicket counters, clears the Gantt log,
 * resets all flags and mutexes, and re-initialises crease_sem
 * so Innings 2 starts with a completely fresh state.
 */
void reset_innings() {
    global_score      = 0;
    wickets_fallen    = 0;
    balls_bowled      = 0;
    total_deliveries  = 0;
    match_intensity   = 0;
    match_over        = false;
    match_started     = false;
    delivery_start_us = 0;

    active_bowler_id = -1;
    striker_id       = -1;
    non_striker_id   = -1;
    ball_delivered   = false;
    ball_processed   = true;
    legal_delivery   = true;
    stroke_done      = false;

    fielders_responded = 0;
    ball_in_air        = false;
    fielder_caught     = false;

    wicket_pending = false;
    out_batsman_id = -1;

    next_batsman_index = 2;
    gantt_log.clear();
    batting_order.clear();

    // Reset per-bowler atomic stats
    for (int i = 0; i < NUM_BOWLERS; i++) {
        bowler_runs[i]    = 0;
        bowler_wickets[i] = 0;
    }

    // Reset per-batsman wait-time records
    for (int i = 0; i < NUM_BATSMEN; i++) {
        wait_stats[i].t_crease_us = 0;
        wait_stats[i].wait_us     = 0;
    }

    // Reset run-out protocol flags
    striker_wants_end2    = false;
    nonstriker_wants_end1 = false;
    striker_blocked       = false;
    nonstriker_blocked    = false;
    runout_decided        = false;
    nonstriker_out        = false;
    nonstriker_run_flag   = false;
    runout_episode_done   = true;

    // Re-initialise the crease semaphore (2 batsmen allowed on pitch)
    sem_destroy(&crease_sem);
    sem_init(&crease_sem, 0, 2);
}
