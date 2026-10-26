/*
 * CYD Game Clock - Metrics Display Module
 *
 * Functions for displaying PC stats metrics on the CYD display.
 */

#ifndef METRICS_H
#define METRICS_H

#include "../config/config.h"

// ========== Metrics Display Functions ==========

// Main display function - renders metrics grid
void displayStats();

// Compact grid layout with position-based rendering
void displayStatsCompactGrid();

// Helper to display a single metric
void displayMetricCompact(Metric* m);

// Draw progress bar for a metric
void drawProgressBar(int x, int y, int width, Metric* m);

// Shown on the stats screen when no PC is sending, or the daemon reports a problem
void displayWaitingForPC();
void displayErrorStatus(uint8_t status);

#endif // METRICS_H
