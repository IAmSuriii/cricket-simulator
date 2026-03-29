/*
 * utils.h

 * Declarations for small helper functions shared across
 * multiple thread implementation files:
 *
 *   elapsed_us()         — microseconds since innings start
 *   ts()                 — "[T+NNNms] " timestamp prefix
 *   signal_stroke_done() — wakes the umpire after a delivery
 *   reset_runout_state() — clears all run-out protocol flags
 *   reset_innings()      — full state reset between innings

 */

#pragma once
#include "globals.h"

/* Returns microseconds elapsed since match_start_time was set. */
long elapsed_us();

/* Returns a human-readable "[T+XXXXms] " string for console logs. */
string ts();

/*
 * Signals the umpire that the current delivery has been fully
 * processed by the batting side.  Must be called by the
 * batsman_thread at the end of every delivery path.
 */
void signal_stroke_done();

/*
 * Resets every flag in the run-out / deadlock protocol so the
 * next run-attempt starts from a clean state.
 */
void reset_runout_state();

/*
 * Called between innings.  Zeros all match counters, clears logs,
 * and re-initialises the crease semaphore.
 */
void reset_innings();
