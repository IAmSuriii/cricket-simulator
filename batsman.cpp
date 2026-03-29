/*
 * batsman.cpp

 * Implements the batsman thread — the most complex thread in
 * the simulator.
 
  LIFECYCLE:
 *  1. Record creation time for wait-time analysis.
 *  2. Block until umpire_thread assigns this batsman as
 *     striker or non-striker.
 *  3. Acquire crease_sem (capacity 2 → only 2 batsmen on pitch).
 *  4. Enter the delivery loop:
 *       a. Non-striker run path    — respond to run-calls
 *       b. Striker calls a run     — 2% chance per delivery
 *       c. Wide ball               — 5% chance per delivery
 *       d. Normal stroke           — 0/1/2/3/4/6 or WICKET
 *  5. On dismissal: release crease_sem and return.
 *
 * RUN-OUT PROTOCOL (deadlock simulation):
 *  Striker locks end1_mutex then tries end2_mutex.
 *  Non-striker locks end2_mutex then tries end1_mutex.
 *  Circular wait → deadlock_detector_thread resolves it by
 *  declaring non-striker run-out (OS process kill analogy).
 */

#include "batsman.h"
#include "utils.h"

void* batsman_thread(void* arg) {
    Batsman* me = (Batsman*)arg;
    int      id = me->id;

    // 1. Record creation timestamp 
    long t_created = elapsed_us();
    pthread_mutex_lock(&stats_mutex);
    wait_stats[id].id           = id;
    wait_stats[id].t_created_us = t_created;
    wait_stats[id].t_crease_us  = 0;
    wait_stats[id].wait_us      = 0;
    pthread_mutex_unlock(&stats_mutex);

    // Per-batsman RNG setup
    mt19937 gen(static_cast<unsigned>(time(nullptr)) + id * 1000 + 7);

    // Higher stay_duration → lower wicket weight (better batsman)
    int wicket_weight = 2 + (110 - me->stay_duration) / 10;

    // Outcome distribution: 0,1,2,3,4,6 runs or wicket
    discrete_distribution<> t20_dist({30, 30, 15, 5, 10, 8,
                                       (double)wicket_weight});

    // For fielder simulation: random ball trajectory
    uniform_real_distribution<> dis_radius(0.1, 0.95);
    uniform_real_distribution<> dis_angle (0.0, 2.0 * M_PI);
    uniform_real_distribution<> dis_flight(0.8, 1.8);

    // 2. Wait until umpire assigns us a role 
        pthread_mutex_lock(&pitch_mutex);
    while (striker_id != id && non_striker_id != id && !match_over)
        pthread_cond_wait(&batsman_cv[id], &pitch_mutex);
    pthread_mutex_unlock(&pitch_mutex);

    if (match_over) return nullptr;

    //  3. Acquire crease slot (semaphore, capacity = 2) 
    sem_wait(&crease_sem);

    long t_crease = elapsed_us();
    pthread_mutex_lock(&stats_mutex);
    wait_stats[id].t_crease_us = t_crease;
    wait_stats[id].wait_us     = t_crease - t_created;
    pthread_mutex_unlock(&stats_mutex);

    cout << ts() << "[Batsman " << id << "] Walking to crease."
         << " (waited " << (t_crease - t_created) / 1000 << " ms)\n";

    bool is_out     = false;
    bool sem_posted = false;  // guard against double sem_post

    // 4. Delivery loop 
    while (!match_over && !is_out) {

        // Block until a ball arrives (striker) or run-call (non-striker)
        pthread_mutex_lock(&pitch_mutex);

        while (!match_over) {
            // If we've been removed from pitch (substituted out)
            if (striker_id != id && non_striker_id != id) {
                is_out = true;
                break;
            }
            // Striker: a new ball has been delivered
            if (ball_delivered && striker_id == id) break;
            // Non-striker: striker called a run
            if (nonstriker_run_flag && non_striker_id == id) break;
            pthread_cond_wait(&batsman_cv[id], &pitch_mutex);
        }

        if (match_over || is_out) {
            pthread_mutex_unlock(&pitch_mutex);
            break;
        }

        // Determine which path to take this iteration
        bool i_should_run = (nonstriker_run_flag && non_striker_id == id
                             && !ball_delivered);

        // Clear ball_delivered flag only if we are the striker
        if (!i_should_run) ball_delivered = false;

        // Snapshot IDs for Gantt chart BEFORE any possible swap
        int gantt_striker_snapshot = striker_id;
        int gantt_bowler_snapshot  = active_bowler_id;

        pthread_mutex_unlock(&pitch_mutex);

        // --------------------
        //  PATH A: NON-STRIKER RUN PATH
        //  Non-striker tries to cross to End 1 after being called.
        //  Simulates resource acquisition that can cause deadlock.
        // ---------------------
        if (i_should_run) {
            pthread_mutex_lock(&runout_mutex);
            nonstriker_run_flag = false;  // clear the flag we were woken on
            pthread_mutex_unlock(&runout_mutex);

            cout << ts()
                 << "   [Batsman " << id
                 << " (Non-Striker)] Responds to run call - locking End 2.\n";

            // Non-striker first locks End 2 (their current end)
            pthread_mutex_lock(&end2_mutex);
            usleep(300);  // simulate time to start running

            // Then tries to lock End 1 (destination)
            bool got_end1 = (pthread_mutex_trylock(&end1_mutex) == 0);

            if (!got_end1) {
                //  DEADLOCK DETECTED 
                // Striker holds end1, non-striker holds end2 -> circular wait
                cout << ts()
                     << "   [Batsman " << id
                     << " (Non-Striker)] Blocked at End 1! Deadlock!\n";

                pthread_mutex_lock(&runout_mutex);
                nonstriker_wants_end1 = true;
                nonstriker_blocked    = true;
                pthread_cond_signal(&runout_cv);  // alert deadlock detector

                // Wait for deadlock_detector_thread to resolve
                while (!runout_decided && !match_over)
                    pthread_cond_wait(&runout_reply_cv, &runout_mutex);

                bool i_am_out = nonstriker_out;

                runout_episode_done = true;
                pthread_cond_broadcast(&runout_reply_cv);

                pthread_mutex_unlock(&runout_mutex);
                pthread_mutex_unlock(&end2_mutex);

                if (i_am_out) {
                    is_out = true;
                    if (!sem_posted) { sem_post(&crease_sem); sem_posted = true; }
                    cout << ts() << RED << "[Batsman " << id
                         << "] Leaves the crease (Run Out)." << RESET << endl;

                    // Gantt entry: run-out dismissal
                    gantt_log.push_back({total_deliveries,
                                         gantt_bowler_snapshot,
                                         gantt_striker_snapshot,
                                         delivery_start_us,
                                         elapsed_us(),
                                         0,      // 0 runs on a run-out
                                         global_score,
                                         false,  // not a standard wicket
                                         true,   // is_runout → RED
                                         false});
                    signal_stroke_done();
                    return nullptr;
                }

            } else {
                // Non-striker crossed safely
                cout << ts()
                     << "   [Batsman " << id
                     << " (Non-Striker)] Crossed safely.\n";

                pthread_mutex_lock(&runout_mutex);
                nonstriker_wants_end1 = false;
                runout_episode_done   = true;
                pthread_cond_broadcast(&runout_reply_cv);
                pthread_mutex_unlock(&runout_mutex);

                pthread_mutex_unlock(&end1_mutex);
                pthread_mutex_unlock(&end2_mutex);
            }

            continue;  // back to delivery wait loop
        }

        // =-----------
        //  PATH B: STRIKER CALLS A RUN  (2% probability)
        //  Simulates OS process requesting a second resource while
        //  holding the first — the classic deadlock scenario.
        // ------------
        if (gen() % 100 < 2) {
            cout << ts()
                 << "   *** [Batsman " << id
                 << " (Striker)] Calls a run! ***\n";

            // Wait for any previous run-out episode to fully resolve
            pthread_mutex_lock(&runout_mutex);
            while (!runout_episode_done && !match_over)
                pthread_cond_wait(&runout_reply_cv, &runout_mutex);

            if (match_over) { pthread_mutex_unlock(&runout_mutex); break; }

            reset_runout_state();          // clear flags for new episode
            nonstriker_run_flag = true;    // wake non-striker
            striker_wants_end2  = true;
            pthread_mutex_unlock(&runout_mutex);

            // Striker locks End 1 first (resource hold-and-wait)
            pthread_mutex_lock(&end1_mutex);
            usleep(300);

            // Signal non-striker to start running
            pthread_mutex_lock(&pitch_mutex);
            if (non_striker_id >= 0)
                pthread_cond_signal(&batsman_cv[non_striker_id]);
            pthread_mutex_unlock(&pitch_mutex);

            usleep(500);  // let non-striker get a head-start

            // Try to lock End 2 without blocking (trylock = non-blocking)
            bool got_end2 = (pthread_mutex_trylock(&end2_mutex) == 0);

            if (!got_end2) {
                //  CIRCULAR WAIT 
                // Striker holds end1, wants end2.
                // Non-striker holds end2, wants end1.
                cout << ts()
                     << "   [Batsman " << id
                     << " (Striker)] Blocked at End 2! Circular Wait!\n";

                pthread_mutex_lock(&runout_mutex);
                striker_blocked = true;
                pthread_cond_signal(&runout_cv);  // notify detector

                while (!runout_decided && !match_over)
                    pthread_cond_wait(&runout_reply_cv, &runout_mutex);

                // Detector resolved: striker survives, non-striker is out
                striker_blocked    = false;
                striker_wants_end2 = false;
                pthread_mutex_unlock(&runout_mutex);

                // Still award 1 run to striker
                pthread_mutex_lock(&score_mutex);
                global_score++;
                if (active_bowler_id >= 0) bowler_runs[active_bowler_id]++;
                pthread_mutex_unlock(&score_mutex);

                pthread_mutex_lock(&pitch_mutex);
                swap(striker_id, non_striker_id);  // ends swapped
                ball_processed = true;
                pthread_mutex_unlock(&pitch_mutex);

            } else {
                // Run completed without deadlock
                cout << ts()
                     << "   [Batsman " << id
                     << " (Striker)] Run completed safely.\n";

                pthread_mutex_lock(&score_mutex);
                global_score++;
                if (active_bowler_id >= 0) bowler_runs[active_bowler_id]++;
                pthread_mutex_unlock(&score_mutex);

                pthread_mutex_lock(&pitch_mutex);
                swap(striker_id, non_striker_id);
                ball_processed = true;
                pthread_mutex_unlock(&pitch_mutex);

                pthread_mutex_lock(&runout_mutex);
                striker_wants_end2 = false;
                pthread_mutex_unlock(&runout_mutex);

                pthread_mutex_unlock(&end2_mutex);
            }

            pthread_mutex_unlock(&end1_mutex);

            // Wait for the entire run-out episode to fully close
            pthread_mutex_lock(&runout_mutex);
            while (!runout_episode_done && !match_over)
                pthread_cond_wait(&runout_reply_cv, &runout_mutex);
            pthread_mutex_unlock(&runout_mutex);

            signal_stroke_done();

            // Gantt entry: called run (always 1 run)
            gantt_log.push_back({total_deliveries,
                                  gantt_bowler_snapshot,
                                  gantt_striker_snapshot,
                                  delivery_start_us,
                                  elapsed_us(),
                                  1,      // 1 run scored
                                  global_score,
                                  false, false, false});
            continue;
        }

        // -------------
        //  PATH C: WIDE BALL  (5% probability per delivery attempt)
        //  Wide does NOT count as a legal delivery; balls_bowled
        //  must NOT increment (umpire_thread checks legal_delivery).
        // -------------
        if (gen() % 100 < 5) {
            bool stolen_single = (gen() % 100 < 15);  // 15% of wides
            int  wide_runs     = stolen_single ? 2 : 1;

            pthread_mutex_lock(&score_mutex);
            global_score += wide_runs;
            if (active_bowler_id >= 0)
                bowler_runs[active_bowler_id] += wide_runs;

            cout << ts() << YELLOW
                 << "   [WIDE] Bowler " << active_bowler_id
                 << " bowls a WIDE! +" << wide_runs << " run(s) awarded."
                 << RESET
                 << (stolen_single ? " (Batsmen steal a single!)" : "")
                 << " Score: " << global_score << "/" << wickets_fallen << "\n";
            pthread_mutex_unlock(&score_mutex);

            // If batsmen ran, swap ends
            if (stolen_single) {
                pthread_mutex_lock(&pitch_mutex);
                swap(striker_id, non_striker_id);
                pthread_mutex_unlock(&pitch_mutex);
            }

            // Mark wide so umpire does NOT increment balls_bowled
            pthread_mutex_lock(&pitch_mutex);
            legal_delivery = false;
            ball_processed = true;
            pthread_mutex_unlock(&pitch_mutex);

            // Gantt entry: wide → YELLOW
            gantt_log.push_back({total_deliveries,
                                  gantt_bowler_snapshot,
                                  gantt_striker_snapshot,
                                  delivery_start_us,
                                  elapsed_us(),
                                  wide_runs,
                                  global_score,
                                  false, false,
                                  true});  // is_wide → YELLOW
            signal_stroke_done();
            continue;
        }

        // -----------------
        //  PATH D: NORMAL STROKE
        //  Outcome sampled from t20_dist.
        //  Odd runs → striker and non-striker swap ends.
        //  Wicket   → set wicket_pending, umpire handles substitution.
        // -----------------
        {
            int  outcome   = t20_dist(gen);
            int  runs      = 0;
            bool is_wicket = false;

            switch (outcome) {
                case 0: runs = 0; break;
                case 1: runs = 1; break;
                case 2: runs = 2; break;
                case 3: runs = 3; break;
                case 4: runs = 4; break;
                case 5: runs = 6; break;
                case 6: is_wicket = true; break;
            }

            pthread_mutex_lock(&score_mutex);
            if (is_wicket) {
                static const char* w_types[] = {
                    "Clean Bowled!", "Caught out!", "LBW!"
                };
                cout << ts() << RED
                     << "   [Batsman " << id << "] "
                     << w_types[gen() % 3] << " OUT!" << RESET
                     << " Score: " << global_score
                     << "/" << wickets_fallen + 1 << "\n";

                is_out         = true;
                wicket_pending = true;   // umpire_thread will handle
                out_batsman_id = id;
                bowler_wickets[active_bowler_id]++;

            } else {
                global_score += runs;
                bowler_runs[active_bowler_id] += runs;
                cout << ts()
                     << "   [Batsman " << id << "] Hits for " << runs
                     << ". Total Score: " << global_score << "\n";
            }
            pthread_mutex_unlock(&score_mutex);

            // Swap ends on odd runs
            if (!is_wicket && runs % 2 != 0) {
                pthread_mutex_lock(&pitch_mutex);
                swap(striker_id, non_striker_id);
                pthread_mutex_unlock(&pitch_mutex);
            }

            // Trigger fielder threads to simulate ball trajectory
            if (!is_wicket) {
                pthread_mutex_lock(&fielder_mutex);
                ball_r             = dis_radius(gen);
                ball_theta         = dis_angle(gen);
                ball_flight_time   = dis_flight(gen);
                fielder_caught     = false;
                fielders_responded = 0;
                ball_in_air        = true;
                pthread_cond_broadcast(&fielders_active_cv);  // wake all fielders

                // Wait until every fielder has responded
                while (fielders_responded < NUM_FIELDERS && !match_over)
                    pthread_cond_wait(&fielders_done_cv, &fielder_mutex);

                pthread_mutex_unlock(&fielder_mutex);
            }

            // Gantt entry: wicket → RED, normal → no colour
            gantt_log.push_back({total_deliveries,
                                  gantt_bowler_snapshot,
                                  gantt_striker_snapshot,
                                  delivery_start_us,
                                  elapsed_us(),
                                  is_wicket ? 0 : runs,
                                  global_score,
                                  is_wicket,   // is_wicket → RED
                                  false,
                                  false});
        }

        // Allow bowler to bowl next ball
        pthread_mutex_lock(&pitch_mutex);
        ball_processed = true;
        pthread_mutex_unlock(&pitch_mutex);

        signal_stroke_done();  // notify umpire

    }  // end delivery loop

    // 5. Release crease slot
    if (!sem_posted) {
        sem_post(&crease_sem);
        sem_posted = true;
    }
    cout << ts() << RED << "[Batsman " << id << "] Leaves the crease.\n" << RESET;
    return nullptr;
}
