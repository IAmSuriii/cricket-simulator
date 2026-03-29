/*
 * fielder.cpp
 * ---------------
 * Implements the fielder thread.
 *
 * Each fielder is positioned at a fixed polar coordinate on the
 * field.  When the batsman hits the ball (ball_in_air = true),
 * every fielder wakes up, computes whether it can intercept the
 * ball, and the first one that can sets fielder_caught = true.
 * The last fielder to finish broadcasts fielders_done_cv so
 * batsman_thread can continue.
 * ---------------
 */

#include "fielder.h"
#include "utils.h"

void* fielder_thread(void* arg) {
    int id = *((int*)arg);

    // Fixed polar position: radius spreads between 0.4 and 0.9,
    // angle is evenly distributed around the boundary.
    double my_r     = 0.4 + 0.5 * ((id % 5) / 4.0);
    double my_theta = (2.0 * M_PI * id) / NUM_FIELDERS;
    double my_speed = 0.7;  // normalised field speed

    while (true) {

        //  Wait until a ball is in the air 
        pthread_mutex_lock(&fielder_mutex);

        while (!ball_in_air && !match_over)
            pthread_cond_wait(&fielders_active_cv, &fielder_mutex);

        // Track how many fielders have responded to this ball
        fielders_responded++;
        bool all_done = (fielders_responded >= NUM_FIELDERS);

        // If match ended while waiting, clean up and exit
        if (match_over) {
            if (all_done)
                pthread_cond_broadcast(&fielders_done_cv);
            pthread_mutex_unlock(&fielder_mutex);
            break;
        }

        // Snapshot shared ball data while holding the mutex
        double br  = ball_r;
        double bth = ball_theta;
        double bft = ball_flight_time;

        // ── Compute distance from my position to ball landing ─────
        // Law of cosines: d² = r1² + r2² - 2·r1·r2·cos(Δθ)
        double dist = sqrt(my_r * my_r + br * br
                           - 2.0 * my_r * br * cos(my_theta - bth));

        // If I can reach the ball before it lands AND no one caught it
        bool i_caught = false;
        if (!fielder_caught && (dist / my_speed) <= bft) {
            fielder_caught = true;   // claim the catch (first-come)
            i_caught       = true;
        }

        if (i_caught) {
            cout << ts()
                 << "   [Fielder " << id << "] Intercepted the ball!"
                 << " (dist=" << fixed << setprecision(2) << dist << ")\n";
        }

        //  Last fielder to respond resets ball_in_air and signals
        if (all_done) {
            if (!fielder_caught)
                cout << ts() << "   [Fielders] No fielder intercepted the ball.\n";
            ball_in_air = false;
            pthread_cond_broadcast(&fielders_done_cv);  // wake batsman_thread
        }

        pthread_mutex_unlock(&fielder_mutex);

    }  // end while(true)

    return nullptr;
}
