/*
 * batsman.h

 * Declares the POSIX thread entry-point for batsman threads.
 *
 * Responsibilities (as striker):
 *   Wait at the crease for a delivery (ball_delivered flag).
 *   Randomly choose an outcome: 0/1/2/3/4/6 runs, wicket,
 *   wide, or a called run (run-out risk).
 *   Update global_score, log a GanttEntry, signal umpire.
 *
 * Responsibilities (as non-striker):
 *   Respond to run calls from striker.
 *   Handle the run-out / deadlock scenario using end1/end2 mutexes.
 */

#pragma once
#include "globals.h"

/*
  Entry point for each batsman thread.
  arg → pointer to a Batsman struct in batting_order[].
 */
void* batsman_thread(void* arg);
