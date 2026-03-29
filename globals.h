/*
 * globals.h
 * 
 * Central header included by EVERY translation unit.
 * Declares (extern) every shared variable, struct, constant,
 * mutex, semaphore, and condition variable so all files can
 * see the same objects without re-defining them.
 * 
 */

#pragma once

#include <iostream>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <vector>
#include <algorithm>
#include <cmath>
#include <random>
#include <iomanip>
#include <string>
#include <sstream>
#include <ctime>
#include <atomic>

using namespace std;

// ANSI colour macros 
#define RED    "\033[31m"
#define GREEN  "\033[32m"
#define YELLOW "\033[33m"
#define CYAN   "\033[36m"
#define RESET  "\033[0m"

//  CONSTANTS
static const int TOTAL_BALLS      = 120;
static const int MAX_WICKETS      = 10;
static const int NUM_BOWLERS      = 5;
static const int NUM_BATSMEN      = 11;
static const int NUM_FIELDERS     = 10;
static const int DEATH_OVER_START = 108;  // balls 109-120 = overs 19-20
static const int DEADLOCK_POLL_US = 5000;

//  STRUCTS

/* One row in the Gantt chart — recorded for every delivery */
struct GanttEntry {
    int  ball_number;
    int  bowler_id;
    int  striker_id;      // snapshot BEFORE any end-swap
    long start_time_us;   // when bowler released the ball
    long end_time_us;     // when stroke was fully resolved
    int  runs_scored;     // runs on THIS delivery (0 for wicket)
    int  score_after;     // cumulative score AFTER this delivery
    bool is_wicket;       // clean bowled / caught / LBW
    bool is_runout;       // run-out dismissal
    bool is_wide;         // wide delivery
};

/* Waiting-time record for each batsman */
struct WaitStats {
    int  id;
    long t_created_us;   // when the batsman thread was spawned
    long t_crease_us;    // when the batsman actually reached the crease
    long wait_us;        // t_crease_us - t_created_us
};

/* Batsman identity + scheduling weight */
struct Batsman {
    int id;
    int stay_duration;   // used by SJF scheduler
};

//  GANTT LOG & TIMING

extern vector<GanttEntry> gantt_log;
extern struct timespec     match_start_time;

/* Written by bowler_thread when ball is released;
   read by batsman_thread to stamp Gantt start_time_us. */
extern long delivery_start_us;

//  WAIT-TIME STATISTICS

extern WaitStats        wait_stats[NUM_BATSMEN];
extern pthread_mutex_t  stats_mutex;

//  MATCH STATE

extern int  global_score;
extern int  target_score;        // -1 until Innings 2 begins
extern int  wickets_fallen;
extern int  balls_bowled;
extern int  total_deliveries;    // every delivery (legal + wide)
extern int  match_intensity;
extern bool match_over;

extern int  active_bowler_id;
extern int  striker_id;
extern int  non_striker_id;

/* Ball-state flags — all guarded by pitch_mutex */
extern bool ball_delivered;
extern bool ball_processed;
extern bool legal_delivery;      // false → wide; do not increment balls_bowled

//  BOWLER STATISTICS  (atomic so threads can update concurrently)

extern atomic<int> bowler_runs   [NUM_BOWLERS];
extern atomic<int> bowler_wickets[NUM_BOWLERS];

//  FIELDER COORDINATION

extern pthread_mutex_t fielder_mutex;
extern pthread_cond_t  fielders_active_cv;  // signal: ball is in air
extern pthread_cond_t  fielders_done_cv;    // signal: all fielders responded

extern bool   ball_in_air;
extern bool   fielder_caught;
extern double ball_r;
extern double ball_theta;
extern double ball_flight_time;
extern int    fielders_responded;

//  WICKET / RUN-OUT STATE

extern bool wicket_pending;
extern int  out_batsman_id;

/* Run-out / deadlock protocol */
extern pthread_mutex_t runout_mutex;
extern pthread_cond_t  runout_cv;

extern bool striker_wants_end2;
extern bool nonstriker_wants_end1;
extern bool striker_blocked;
extern bool nonstriker_blocked;

extern pthread_cond_t  runout_reply_cv;
extern bool            runout_decided;
extern bool            nonstriker_out;

/* Signal non-striker to begin running */
extern pthread_cond_t  nonstriker_run_cv;
extern bool            nonstriker_run_flag;
extern bool            runout_episode_done;


//  SYNCHRONIZATION PRIMITIVES

extern pthread_mutex_t pitch_mutex;
extern pthread_mutex_t score_mutex;

extern pthread_mutex_t umpire_mutex;
extern pthread_cond_t  umpire_cv;
extern bool            stroke_done;

extern pthread_cond_t  bowler_cv [NUM_BOWLERS];
extern pthread_cond_t  batsman_cv[NUM_BATSMEN];

extern sem_t           crease_sem;   // 2 slots → exactly 2 batsmen on pitch

extern pthread_mutex_t end1_mutex;   // represents the striker's end
extern pthread_mutex_t end2_mutex;   // represents the non-striker's end

extern pthread_mutex_t start_mutex;
extern pthread_cond_t  start_cv;
extern bool            match_started;

//  BATTING ORDER

extern vector<Batsman> batting_order;
extern int             next_batsman_index;
extern bool            sjf_mode;
