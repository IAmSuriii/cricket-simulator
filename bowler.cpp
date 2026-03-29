/*
 * bowler.cpp

 * Implements the bowler thread.
 *
 * A bowler thread blocks until:
     (a) it is the active bowler  (active_bowler_id == id), AND
     (b) the previous delivery has been processed (ball_processed).
 
 * Then it records the exact microsecond it releases the ball into
 * delivery_start_us (read later by batsman_thread for Gantt chart),
 * sets ball_delivered, and signals the striker's condition variable.
 *
 * Context switching between bowlers is handled by umpire_thread
 * by updating active_bowler_id at the end of each over.

 */

#include "bowler.h"
#include "utils.h"

void* bowler_thread(void* arg) {
    int    id   = *((int*)arg);
    string role = (id == 4) ? "Death Over Specialist" : "Standard Bowler";

    // Wait until umpire_thread fires the start signal 
    pthread_mutex_lock(&start_mutex);
    while (!match_started)
        pthread_cond_wait(&start_cv, &start_mutex);
    pthread_mutex_unlock(&start_mutex);

    while (true) {

        // Block until this bowler is scheduled AND pitch is free 
        pthread_mutex_lock(&pitch_mutex);

        while (!match_over && (active_bowler_id != id || !ball_processed))
            pthread_cond_wait(&bowler_cv[id], &pitch_mutex);

        if (match_over) {
            pthread_mutex_unlock(&pitch_mutex);
            break;
        }

        //  Release the ball 
        cout << ts()
             << "\n[Bowler " << id << " - " << role << "] Bowls delivery "
             << balls_bowled + 1 << "...\n";

        // Stamp the exact release time so batsman_thread can record
        // an accurate start_time_us in the Gantt chart entry.
        delivery_start_us = elapsed_us();

        ball_delivered = true;   // batsman_thread key: ball is here
        ball_processed = false;  // prevent bowler from bowling again

        // Wake the striker so they can respond to the delivery
        if (striker_id >= 0 && striker_id < NUM_BATSMEN)
            pthread_cond_signal(&batsman_cv[striker_id]);

        pthread_mutex_unlock(&pitch_mutex);

        // Small sleep simulates ball-travel time before batsman reacts
        usleep(15000);

    }  // end while(true)

    return nullptr;
}
