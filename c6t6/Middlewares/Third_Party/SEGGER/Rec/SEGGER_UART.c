/**********************************************************
 *          SEGGER MICROCONTROLLER SYSTEME GmbH
 *   Solutions for real time microcontroller applications
 ***********************************************************
 File    : HIF_UART.c
 Purpose : Terminal control for Flasher using USART1 on PA9/PA10
 --------- END-OF-HEADER ---------------------------------*/

#include "SEGGER_SYSVIEW.h"

#if (SEGGER_UART_REC == 1)
#include "SEGGER_RTT.h"
#include "stm32f1xx.h"

#define OS_FSYS 72000000L   // MCU core frequency (72 MHz for STM32F103C6T6)
#define RCC_BASE_ADDR       0x40021000

#define OFF_APB1ENR         0x1C        // APB1 peripheral clock enable register
#define OFF_APB2ENR         0x18        // APB2 peripheral clock enable register

#define RCC_APB1ENR         *(volatile uint32_t*)(RCC_BASE_ADDR + OFF_APB1ENR)
#define RCC_APB2ENR         *(volatile uint32_t*)(RCC_BASE_ADDR + OFF_APB2ENR)

#define GPIOA_BASE_ADDR     0x40010800

#define OFF_CRL             0x00        // GPIOx_CRL (GPIO port configuration low register)
#define OFF_CRH             0x04        // GPIOx_CRH (GPIO port configuration high register)
#define OFF_IDR             0x08        // GPIOx_IDR (GPIO port input data register)
#define OFF_ODR             0x0C        // GPIOx_ODR (GPIO port output data register)
#define OFF_BSRR            0x10        // GPIOx_BSRR (GPIO port bit set/reset register)
#define OFF_LCKR            0x18        // GPIOx_LCKR (GPIO port configuration lock register)

#define USART1_BASE_ADDR    0x40013800

#define OFF_SR              0x00        // Status register
#define OFF_DR              0x04        // Data register
#define OFF_BRR             0x08        // Baudrate register
#define OFF_CR1             0x0C        // Control register 1
#define OFF_CR2             0x10        // Control register 2
#define OFF_CR3             0x14        // Control register 3

#define UART_BASECLK        OS_FSYS          // USART1 runs on APB2 clock (72 MHz)
#define GPIO_BASE_ADDR      GPIOA_BASE_ADDR
#define USART_BASE_ADDR     USART1_BASE_ADDR
#define GPIO_UART_TX_BIT    9               // USART1 TX: Pin PA9
#define GPIO_UART_RX_BIT    10              // USART1 RX: Pin PA10
#define USART_IRQn          USART1_IRQn

#define GPIO_CRH            *(volatile uint32_t*)(GPIO_BASE_ADDR + OFF_CRH)

#define USART_SR            *(volatile uint32_t*)(USART_BASE_ADDR + OFF_SR)
#define USART_DR            *(volatile uint32_t*)(USART_BASE_ADDR + OFF_DR)
#define USART_BRR           *(volatile uint32_t*)(USART_BASE_ADDR + OFF_BRR)
#define USART_CR1           *(volatile uint32_t*)(USART_BASE_ADDR + OFF_CR1)
#define USART_CR2           *(volatile uint32_t*)(USART_BASE_ADDR + OFF_CR2)
#define USART_CR3           *(volatile uint32_t*)(USART_BASE_ADDR + OFF_CR3)

#define USART_RXNE          5     // Read data register not empty flag (SR)
#define USART_TC            6     // Transmission complete flag (SR)
#define USART_RX_ERROR_FLAGS 0xB  // Parity, framing, overrun error flags (SR)
#define USART_RXNEIE        5     // Read data register not empty interrupt enable (CR1)
#define USART_TCIE          6     // Transmission complete interrupt enable (CR1)
#define USART_TXE           7     // Transmit data register empty
#define USART_TXEIE         7     // Transmit data register empty interrupt enable

typedef void UART_ON_RX_FUNC(uint8_t Data);
typedef int UART_ON_TX_FUNC(uint8_t *pChar);

typedef UART_ON_TX_FUNC *UART_ON_TX_FUNC_P;
typedef UART_ON_RX_FUNC *UART_ON_RX_FUNC_P;

static UART_ON_RX_FUNC_P _cbOnRx;
static UART_ON_TX_FUNC_P _cbOnTx;

void HIF_UART_Init(uint32_t Baudrate, UART_ON_TX_FUNC_P cbOnTx,
		UART_ON_RX_FUNC_P cbOnRx);

#define _SERVER_HELLO_SIZE        (4)
#define _TARGET_HELLO_SIZE        (4)

static const U8 _abHelloMsg[_TARGET_HELLO_SIZE] = { 'S', 'V',
		(SEGGER_SYSVIEW_VERSION / 10000), (SEGGER_SYSVIEW_VERSION / 1000) % 10 };

static struct {
	U8 NumBytesHelloRcvd;
	U8 NumBytesHelloSent;
	int ChannelID;
} _SVInfo = { 0, 0, 1 };

static void _StartSysView(void) {
	int r;
	r = SEGGER_SYSVIEW_IsStarted();
	if (r == 0) {
		SEGGER_SYSVIEW_Start();
	}
}

static void _cbOnUARTRx(U8 Data) {
	if (_SVInfo.NumBytesHelloRcvd < _SERVER_HELLO_SIZE) {
		_SVInfo.NumBytesHelloRcvd++;
		goto Done;
	}
	_StartSysView();
	SEGGER_RTT_WriteDownBuffer(_SVInfo.ChannelID, &Data, 1);
	Done: return;
}

static int _cbOnUARTTx(U8 *pChar) {
	int r;
	if (_SVInfo.NumBytesHelloSent < _TARGET_HELLO_SIZE) {
		*pChar = _abHelloMsg[_SVInfo.NumBytesHelloSent];
		_SVInfo.NumBytesHelloSent++;
		r = 1;
		goto Done;
	}
	r = SEGGER_RTT_ReadUpBufferNoLock(_SVInfo.ChannelID, pChar, 1);
	if (r < 0) {
		r = 0;
	}
	Done: return r;
}

void SEGGER_UART_init(U32 baud) {
	HIF_UART_Init(baud, _cbOnUARTTx, _cbOnUARTRx);
}

void HIF_UART_WaitForTxEnd(void) {
	while ((USART_SR & (1 << USART_TXE)) == 0)
		; // Wait until transmit buffer empty
	while ((USART_SR & (1 << USART_TC)) == 0)
		;  // Wait until transmission is complete
}

void USART1_IRQHandler(void) {
	int UsartStatus;
	uint8_t v;
	int r;

	UsartStatus = USART_SR;
	if (UsartStatus & (1 << USART_RXNE)) {
		v = USART_DR;
		if ((UsartStatus & USART_RX_ERROR_FLAGS) == 0) {
			(void) v;
			if (_cbOnRx) {
				_cbOnRx(v);
			}
		}
	}
	if (UsartStatus & (1 << USART_TXE)) {
		if (_cbOnTx == NULL) {
			return;
		}
		r = _cbOnTx(&v);
		if (r == 0) {
			USART_CR1 &= ~(1UL << USART_TXEIE);
		} else {
			USART_SR;
			USART_DR = v;
		}
	}
}

void HIF_UART_EnableTXEInterrupt(void) {
	USART_CR1 |= (1 << USART_TXEIE);
}

void HIF_UART_Init(uint32_t Baudrate, UART_ON_TX_FUNC_P cbOnTx,
        UART_ON_RX_FUNC_P cbOnRx) {
    uint32_t v;
    uint32_t Div;

    // Enable clocks
    RCC_APB2ENR |= (1 << 2) | (1 << 14); // Enable GPIOA and USART1 clocks

    // Configure USART RX/TX pins (PA9, PA10) for alternate function
    v = GPIO_CRH;
    v &= ~((0xF << ((GPIO_UART_TX_BIT - 8) * 4))
            | (0xF << ((GPIO_UART_RX_BIT - 8) * 4)));
    v |= (0xB << ((GPIO_UART_TX_BIT - 8) * 4))    // TX: AF push-pull, 50MHz
            | (0x4 << ((GPIO_UART_RX_BIT - 8) * 4)); // RX: Input floating (sửa từ 0x8 thành 0x4)
    GPIO_CRH = v;

    // Initialize USART
    USART_CR1 = 0 | (0 << 15)  // OVER8 = 0; Oversampling by 16 (sửa từ 1 thành 0)
            | (1 << 13)  // UE = 1; USART enabled
            | (0 << 12)  // M = 0; 8 data bits
            | (0 << 10)  // PCE = 0; No parity
            | (1 << 5)   // RXNEIE = 1; RX interrupt enabled
            | (1 << 3)   // TE = 1; Transmitter enabled
            | (1 << 2);  // RE = 1; Receiver enabled

    USART_CR2 = 0 | (0 << 12); // STOP = 00b; 1 stop bit

    USART_CR3 = 0 | (0 << 11)  // ONEBIT = 0; Three sample bit method
            | (0 << 7);  // DMAT = 0; DMA disabled (sửa từ 1 thành 0 để đơn giản)

    // Set baud rate - Công thức đúng cho oversampling 16
    Div = (UART_BASECLK + (Baudrate/2)) / Baudrate;  // Làm tròn

    // Kiểm tra giới hạn
    if (Div < 1) {
        Div = 1;
    }
    if (Div > 0xFFFF) {
        Div = 0xFFFF;
    }

    USART_BRR = Div;  // Gán trực tiếp

    // Setup callbacks and enable interrupt
    _cbOnRx = cbOnRx;
    _cbOnTx = cbOnTx;
    NVIC_SetPriority(USART_IRQn, 6);
    NVIC_EnableIRQ(USART_IRQn);
}

#endif
