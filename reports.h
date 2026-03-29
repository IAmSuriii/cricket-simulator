/*
 * reports.h
 
 * Declares the three post-innings report printers:
 *
 *  print_gantt_chart()    — ball-by-ball Gantt with ANSI colour.
 *  print_bowler_summary() — per-bowler runs/wickets totals.
 *  print_wait_analysis()  — batsman wait-time table.

 */

#pragma once
#include "globals.h"

/*
 * Prints the Gantt chart to stdout.
 * Colour key:
 *   RED    = wicket (bowled/caught/LBW) or run-out
 *   YELLOW = wide delivery
 *   none   = normal legal delivery
 */
void print_gantt_chart();

/*
 * Prints per-bowler run and wicket totals.
 * Also verifies that the sum of bowler_runs equals global_score.
 */
void print_bowler_summary();

/*
 * Prints how long each batsman waited before reaching the crease.
 * innings parameter is used in the section heading only.
 */
void print_wait_analysis(int innings);
