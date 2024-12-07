

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/drivers/gpio.h>
#include <string.h>

//associa uma variavel com a componente uat na device tree
#define UART_DEVICE_NODE DT_CHOSEN(zephyr_shell_uart)

#define MSG_SIZE 1 //define o tamnho do vetor de mensagens

/* size of stack area used by each thread */
#define STACKSIZE 1024

/* scheduling priority used by each thread */
#define PRIORITY 1


/* queue to store up to 10 messages (aligned to 4-byte boundary) */
K_MSGQ_DEFINE(uart_msgq, MSG_SIZE, 100, 4);

static const struct device *const uart_dev = DEVICE_DT_GET(UART_DEVICE_NODE);

/* receive buffer used in UART ISR callback */
static char rx_buf[MSG_SIZE]; //vetor rx buf
static int rx_buf_pos; 

/*
 * Read characters from UART until line end is detected. Afterwards push the
 * data to the message queue.
 */
void serial_cb(const struct device *dev, void *user_data) //callback da serial
{// aceita como parametros o ponteiro dev, que contem as informaçoes da device tree sobre a uart
// e o ponteiro user data
	uint8_t c;

	/* read until FIFO empty */
	while (uart_fifo_read(uart_dev, &c, 1) == 1) {
		if ((c == '\n' || c == '\r') && rx_buf_pos > 0) {
			/* terminate string */
			rx_buf[rx_buf_pos] = '\0';

			/* if queue is full, message is silently dropped */
			k_msgq_put(&uart_msgq, &rx_buf, K_NO_WAIT);

			/* reset the buffer (it was copied to the msgq) */
			rx_buf_pos = 0;
		} else if (rx_buf_pos < (sizeof(rx_buf) - 1)) {
			rx_buf[rx_buf_pos++] = c;
		}
		/* else: characters beyond buffer size are dropped */
	}

}

void leitor (){ // Essa função inicia o calback pa a função de leitura do teclado
	

	/* configure interrupt and callback to receive data */
	uart_irq_callback_user_data_set(uart_dev, serial_cb, NULL);// chama a função serial_cb quando o teclado é utilizado

	uart_irq_rx_enable(uart_dev); // inicia a uart 
}
//declaração de criação e inicialização de thread com leitor como função a ser percorrida
K_THREAD_DEFINE(leitura, STACKSIZE,  leitor, NULL, NULL, NULL, PRIORITY, 0, 0);

//INICIO DA TRANSMISSÃO

const struct device *stx = DEVICE_DT_GET(DT_NODELABEL(gpiob)); //coleta as informações da device tree sobre 
//a port B do gpio e salva no ponteiro stx




int i = 0;
void transmissao (struct k_timer *tempo){ //transmissão por bit bang
	uint8_t palavra = 0b01010101; //capsula da mensagem
    uint8_t sync = 0b00010110;
    uint8_t st = 0b00000010;
    // uint8_t id = 0b00001000;
    // uint8_t etx = 0b00000011;
	int aux;

	if (i<8){  // transmissão da parte inicial da capsula
	aux = (palavra >> i) & 0b00000001;
	printk ("número %d\n",aux);
	}
	else if (i<16){
	aux = (sync >> i-8) & 0b00000001;
	printk ("número %d\n",aux);
	}
	else if (i<16){
	aux = (st >> i-16) & 0b00000001;
	printk ("número %d\n",aux);
	}
	else if (i<24){ // transmissão da mensagem
	aux = ( >> i-24) & 0b00000001;
	printk ("número %d\n",aux);
	}
	i++;

	
	
}
K_TIMER_DEFINE(tempo, transmissao, NULL); //define e inicializa meu timer com o respectivo nome e função a ser chamada



void trans (){
	gpio_pin_configure(stx, 3, GPIO_OUTPUT); 
	char tx_buf[MSG_SIZE];
	k_timer_start(&tempo, K_MSEC(100), K_MSEC(1000)); // associa um tempo de expiração do meu timer 



	while (k_msgq_get(&uart_msgq, tx_buf, K_FOREVER) == 0) {
	}
}
	


K_THREAD_DEFINE(transmis, STACKSIZE, trans , NULL, NULL, NULL,
		PRIORITY, 0, 0);