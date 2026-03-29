/*
 * fielder.h
 * --------------
 * Declares the POSIX thread entry-point for fielder threads.
 *
 * Responsibilities:
 *    Wait (blocked) until ball_in_air is set by batsman_thread.
 *    Compute Euclidean distance from personal position to ball.
 *    If reachable before ball_flight_time, mark fielder_caught.
 *    Last fielder to respond broadcasts fielders_done_cv so
 *    batsman_thread can unblock.
 */

#pragma once
#include "globals.h"

/*
 * Entry point for each fielder thread.
 * arg -> pointer to the fielder's integer ID (0 … NUM_FIELDERS-1).
 */
void* fielder_thread(void* arg);
