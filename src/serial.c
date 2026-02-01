#include "serial.h"
#include "motion.h"
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/usart.h>

char rx_buffer[RX_BUFFER_SIZE];
int rx_index = 0;

void usart_setup(void) {
  rcc_periph_clock_enable(RCC_GPIOA);
  rcc_periph_clock_enable(RCC_USART2);
  gpio_mode_setup(GPIOA, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO2 | GPIO3);
  gpio_set_af(GPIOA, GPIO_AF7, GPIO2 | GPIO3);
  usart_set_baudrate(USART2, 115200);
  usart_set_databits(USART2, 8);
  usart_set_stopbits(USART2, USART_STOPBITS_1);
  usart_set_mode(USART2, USART_MODE_TX_RX);
  usart_set_parity(USART2, USART_PARITY_NONE);
  usart_set_flow_control(USART2, USART_FLOWCONTROL_NONE);
  usart_enable(USART2);
}

int _write(int file, char *ptr, int len) {
  if (file == 1)
    for (int i = 0; i < len; i++)
      usart_send_blocking(USART2, ptr[i]);
  return len;
}

void process_command(char *line) {
  for (int i = 0; line[i]; i++)
    line[i] = toupper(line[i]);

  // 檢查是否為 G1 指令 (strncmp 比 strcmp 安全，只比對前 2 個字元)
  if (strncmp(line, "G1", 2) == 0) {
    int32_t tx = pos_x, ty = pos_y, tz = pos_z;
    uint32_t fr = 2000; // Default Feedrate

    // 指標 p 指向 "G1 X..." 的 'X' 位置 (line + 2)
    char *p = line + 2;
    while (*p) {
      if (*p == 'X')
        // strtol的神奇用法: &p 會把指標自動移到數字的後面
        // 例如讀完 "1000"，p 就會指到下一個字元 (可能是空格或 'Y')
        tx = (int32_t)strtol(p + 1, &p, 10);
      else if (*p == 'Y')
        ty = (int32_t)strtol(p + 1, &p, 10);
      else if (*p == 'Z')
        tz = (int32_t)strtol(p + 1, &p, 10);
      else if (*p == 'F')
        fr = strtol(p + 1, &p, 10);
      else
        p++; // 如果不是關鍵字，就往後找
    }

    if (motion.is_moving == 0) {
      printf("S-Curve Move: X%ld Y%ld Z%ld F%ld\r\n", tx, ty, tz, fr);
      start_move(tx - pos_x, ty - pos_y, tz - pos_z, fr);
    } else {
      printf("busy\r\n");
    }
  } else if (strncmp(line, "M114", 4) == 0) {
    printf("X:%ld Y:%ld Z:%ld\r\n", pos_x, pos_y, pos_z);
  }
}
