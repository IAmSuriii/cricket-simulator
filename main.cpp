/*
 * main.cpp

  Entry point for the T20 Cricket Simulator.
 
 * Responsibilities:
    1. Parse --sjf flag for scheduling mode.
    2. Initialise shared primitives (semaphore, cond vars).
    3. Run the two-innings loop:
        a. Build batting_order (FCFS or SJF sorted).
        b. Set Innings-2 target if applicable.
        c. Spawn all threads (fielders, bowlers, batsmen,
           umpire, deadlock detector).
        d. Join all threads.
        e. Print Gantt / bowler summary / wait-time reports.
        f. Call reset_innings() before Innings 2.
   4. Print the final match result.
   5. Destroy shared primitives.
  
   */

#include "globals.h"
#include "utils.h"
#include "fielder.h"
#include "bowler.h"
#include "batsman.h"
#include "umpire.h"
#include "reports.h"

int main(int argc, char* argv[]) {

    // 1. Parse command-line flags 
    sjf_mode = (argc > 1 && string(argv[1]) == "--sjf");

    cout << "=================================================\n";
    cout << "  T20 Cricket Simulator - CSC-204 Assignment 1\n";
    cout << "  Scheduling mode: " << (sjf_mode ? "SJF" : "FCFS") << "\n";
    cout << "=================================================\n\n";

    //  2. Initialise shared primitives 
    // crease_sem(2): at most 2 batsmen may occupy the crease
    sem_init(&crease_sem, 0, 2);

    // Condition variables must be explicitly initialised (not in globals.cpp
    // because PTHREAD_COND_INITIALIZER only works for static objects)
    for (int i = 0; i < NUM_BOWLERS;  i++) pthread_cond_init(&bowler_cv[i],  nullptr);
    for (int i = 0; i < NUM_BATSMEN;  i++) pthread_cond_init(&batsman_cv[i], nullptr);

    mt19937 master_gen(static_cast<unsigned>(time(nullptr)));
    uniform_int_distribution<> variance_dist(-15, 15);

    int innings1_score = 0;

    //  3. Innings loop 
    for (int innings = 1; innings <= 2; innings++) {

        cout << "\n"
             << "##################################################\n"
             << "##         INNINGS " << innings
             << " BEGINNING                  ##\n"
             << "##################################################\n\n";

        //  3a. Build batting order 
        batting_order.clear();

        // Base stay-durations (higher index = weaker batsman)
        int base_stay[NUM_BATSMEN] = {100,90,80,70,60,50,40,30,25,20,15};

        for (int i = 0; i < NUM_BATSMEN; i++) {
            batting_order.push_back({i, base_stay[i]});
        }

        if (sjf_mode) {
            // SJF: shortest job (weakest batsman) bats first
            sort(batting_order.begin(), batting_order.end(),
                 [](const Batsman& a, const Batsman& b) {
                     return a.stay_duration < b.stay_duration;
                 });
            cout << "[SCHEDULER] SJF mode - batting order sorted by "
                    "stay_duration (ascending).\n";
        } else {
            cout << "[SCHEDULER] FCFS mode - batting order is positional.\n";
        }

        cout << "[SCHEDULER] Batting order for Innings " << innings << ":\n";
        for (int i = 0; i < NUM_BATSMEN; i++) {
            cout << "   Position " << setw(2) << i
                 << " -> Batsman " << batting_order[i].id
                 << "  (stay_duration=" << batting_order[i].stay_duration << ")\n";
        }
        cout << "\n";

        //  3b. Set target for Innings 2 
        if (innings == 2) {
            target_score = innings1_score + 1;
            cout << "[UMPIRE] Team 2 needs " << target_score
                 << " runs to win.\n\n";
        }

        // Mark the wall-clock start of this innings
        clock_gettime(CLOCK_MONOTONIC, &match_start_time);

        //   3c. Spawn threads 

        // Fielder threads (10 threads, one per fielder position)
        pthread_t fielder_threads[NUM_FIELDERS];
        int       fielder_ids[NUM_FIELDERS];
        for (int i = 0; i < NUM_FIELDERS; i++) {
            fielder_ids[i] = i;
            pthread_create(&fielder_threads[i], nullptr,
                           fielder_thread, &fielder_ids[i]);
        }

        // Bowler threads (5 threads: 4 standard + 1 death specialist)
        pthread_t bowler_threads[NUM_BOWLERS];
        int       bowler_ids[NUM_BOWLERS];
        for (int i = 0; i < NUM_BOWLERS; i++) {
            bowler_ids[i] = i;
            pthread_create(&bowler_threads[i], nullptr,
                           bowler_thread, &bowler_ids[i]);
        }

        // Batsman threads (11 threads, one per batting position)
        pthread_t batsman_threads[NUM_BATSMEN];
        for (int i = 0; i < NUM_BATSMEN; i++) {
            pthread_create(&batsman_threads[i], nullptr,
                           batsman_thread, &batting_order[i]);
        }

        // Umpire thread (scheduler) and deadlock detector
        pthread_t umpire_tid, dd_tid;
        pthread_create(&umpire_tid, nullptr, umpire_thread,            nullptr);
        pthread_create(&dd_tid,     nullptr, deadlock_detector_thread, nullptr);

        //  3d. Join threads (wait for innings to end)
        pthread_join(umpire_tid, nullptr);
        pthread_join(dd_tid,     nullptr);

        for (int i = 0; i < NUM_BOWLERS;  i++) pthread_join(bowler_threads[i],  nullptr);
        for (int i = 0; i < NUM_BATSMEN;  i++) pthread_join(batsman_threads[i], nullptr);
        for (int i = 0; i < NUM_FIELDERS; i++) pthread_join(fielder_threads[i], nullptr);

        //  3e. Print innings summary reports 
        cout << "\n##################################################\n";
        cout << "  INNINGS " << innings << " FINAL SCORE: "
             << global_score << "/" << wickets_fallen << "\n";
        cout << "##################################################\n";

        print_gantt_chart();
        print_bowler_summary();
        print_wait_analysis(innings);

        if (innings == 1) {
            innings1_score = global_score;  // save for Innings 2 target
        }

        // 3f. Reset state before Innings 2 
        if (innings < 2) {
            reset_innings();
        }

    }  // end innings loop

    //  4. Final match result 
    cout << "\n##################################################\n";
    cout << "  MATCH RESULT\n";
    cout << "##################################################\n";
    if (target_score != -1) {
        if (global_score >= target_score)
            cout << "  Team 2 WINS by "
                 << (10 - wickets_fallen) << " wicket(s)!\n";
        else
            cout << "  Team 1 WINS by "
                 << (target_score - 1 - global_score) << " run(s)!\n";
    }
    cout << "  Innings 1 Score : " << innings1_score << "\n";
    cout << "  Innings 2 Score : " << global_score
         << "/" << wickets_fallen << "\n";
    cout << "##################################################\n\n";

    //  5. Destroy shared primitives
    sem_destroy(&crease_sem);
    for (int i = 0; i < NUM_BOWLERS;  i++) pthread_cond_destroy(&bowler_cv[i]);
    for (int i = 0; i < NUM_BATSMEN;  i++) pthread_cond_destroy(&batsman_cv[i]);

    return 0;
}
