// Standard library includes.
#include <stdint.h>
#include <string.h>

// Device header files.
#include "gd32vf103.h"
#include "riscv_encoding.h"

// UART definitions
//-----------------
#define RCU_CTL *(int *)(0x40021000)     // RCU Control register
#define RCU_CFG0 *(int *)(0x40021004)    // RCU Clock configuration register 0
#define RCU_APB2RST *(int *)(0x4002100c) // RCU APB2 reset register
#define RCU_APB2EN *(int *)(0x40021018)  // RCU APB2 enable register
#define GPIOA_CTL1 *(int *)(0x40010804)  // Port control register 1
// UART1 registers
#define UART0_ADDRESS 0x40013800
#define STAT *(int *)(UART0_ADDRESS + 0x0)  // Status register
#define DATA *(int *)(UART0_ADDRESS + 0x4)  // Data register
#define BAUD *(int *)(UART0_ADDRESS + 0x08) // Baud rate register
#define CTL0 *(int *)(UART0_ADDRESS + 0x0C) // Control register 0
#define CTL1 *(int *)(UART0_ADDRESS + 0x10) // Control register 1
#define CTL2 *(int *)(UART0_ADDRESS + 0x14) // Control register 2
#define GP *(int *)(UART0_ADDRESS + 0x18)   // Guard time and prescaler register
//-----------------
#define check_bit(value, n) (value & (1U << (n)))

// Pre-defined memory locations for program initialization.
extern uint32_t _sidata, _sdata, _edata, _sbss, _ebss;
// Current system core clock speed.
volatile uint32_t SystemCoreClock = 8000000;

// Simple 'busy loop' delay method.
__attribute__((optimize("O0"))) void delay_cycles(uint32_t cyc)
{
  uint32_t d_i;
  for (d_i = 0; d_i < cyc; ++d_i)
  {
    __asm__("nop");
  }
}

void prepare_device()
{
  // Set up system so UART can be used
  RCU_CFG0 = 0x20280000;
  RCU_CTL = 0x01006883;
  RCU_CFG0 = 0x20280002;
  RCU_APB2RST = 0x00004005;
  RCU_APB2RST = 0x0;
  RCU_APB2EN = 0x0000501d;
  GPIOA_CTL1 = 0x888444b4;
}

void initialize_uart()
{
  // Set up UART
  CTL0 = 0x0;
  BAUD = 0x3a9;
  CTL0 = 0x200c;
}

void uart_send(const char *msg) {
  const char *current_char = msg;

  // Transmission loop
  while (*current_char != '\0') {
    int stat_content = STAT;

    // Check if TBE (Transmitter buffer empty) bit is set to 1
    if (check_bit(stat_content, 7)) {
      // Buffer is empty, we can write data
      DATA = *current_char;

      current_char++; 
    }
  }
}

// 'main' method which gets called from the boot code.
int main(void)
{
  // Copy initialized data from .sidata (Flash) to .data (RAM)
  memcpy(&_sdata, &_sidata, ((void *)&_edata - (void *)&_sdata));
  // Clear the .bss RAM section.
  memset(&_sbss, 0x00, ((void *)&_ebss - (void *)&_sbss));

  prepare_device();
  initialize_uart();
  const char *message = "Hello UART ";

  uart_send(message);

  return 0;
}
