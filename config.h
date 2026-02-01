#ifndef CONFIG_H
#define CONFIG_H

#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/rcc.h>

// ==========================================
// System Configuration
// ==========================================
// STM32F446 的 CPU 頻率
#define SYSTEM_CLOCK_MHZ 168

// ==========================================
// Pin Definitions (CNC Shield V3 Mapping)
// ==========================================
#define EN_PORT GPIOA
#define EN_PIN GPIO9

// X Axis (對應 CNC Shield 的 X 軸插槽)
#define X_STEP_PORT GPIOA
#define X_STEP_PIN GPIO10
#define X_DIR_PORT GPIOB
#define X_DIR_PIN GPIO4

// Y1 Axis (對應 CNC Shield 的 Y 軸插槽)
// 這也是一般 3D 印表機的左邊馬達
#define Y1_STEP_PORT GPIOB
#define Y1_STEP_PIN GPIO3
#define Y1_DIR_PORT GPIOB
#define Y1_DIR_PIN GPIO10

// Y2 Axis (對應 CNC Shield 的 A 軸插槽)
#define Y2_STEP_PORT GPIOA
#define Y2_STEP_PIN GPIO6
#define Y2_DIR_PORT GPIOA
#define Y2_DIR_PIN GPIO5

// Z Axis (對應 CNC Shield 的 Z 軸插槽)
#define Z_STEP_PORT GPIOB
#define Z_STEP_PIN GPIO5
#define Z_DIR_PORT GPIOA
#define Z_DIR_PIN GPIO8

// Serial Buffer (接收指令的長度限制)
#define RX_BUFFER_SIZE 128

#endif // CONFIG_H
