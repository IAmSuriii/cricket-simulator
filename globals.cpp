/*
 * globals.cpp
 * ------------
 * Provides the ONE definition of every extern declared in
 * globals.h.  All other .cpp files include globals.h and
 * link against this translation unit.
 * ------------
 */

#include "globals.h"

//  Gantt & timing 
vector<GanttEntry> gantt_log;
struct timespec     match_start_time;
long                delivery_start_us = 0;

//  Wait stats 
WaitStats       wait_stats[NUM_BATSMEN];
pthread_mutex_t stats_mutex = PTHREAD_MUTEX_INITIALIZER;

//  Match state 
int  global_score     = 0;
int  target_score     = -1;
int  wickets_fallen   = 0;
int  balls_bowled     = 0;
int  total_deliveries = 0;
int  match_intensity  = 0;
bool match_over       = false;

int  active_bowler_id = -1;
int  striker_id       = -1;
int  non_striker_id   = -1;

bool ball_delivered = false;
bool ball_processed = true;
bool legal_delivery = true;

//  Bowler stats 
atomic<int> bowler_runs   [NUM_BOWLERS];
atomic<int> bowler_wickets[NUM_BOWLERS];

//  Fielder coordination
pthread_mutex_t fielder_mutex      = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t  fielders_active_cv = PTHREAD_COND_INITIALIZER;
pthread_cond_t  fielders_done_cv   = PTHREAD_COND_INITIALIZER;

bool   ball_in_air        = false;
bool   fielder_caught     = false;
double ball_r             = 0.0;
double ball_theta         = 0.0;
double ball_flight_time   = 0.0;
int    fielders_responded = 0;

//  Wicket / run-out state
bool wicket_pending = false;
int  out_batsman_id = -1;

pthread_mutex_t runout_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t  runout_cv    = PTHREAD_COND_INITIALIZER;

bool striker_wants_end2    = false;
bool nonstriker_wants_end1 = false;
bool striker_blocked       = false;
bool nonstriker_blocked    = false;

pthread_cond_t  runout_reply_cv = PTHREAD_COND_INITIALIZER;
bool            runout_decided  = false;
bool            nonstriker_out  = false;

pthread_cond_t  nonstriker_run_cv   = PTHREAD_COND_INITIALIZER;
bool            nonstriker_run_flag = false;
bool            runout_episode_done = true;

//  Synchronization primitives 
pthread_mutex_t pitch_mutex  = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t score_mutex  = PTHREAD_MUTEX_INITIALIZER;

pthread_mutex_t umpire_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t  umpire_cv    = PTHREAD_COND_INITIALIZER;
bool            stroke_done  = false;

// NOTE: bowler_cv and batsman_cv are dynamic-initialised in main()
pthread_cond_t bowler_cv [NUM_BOWLERS];
pthread_cond_t batsman_cv[NUM_BATSMEN];

sem_t crease_sem;

pthread_mutex_t end1_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t end2_mutex = PTHREAD_MUTEX_INITIALIZER;

pthread_mutex_t start_mutex   = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t  start_cv      = PTHREAD_COND_INITIALIZER;
bool            match_started = false;

//  Batting order 
vector<Batsman> batting_order;
int             next_batsman_index = 2;
bool            sjf_mode           = false;
