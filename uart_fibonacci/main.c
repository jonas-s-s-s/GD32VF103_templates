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

// Simple 'busy loop' delay method.
__attribute__((optimize("O0"))) void delay_cycles(uint32_t cyc)
{
  uint32_t d_i;
  for (d_i = 0; d_i < cyc; ++d_i)
  {
    __asm__("nop");
  }
}

// ####################################################
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

// ####################################################
// # MATH FUNCTIONS
// ####################################################

int fib(int n) {
    int a, b, c;
    a = 0;
    b = 1;
    c = 0;
    for (int i = 0; i < n; i++) {
        a = b+c;
        b = c;
        c = a;
    }
    return a;
}

// ####################################################
// # STRING MANIPULATION FUNCTIONS
// ####################################################

int strtoi(const char *s)
{
  char *p = (char *)s;
  int nmax = (1ULL << 31) - 1; /* INT_MAX  */
  int nmin = -nmax - 1;        /* INT_MIN  */
  long long sum = 0;
  char sign = *p;

  if (*p == '-' || *p == '+')
    p++;

  while (*p >= '0' && *p <= '9')
  {
    sum = sum * 10 - (*p - '0');
    if (sum < nmin || (sign != '-' && -sum > nmax))
      goto error;
    p++;
  }

  if (sign != '-')
    sum = -sum;

  return (int)sum;

error:
  return 0;
}

char itoa_buffer[MAX_LINE_SIZE];
void itoa(int n)
{
  int i = 0;

  short isNeg = n < 0;

  unsigned int n1 = isNeg ? -n : n;

  while (n1 != 0)
  {
    itoa_buffer[i++] = n1 % 10 + '0';
    n1 = n1 / 10;
  }

  if (isNeg)
    itoa_buffer[i++] = '-';

  itoa_buffer[i] = '\0';

  for (int t = 0; t < i / 2; t++)
  {
    itoa_buffer[t] ^= itoa_buffer[i - t - 1];
    itoa_buffer[i - t - 1] ^= itoa_buffer[t];
    itoa_buffer[t] ^= itoa_buffer[i - t - 1];
  }

  if (n == 0)
  {
    itoa_buffer[0] = '0';
    itoa_buffer[1] = '\0';
  }
}

// ####################################################
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
  initialize_uart();

  const char *message = "FIBONACCI NUMBER GENERATOR\nPlease enter a number...\n";
  uart_send(message);

  // Listen for user input and perform fib
  while (1)
  {
    uart_getline();

    // Convert user input to int
    int num = strtoi(getline_buffer);

    // Compute fib
    int result = fib(num);

    // Print F(
    char msg1[] = {'F', '(', '\0'};
    uart_send(msg1);

    // Print input number
    itoa(num);
    uart_send(itoa_buffer);

    // Print ) = 
    char msg2[] = {')', ' ', '=', ' ', '\0'};
    uart_send(msg2);

    // Print result integer
    itoa(result);
    uart_send(itoa_buffer);

    // Move to next line
    char endLineStr[] = {'\n', '\0'};
    uart_send(endLineStr);

  }

  return 0;
}
