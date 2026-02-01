/**
 * 智慧點膠機 - S-Curve 加減速控制 (Modular Version)
 * 功能: 實作 "SmoothStep" S 型加減速曲線
 *
 * Target Board: STM32 Nucleo-F446RE
 * Clock: 168MHz
 */

#include "config.h"
#include "motion.h"
#include "serial.h"

#include <libopencm3/cm3/scb.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/usart.h>
#include <stdio.h>

// ==========================================
// System Setup
// ==========================================
static void clock_setup(void) {
  // 設定由外部晶振 (HSE) 倍頻到 168MHz (STM32F446 的極限速度)
  rcc_clock_setup_pll(&rcc_hse_8mhz_3v3[RCC_CLOCK_3V3_168MHZ]);

  // (Critical) 開啟 FPU (浮點運算單元)
  SCB_CPACR |= ((3UL << 10 * 2) | (3UL << 11 * 2));
}

// ==========================================
// Main 
// ==========================================
int main(void) {
  clock_setup();        
  gpio_setup();        
  usart_setup();        
  timer_setup();        // 鬧鐘 (S-Curve 引擎)
  setbuf(stdout, NULL); // 讓 printf 直接輸出，不要緩衝

  printf("\r\n=== STM32 S-Curve Motion (Modular) ===\r\n");
  printf("Controls: [W/S] Y, [A/D] X, [Q/E] Z, [1-3] Speed\r\n");

  int was_moving = 0;
  while (1) {
    // 狀態機: 偵測馬達是否剛停下來
    // 這種寫法可以避免在 ISR 裡面 print，防止卡死
    if (was_moving && motion.is_moving == 0) {
      printf("ok: Pos X%ld Y%ld Z%ld\r\n", pos_x, pos_y, pos_z);
      was_moving = 0;
    }
    if (motion.is_moving)
      was_moving = 1;

    // 輪詢: 檢查有沒有收到新字元
    if ((USART_SR(USART2) & USART_SR_RXNE)) {
      char c = usart_recv(USART2);

      // Special handling for WASD when buffer is empty (Manual Mode)
      if (rx_index == 0) {
        int handled = 1;
        switch (c) {
        case 'w':
        case 'W':
          manual_move(1, 1);
          break; // Y+
        case 's':
        case 'S':
          manual_move(1, -1);
          break; // Y-
        case 'a':
        case 'A':
          manual_move(0, -1);
          break; // X-
        case 'd':
        case 'D':
          manual_move(0, 1);
          break; // X+
        case 'q':
        case 'Q':
          manual_move(2, 1);
          break; // Z+
        case 'e':
        case 'E':
          manual_move(2, -1);
          break; // Z-
        case '1':
          manual_move(3, 1);
          break; // Speed 1
        case '2':
          manual_move(3, 2);
          break; // Speed 2
        case '3':
          manual_move(3, 3);
          break; // Speed 3
        default:
          handled = 0;
          break;
        }
        if (handled)
          continue; // Skip buffer logic
      }

      usart_send_blocking(USART2, c); // Echo
      if (c == '\n' || c == '\r') {
        rx_buffer[rx_index] = '\0';
        if (rx_index > 0) {
          usart_send_blocking(USART2, '\r');
          usart_send_blocking(USART2, '\n');
          process_command(rx_buffer);
        }
        rx_index = 0;
      } else {
        if (rx_index < RX_BUFFER_SIZE - 1)
          rx_buffer[rx_index++] = c;
      }
    }
  }
  return 0;
}
