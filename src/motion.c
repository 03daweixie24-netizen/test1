#include "motion.h"
#include <libopencm3/cm3/nvic.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/timer.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

// Global Variables
// 所以每次讀取時都要去記憶體重讀，不能用暫存器的舊值。
volatile MotionProfile motion = {0};
volatile int32_t pos_x = 0;
volatile int32_t pos_y = 0;
volatile int32_t pos_z = 0;

// S-Curve Formula: SmoothStep (3t^2 - 2t^3)
// 輸入 t: 目前進度 (0.0 = 剛開始, 1.0 = 結束)
// 回傳: 經過 S 曲線調整後的比例 (0.0 ~ 1.0)
// 特性: 起步和結束時的變化率(加速度)為 0，所以不會有頓挫感。
static float s_curve_factor(float t) {
  if (t <= 0.0f)
    return 0.0f;
  if (t >= 1.0f)
    return 1.0f;
  return t * t * (3.0f - 2.0f * t);
}

void gpio_setup(void) {
  rcc_periph_clock_enable(RCC_GPIOA);
  rcc_periph_clock_enable(RCC_GPIOB);
  gpio_mode_setup(GPIOA, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE,
                  GPIO5 | GPIO6 | GPIO8 | GPIO9 | GPIO10);
  gpio_mode_setup(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE,
                  GPIO3 | GPIO4 | GPIO5 | GPIO10);
  gpio_set(EN_PORT, EN_PIN);
}

void timer_setup(void) {
  rcc_periph_clock_enable(RCC_TIM2);
  nvic_enable_irq(NVIC_TIM2_IRQ);

  // Reset Timer
  timer_disable_counter(TIM2);
  timer_set_counter(TIM2, 0);

  timer_set_mode(TIM2, TIM_CR1_CKD_CK_INT, TIM_CR1_CMS_EDGE, TIM_CR1_DIR_UP);
  timer_set_prescaler(TIM2, 84 - 1); // 1us tick (Assuming 84MHz APB1)
  timer_set_period(TIM2, 2000 - 1);  // Initial safe speed
  timer_enable_irq(TIM2, TIM_DIER_UIE);
  timer_enable_counter(TIM2);
}

// TIM2 ISR: Step Generation & Speed Update
// 這個函式每隔幾微秒就會被 CPU 呼叫一次
void tim2_isr(void) {
  // 1. 檢查旗標: 確保真的是 時間到了 才進來
  if (timer_get_flag(TIM2, TIM_SR_UIF)) {
    // 2. 清除旗標 (CRITICAL):
    // 如果不手動清成 0，CPU 離開這裡後會立刻又跳回來，導致系統卡死 (Infinite
    // IRQ Loop)
    timer_clear_flag(TIM2, TIM_SR_UIF);

    if (motion.is_moving) {
      // 1. 發送脈衝 (Generate Pulse) - 上緣
      if (motion.dir_x)
        gpio_set(X_STEP_PORT, X_STEP_PIN);
      if (motion.dir_y) {
        gpio_set(Y1_STEP_PORT, Y1_STEP_PIN);
        gpio_set(Y2_STEP_PORT, Y2_STEP_PIN);
      }
      if (motion.dir_z)
        gpio_set(Z_STEP_PORT, Z_STEP_PIN);

      // (Critical Fix) 脈衝寬度延遲
      // 這個迴圈強迫 CPU 空轉約 2 微秒，確保訊號夠寬。
      for (volatile int i = 0; i < 300; i++)
        __asm__("nop");

      // 發送脈衝 - 下緣
      if (motion.dir_x)
        gpio_clear(X_STEP_PORT, X_STEP_PIN);
      if (motion.dir_y) {
        gpio_clear(Y1_STEP_PORT, Y1_STEP_PIN);
        gpio_clear(Y2_STEP_PORT, Y2_STEP_PIN);
      }
      if (motion.dir_z)
        gpio_clear(Z_STEP_PORT, Z_STEP_PIN);

      // 座標與步數更新
      if (motion.dir_x)
        pos_x += motion.dir_x;
      if (motion.dir_y)
        pos_y += motion.dir_y;
      if (motion.dir_z)
        pos_z += motion.dir_z;

      motion.current_step++;

      // 2. 計算 S-Curve 速度 (The Math)
      float target_freq = motion.min_freq;
      uint32_t cs = motion.current_step;

      if (cs <= motion.accel_steps) {
        // 加速段: 把進度 (0~1) 丟進 s_curve_factor 算出平滑比例
        float t = (float)cs / (float)motion.accel_steps;
        target_freq = motion.min_freq +
                      (motion.max_freq - motion.min_freq) * s_curve_factor(t);
      } else if (cs > (motion.total_steps_needed - motion.decel_steps)) {
        // 減速段
        uint32_t steps_left = motion.total_steps_needed - cs;
        float t = (float)steps_left / (float)motion.decel_steps;
        target_freq = motion.min_freq +
                      (motion.max_freq - motion.min_freq) * s_curve_factor(t);
      } else {
        // 均速段 (Cruising)
        target_freq = motion.max_freq;
      }

      // 3. 設定下一發脈衝的鬧鐘 (Update Timer Period)
      if (target_freq < motion.min_freq)
        target_freq = motion.min_freq;
      // ARR = 時鐘頻率 / 目標頻率。例如 1MHz / 1000Hz = 1000 tick
      uint32_t new_arr = (uint32_t)(1000000.0f / target_freq);
      if (new_arr < 100)
        new_arr = 100; // 安全限速，防止太快當機
      timer_set_period(TIM2, new_arr);

      // 4. 檢查是否走完
      if (motion.current_step >= motion.total_steps_needed) {
        // 走完了，把旗標降下來。
        motion.is_moving = 0;
      }
    }
  }
}

void start_move(int32_t dx, int32_t dy, int32_t dz, uint32_t target_hz) {
  if (motion.is_moving)
    return;

  gpio_clear(EN_PORT, EN_PIN); // Enable Drivers

  // Detect Max Steps
  uint32_t steps_x = abs(dx);
  uint32_t steps_y = abs(dy);
  uint32_t steps_z = abs(dz);
  uint32_t total = steps_x;
  if (steps_y > total)
    total = steps_y;
  if (steps_z > total)
    total = steps_z;

  if (total == 0)
    return;

  // Directions
  motion.dir_x = (dx == 0) ? 0 : (dx > 0 ? 1 : -1);
  motion.dir_y = (dy == 0) ? 0 : (dy > 0 ? 1 : -1);
  motion.dir_z = (dz == 0) ? 0 : (dz > 0 ? 1 : -1);

  // Set Dir Pins
  if (motion.dir_x > 0)
    gpio_set(X_DIR_PORT, X_DIR_PIN);
  else
    gpio_clear(X_DIR_PORT, X_DIR_PIN);

  if (motion.dir_y > 0) {
    gpio_set(Y1_DIR_PORT, Y1_DIR_PIN);
    gpio_set(Y2_DIR_PORT, Y2_DIR_PIN);
  } else {
    gpio_clear(Y1_DIR_PORT, Y1_DIR_PIN);
    gpio_clear(Y2_DIR_PORT, Y2_DIR_PIN);
  }

  if (motion.dir_z > 0)
    gpio_set(Z_DIR_PORT, Z_DIR_PIN);
  else
    gpio_clear(Z_DIR_PORT, Z_DIR_PIN);

  // S-Curve Profile Calculation
  motion.total_steps_needed = total;
  motion.current_step = 0;
  motion.min_freq = 500.0f;
  motion.max_freq = (float)target_hz;
  if (motion.max_freq < motion.min_freq)
    motion.max_freq = motion.min_freq;

  // Define Ramps (20% Accel, 20% Decel)
  motion.accel_steps = total / 5;
  motion.decel_steps = total / 5;
  if (motion.accel_steps < 10 && total > 20)
    motion.accel_steps = 10;
  if (motion.decel_steps < 10 && total > 20)
    motion.decel_steps = 10;

  // Reset Timer
  timer_set_period(TIM2, (uint32_t)(1000000.0f / motion.min_freq));
  timer_generate_event(TIM2, TIM_EGR_UG);
  timer_clear_flag(TIM2, TIM_SR_UIF);

  motion.is_moving = 1;
}

// Helper for Manual Move (WASD)
void manual_move(int axis, int dir) {
  int32_t dist = 200; // 1 Revolution
  static uint32_t manual_speed = 1000;

  if (axis == 3) {
    if (dir == 1)
      manual_speed = 500;
    if (dir == 2)
      manual_speed = 2000;
    if (dir == 3)
      manual_speed = 5000;
    printf("Speed: %ld\r\n", manual_speed);
    return;
  }

  if (axis == 0)
    start_move(dir * dist, 0, 0, manual_speed);
  if (axis == 1)
    start_move(0, dir * dist, 0, manual_speed);
  if (axis == 2)
    start_move(0, 0, dir * dist, manual_speed);

  printf("Man -> Pos: %ld %ld %ld\r\n", pos_x, pos_y, pos_z);
}
