/*
 * reports.cpp
 * =========================================================
 * Implements the three post-innings report printers.
 * All data comes from the shared globals (gantt_log,
 * bowler_runs, bowler_wickets, wait_stats, batting_order).
 * =========================================================
 */

#include "reports.h"

// ── GANTT CHART ───────────────────────────────────────────────
/*
 * Iterates gantt_log and prints each delivery as one row.
 * Colours:
 *   RED    → is_wicket or is_runout
 *   YELLOW → is_wide
 *   RESET  → normal delivery
 */
void print_gantt_chart() {
    cout << "\n===================================================\n";
    cout << "  GANTT CHART - Ball-by-Ball Thread Usage\n";
    cout << "===================================================\n";

    // Table header
    cout << left
         << setw(6)  << "Ball"
         << setw(9)  << "Bowler"
         << setw(9)  << "Striker"
         << setw(13) << "Start(ms)"
         << setw(13) << "End(ms)"
         << setw(7)  << "Runs"
         << setw(8)  << "Score"
         << setw(10) << "Event"
         << "\n";
    cout << string(75, '-') << "\n";

    for (auto& e : gantt_log) {
        const char* colour = RESET;
        const char* event  = "Normal";

        if (e.is_runout) {
            colour = RED;    event = "RUN OUT";
        } else if (e.is_wicket) {
            colour = RED;    event = "WICKET";
        } else if (e.is_wide) {
            colour = YELLOW; event = "WIDE";
        }

        cout << colour
             << left
             << setw(6)  << e.ball_number
             << setw(9)  << ("B"   + to_string(e.bowler_id))
             << setw(9)  << ("Bat" + to_string(e.striker_id))
             << setw(13) << (e.start_time_us / 1000)  // convert µs → ms
             << setw(13) << (e.end_time_us   / 1000)
             << setw(7)  << e.runs_scored
             << setw(8)  << e.score_after
             << setw(10) << event
             << RESET << "\n";
    }
    cout << string(75, '-') << "\n";
}

// ── BOWLER SUMMARY ────────────────────────────────────────────
/*
 * Reads the atomic<int> arrays bowler_runs[] and
 * bowler_wickets[] and prints one row per bowler plus
 * a grand total.  Validates that total runs == global_score.
 */
void print_bowler_summary() {
    cout << "\n===================================================\n";
    cout << "  BOWLER SUMMARY\n";
    cout << "===================================================\n";
    cout << left
         << setw(10) << "Bowler"
         << setw(12) << "Runs Given"
         << setw(12) << "Wickets"
         << "\n";
    cout << string(34, '-') << "\n";

    int total_bowler_runs = 0;
    for (int i = 0; i < NUM_BOWLERS; i++) {
        int r = bowler_runs[i].load();
        int w = bowler_wickets[i].load();
        total_bowler_runs += r;
        cout << left
             << setw(10) << ("Bowler " + to_string(i))
             << setw(12) << r
             << setw(12) << w
             << "\n";
    }

    cout << string(34, '-') << "\n";
    cout << left
         << setw(10) << "TOTAL"
         << setw(12) << total_bowler_runs
         << "\n";

    // Integrity check: sum of attributed runs must match final score
    if (total_bowler_runs == global_score)
        cout << "  [OK] Bowler run totals perfectly match final score.\n";
    else
        cout << "  [WARN] Mismatch! Bowler total=" << total_bowler_runs
             << " vs score=" << global_score << "\n";
}

void print_wait_analysis(int innings) {
    cout << "\n===================================================\n";
    cout << "  WAIT-TIME ANALYSIS - Innings " << innings << "\n";
    cout << "===================================================\n";
    cout << left
         << setw(10) << "Batsman"
         << setw(18) << "Wait(ms)"
         << setw(15) << "StayDuration"
         << "\n";
    cout << string(43, '-') << "\n";

    long total_wait_time = 0;
    int total_batsmen = NUM_BATSMEN;
    long middle_order_wait_time = 0;
    int middle_order_count = 0;

    for (int i = 0; i < NUM_BATSMEN; i++) {
        long wait_time = wait_stats[i].wait_us / 1000;  // Convert to ms
        cout << left
             << setw(10) << wait_stats[i].id
             << setw(18) << wait_time
             << setw(15) << batting_order[i].stay_duration
             << "\n";

        // Aggregate total wait time for all batsmen
        total_wait_time += wait_time;

        // Check if the batsman is in the middle order (4 to 7)
        if (i >= 4 && i <= 7) {  // Positions 4 to 7 
            middle_order_wait_time += wait_time;
            middle_order_count++;
        }
    }

    // Calculate and print average wait time for all batsmen
    double average_wait_time = static_cast<double>(total_wait_time) / total_batsmen;
    cout << "\nAverage wait time for all batsmen: "
         << average_wait_time << " ms\n";

    // Calculate and print average wait time for middle-order batsmen
    if (middle_order_count > 0) {
        double average_middle_order_wait_time = static_cast<double>(middle_order_wait_time) / middle_order_count;
        cout << "Average wait time for middle-order batsmen (4-7): "
             << average_middle_order_wait_time << " ms\n";
    } else {
        cout << "No middle-order batsmen were present.\n";
    }
}
