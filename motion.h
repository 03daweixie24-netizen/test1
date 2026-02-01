#ifndef MOTION_H
#define MOTION_H

#include "config.h"
#include <stdint.h>


// ==========================================
// S-Curve Motion Parameters
// ==========================================
typedef struct {
  uint32_t total_steps_needed;
  uint32_t accel_steps;
  uint32_t decel_steps;
  uint32_t const_steps;

  float min_freq; // Start Hz
  float max_freq; // Cruise Hz (Feedrate)

  volatile uint32_t current_step;
  volatile int dir_x, dir_y, dir_z;
  volatile int is_moving; // Flag for main loop
} MotionProfile;

// Global Motion State
extern volatile MotionProfile motion;
extern volatile int32_t pos_x;
extern volatile int32_t pos_y;
extern volatile int32_t pos_z;

// Function Prototypes
void timer_setup(void);
void gpio_setup(void);
void start_move(int32_t dx, int32_t dy, int32_t dz, uint32_t target_hz);
void manual_move(int axis, int dir);

#endif // MOTION_H
