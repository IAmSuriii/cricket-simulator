/*
 * umpire.h
 
 * Declares the two "control plane" threads:
 *
 *  umpire_thread            — acts as the OS kernel scheduler.
 *     Assigns opening batsmen and first bowler.
 *     Waits for stroke_done after every delivery.
 *     Increments ball count, handles wickets, swaps ends.
 *     Performs Round-Robin scheduling of bowlers (overs 1-18)
 *     and Priority scheduling of Bowler 4 (death overs 19-20).
 *     Detects innings-end conditions and broadcasts match_over.
 *
 *  deadlock_detector_thread — acts as the OS deadlock monitor.
 *     Polls every DEADLOCK_POLL_US microseconds.
 *     If both striker_blocked AND nonstriker_blocked are set,
 *     declares circular wait and kills the non-striker (run-out).

 */

#pragma once
#include "globals.h"

/* Entry point for the umpire / kernel-scheduler thread. */
void* umpire_thread(void* arg);

/* Entry point for the deadlock-detector thread. */
void* deadlock_detector_thread(void* arg);
