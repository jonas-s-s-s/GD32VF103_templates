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
// UART0 registers
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
#define MAX_LINE_SIZE 255

// Pre-defined memory locations for program initialization.
extern uint32_t _sidata, _sdata, _edata, _sbss, _ebss;
// Current system core clock speed.
volatile uint32_t SystemCoreClock = 8000000;

// ###################################################
// # UART FUNCTIONS
// ####################################################

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

void uart_send(const char *msg)
{
  const char *current_char = msg;

  // Transmission loop
  while (*current_char != '\0')
  {
    int stat_content = STAT;

    // Check if TBE (Transmitter buffer empty) bit is set to 1
    if (check_bit(stat_content, 7))
    {
      // Buffer is empty, we can write data
      DATA = *current_char;

      current_char++;
    }
  }
}

char uart_receive()
{
  char content;

  while (1)
  {
    int stat_content = STAT;

    // Check if RBNE (Receiver buffer not empty) bit is set to 1
    if (check_bit(stat_content, 5))
    {
      content = DATA;
      return content;
    }
  }
}

// Global variable for getline function (no stdlib so dynamic alloc cannot be used)
char getline_buffer[MAX_LINE_SIZE];
void uart_getline()
{
  int i = 0;
  while (1)
  {
    char input = uart_receive();
    // Append char
    getline_buffer[i] = input;

    // Detect line feed (LF = 0xA in ASCII)
    if (input == 0xA)
    {
      getline_buffer[i + 1] = '\0';
      return;
    }

    // Detect if max size was exceeded
    if (i >= MAX_LINE_SIZE)
    {
      getline_buffer[MAX_LINE_SIZE - 1] = '\0';
      return;
    }

    i++;
  }
}

// ###################################################
// # LED FUNCTIONS
// ####################################################

void led_initialize()
{
  // Enable the GPIOA and GPIOC peripherals.
  RCC->APB2ENR |=  ( RCC_APB2ENR_IOPAEN |
                     RCC_APB2ENR_IOPCEN );

  // Configure pins A1, A2, and C13 as low-speed push-pull outputs.
  GPIOA->CRL   &= ~( GPIO_CRL_MODE1 | GPIO_CRL_CNF1 |
                     GPIO_CRL_MODE2 | GPIO_CRL_CNF2 );
  GPIOA->CRL   |=  ( ( 0x2 << GPIO_CRL_MODE1_Pos ) |
                     ( 0x2 << GPIO_CRL_MODE2_Pos ) );
  GPIOC->CRH   &= ~( GPIO_CRH_MODE13 | GPIO_CRH_CNF13 );
  GPIOC->CRH   |=  ( 0x2 << GPIO_CRH_MODE13_Pos );

  // Turn all three LEDs off.
  // The pins are connected to the LED cathodes, so pulling
  // the pin high turns the LED off, and low turns it on.
  GPIOA->ODR   |=  ( 0x1 << 1 |
                     0x1 << 2 );
  GPIOC->ODR   |=  ( 0x1 << 13 );
}

// ###################################################
// # MAIN
// ####################################################

// 'main' method which gets called from the boot code.
int main(void)
{
  // Copy initialized data from .sidata (Flash) to .data (RAM)
  memcpy(&_sdata, &_sidata, ((void *)&_edata - (void *)&_sdata));
  // Clear the .bss RAM section.
  memset(&_sbss, 0x00, ((void *)&_ebss - (void *)&_sbss));

  prepare_device();

  led_initialize();

  initialize_uart();

  const char *message = "LED TOGGLE\nInput one of the following numbers:\n1 = Toggle RED led\n2 = Toggle GREEN led\n3 = Toggle BLUE led\n\n";
  uart_send(message);

  // LED state
  short red = 0;
  short green = 0;
  short blue = 0;

  // Listen for user input
  while (1)
  {
    uart_getline();

    if (getline_buffer[0] == '1')
    {
      uart_send("Toggling RED...\n");
      if (red)
      {
        // Turn off red
        GPIOC->ODR |= (0x1 << 13);
      }
      else
      {
        // Turn on red
        GPIOC->ODR &= ~(0x1 << 13);
      }
      red = !red;
    }
    else if (getline_buffer[0] == '2')
    {
      uart_send("Toggling GREEN...\n");
      if (green)
      {
        // Turn off green
        GPIOA->ODR |= (0x1 << 1);
      }
      else
      {
        // Turn on green
        GPIOA->ODR &= ~(0x1 << 1);
      }
      green = !green;
    }
    else if (getline_buffer[0] == '3')
    {
      uart_send("Toggling BLUE...\n");
      if (blue)
      {
        // Turn off blue
        GPIOA->ODR |= (0x1 << 2);
      }
      else
      {
        // Turn on blue
        GPIOA->ODR &= ~(0x1 << 2);
      }
      blue = !blue;
    }
    else
    {
      uart_send("Wrong input.\n");
    }
  }

  return 0;
}
