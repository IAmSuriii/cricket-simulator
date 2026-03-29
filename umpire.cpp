/*
 * umpire.cpp
 
 * Implements umpire_thread and deadlock_detector_thread.
 *
 * UMPIRE THREAD — mirrors an OS CPU scheduler:
 *  Round Robin (overs 1-18)  → cycle through bowlers 0-3.
 *  Priority    (overs 19-20) → always assign bowler 4.
 *  Manages wicket substitutions by waking the next batsman.
 *  Checks the Innings-2 target-score run-chase condition.
 *
 * DEADLOCK DETECTOR THREAD — mirrors OS deadlock detection:
 *  Polls the run-out flags.  When circular wait is confirmed
 *  (both threads blocked on each other's resource), it acts
 *  as the "kernel" and terminates the non-striker process.

 */

#include "umpire.h"
#include "utils.h"

//  DEADLOCK DETECTOR 
void* deadlock_detector_thread(void* /*arg*/) {

    while (!match_over) {
        usleep(DEADLOCK_POLL_US);  // poll every 5 ms

        pthread_mutex_lock(&runout_mutex);

        // ── Circular-wait condition check 
        // Striker holds end1, wants end2 (striker_blocked).
        // Non-striker holds end2, wants end1 (nonstriker_blocked).
        if (striker_blocked && nonstriker_blocked && !runout_decided) {

            cout << ts()
                 << "\n   [UMPIRE / DEADLOCK DETECTOR] "
                    "*** CIRCULAR WAIT DETECTED ***\n"
                 << ts()
                 << "   Striker holds End-1, wants End-2.\n"
                 << ts()
                 << "   Non-Striker holds End-2, wants End-1.\n"
                 << ts() << RED
                 << "   Umpire rules: Non-Striker is RUN OUT - "
                    "Process killed by Kernel.\n\n" << RESET;

            // Set the wicket pending so umpire_thread does the substitution
            pthread_mutex_lock(&score_mutex);
            if (non_striker_id >= 0) {
                wicket_pending = true;
                out_batsman_id = non_striker_id;
            }
            pthread_mutex_unlock(&score_mutex);

            // Resolution: non-striker is out, striker survives
            nonstriker_out  = true;
            runout_decided  = true;
            striker_blocked = false;

            // Wake both blocked threads so they can read the decision
            pthread_cond_broadcast(&runout_reply_cv);
        }

        pthread_mutex_unlock(&runout_mutex);
    }
    return nullptr;
}

// ─UMPIRE THREAD 
void* umpire_thread(void* /*arg*/) {

    cout << "\n[Umpire] Match started. Batsmen walking to crease.\n\n";

    // ─ Assign opening pair and signal them 
    pthread_mutex_lock(&pitch_mutex);
    striker_id     = batting_order[0].id;
    non_striker_id = batting_order[1].id;
    pthread_cond_signal(&batsman_cv[striker_id]);
    pthread_cond_signal(&batsman_cv[non_striker_id]);
    pthread_mutex_unlock(&pitch_mutex);

    // ─ Release the bowlers (start_cv broadcast) 
    pthread_mutex_lock(&start_mutex);
    match_started = true;
    pthread_cond_broadcast(&start_cv);
    pthread_mutex_unlock(&start_mutex);

    // ─ Assign first bowler 
    pthread_mutex_lock(&pitch_mutex);
    active_bowler_id = 0;
    ball_processed   = true;
    pthread_cond_signal(&bowler_cv[0]);
    pthread_mutex_unlock(&pitch_mutex);

    // ─ Main scheduling loop 
    while (balls_bowled < TOTAL_BALLS && wickets_fallen < MAX_WICKETS) {

        // Block until batsman_thread signals stroke_done
        pthread_mutex_lock(&umpire_mutex);
        while (!stroke_done && !match_over)
            pthread_cond_wait(&umpire_cv, &umpire_mutex);
        stroke_done = false;
        pthread_mutex_unlock(&umpire_mutex);

        if (match_over) break;

        // Every delivery (legal or wide) gets a unique Gantt ID
        total_deliveries++;

        // ─ Process wicket if pending 
        pthread_mutex_lock(&score_mutex);
        bool wkt = wicket_pending;
        int  wid = out_batsman_id;
        wicket_pending = false;
        out_batsman_id = -1;
        pthread_mutex_unlock(&score_mutex);

        if (wkt) {
            wickets_fallen++;
            cout << ts()
                 << "   [UMPIRE] Wicket #" << wickets_fallen
                 << "! Batsman " << wid << " is OUT.\n";

            // Check if innings should end now
            if (wickets_fallen >= MAX_WICKETS ||
                next_batsman_index >= NUM_BATSMEN ||
                balls_bowled >= TOTAL_BALLS)
            { break; }

            // Bring next batsman to the crease
            int new_id = batting_order[next_batsman_index].id;
            next_batsman_index++;

            cout << ts()
                 << "   [UMPIRE] New batsman: Batsman " << new_id
                 << " heading to crease.\n";

            pthread_mutex_lock(&pitch_mutex);
            if      (striker_id     == wid) striker_id     = new_id;
            else if (non_striker_id == wid) non_striker_id = new_id;
            else                            striker_id     = new_id;
            pthread_cond_signal(&batsman_cv[new_id]);
            pthread_mutex_unlock(&pitch_mutex);
        }

        // ─ Update ball count 
        if (legal_delivery) {
            balls_bowled++;
        } else {
            // Wide: do not count, reset flag for next delivery
            legal_delivery = true;
            cout << ts()
                 << "   [UMPIRE] Wide not counted - ball tally stays at "
                 << balls_bowled << ".\n";
        }

        // ─ Innings 2: check if target has been reached 
        if (target_score != -1 && global_score >= target_score) {
            cout << ts() << "   [UMPIRE] TARGET REACHED! Match Over.\n";
            break;
        }

        if (balls_bowled >= TOTAL_BALLS || wickets_fallen >= MAX_WICKETS)
            break;

        // ─ End-of-over: context switch (scheduler logic) 
        bool end_of_over = (balls_bowled % 6 == 0);

        if (end_of_over) {
            int over_num    = balls_bowled / 6;
            int prev_bowler = active_bowler_id;

            cout << ts()
                 << "\n===================================================\n";
            cout << "[SCHEDULER] Over " << over_num << " complete. "
                 << "Score: " << global_score << "/" << wickets_fallen << "\n";
            cout << "[SCHEDULER] Bowler " << prev_bowler
                 << " stats this over - Runs: "
                 << bowler_runs[prev_bowler].load()
                 << ", Wickets: "
                 << bowler_wickets[prev_bowler].load() << "\n";
            cout << "[SCHEDULER] Context Switch - saving Bowler "
                 << prev_bowler << " state.\n";
            cout << "[SCHEDULER] Batsmen swap ends.\n";

            // End-of-over: batsmen swap ends (standard cricket rule)
            pthread_mutex_lock(&pitch_mutex);
            swap(striker_id, non_striker_id);
            pthread_mutex_unlock(&pitch_mutex);

            // ─ Bowler scheduling policy ─
            if (balls_bowled >= DEATH_OVER_START) {
                // PRIORITY scheduling: death specialist always bowls
                if (match_intensity == 0) {
                    match_intensity = 1;
                    cout << "[SCHEDULER] DEATH OVERS! Priority Scheduling - "
                            "Bowler 4 (Death Specialist).\n";
                }
                active_bowler_id = 4;
                cout << "[SCHEDULER] Context Switch - loading Bowler 4 "
                        "(Priority)\n";
            } else {
                // ROUND ROBIN scheduling: cycle bowlers 0-3
                active_bowler_id = (active_bowler_id + 1) % 4;
                cout << "[SCHEDULER] Context Switch (RR) - loading Bowler "
                     << active_bowler_id << "\n";
            }
            cout << "===================================================\n\n";
        }

        // ─ Signal next bowler to deliver 
        pthread_mutex_lock(&pitch_mutex);
        ball_processed = true;
        pthread_cond_signal(&bowler_cv[active_bowler_id]);
        pthread_mutex_unlock(&pitch_mutex);

    }  // end scheduler loop

    // ─ Innings over: broadcast match_over to every thread 
    match_over = true;

    // Wake all fielders so they can exit
    pthread_mutex_lock(&fielder_mutex);
    ball_in_air        = false;
    fielders_responded = 0;
    pthread_cond_broadcast(&fielders_active_cv);
    pthread_cond_broadcast(&fielders_done_cv);
    pthread_mutex_unlock(&fielder_mutex);

    // Wake all bowlers and batsmen so they can exit
    pthread_mutex_lock(&pitch_mutex);
    for (int i = 0; i < NUM_BOWLERS;  i++) pthread_cond_broadcast(&bowler_cv[i]);
    for (int i = 0; i < NUM_BATSMEN;  i++) pthread_cond_broadcast(&batsman_cv[i]);
    pthread_mutex_unlock(&pitch_mutex);

    // Wake any thread still waiting on umpire_cv
    pthread_mutex_lock(&umpire_mutex);
    stroke_done = true;
    pthread_cond_broadcast(&umpire_cv);
    pthread_mutex_unlock(&umpire_mutex);

    // Wake deadlock-related wait conditions
    pthread_mutex_lock(&runout_mutex);
    runout_decided      = true;
    nonstriker_out      = false;
    runout_episode_done = true;
    pthread_cond_broadcast(&runout_reply_cv);
    pthread_cond_broadcast(&nonstriker_run_cv);
    pthread_mutex_unlock(&runout_mutex);

    // Wake any thread still waiting on start_cv
    pthread_mutex_lock(&start_mutex);
    pthread_cond_broadcast(&start_cv);
    pthread_mutex_unlock(&start_mutex);

    return nullptr;
}
