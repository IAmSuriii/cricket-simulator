/*
 * bowler.h
 * ------------
 * Declares the POSIX thread entry-point for bowler threads.
 *
 * Responsibilities:
 *  - Wait until the umpire assigns this bowler (active_bowler_id).
 *  - Record delivery_start_us (wall-clock when ball is released).
 *  - Set ball_delivered and signal the striker's batsman_thread.
 * ------------
 */

#pragma once
#include "globals.h"

/*
 * Entry point for each bowler thread.
 * arg -> pointer to the bowler's integer ID (0 … NUM_BOWLERS-1).
 * Bowler 4 is the Death Over Specialist (overs 19-20).
 */
void* bowler_thread(void* arg);
