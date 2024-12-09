

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/drivers/gpio.h>
#include <string.h>

//associa uma variavel com a componente uat na device tree
#define UART_DEVICE_NODE DT_CHOSEN(zephyr_shell_uart)

#define MSG_SIZE 8 //define o tamnho do vetor de mensagens

/* size of stack area used by each thread */
#define STACKSIZE 1024

/* scheduling priority used by each thread */
#define PRIORITY 1


K_MUTEX_DEFINE	(mut1);


/* queue to store up to 10 messages (aligned to 4-byte boundary) */
K_MSGQ_DEFINE(uart_msgq, MSG_SIZE, 10, 4);

static const struct device *const uart_dev = DEVICE_DT_GET(UART_DEVICE_NODE);

/* receive buffer used in UART ISR callback */
static char rx_buf[MSG_SIZE]; //vetor rx buf
static int rx_buf_pos =0; 

/*
 * Read characters from UART until line end is detected. Afterwards push the
 * data to the message queue.
 */
void serial_cb(const struct device *dev, void *user_data) //callback da serial
{// aceita como parametros o ponteiro dev, que contem as informaçoes da device tree sobre a uart
// e o ponteiro user data
	uint8_t c;
	
	/* read until FIFO empty */
	if (k_mutex_lock(&mut1, K_FOREVER) == 0){
	while (uart_fifo_read(uart_dev, &c, 1) == 1) {
		
		if ((c == '\n' || c == '\r') && rx_buf_pos > 0) {
			/* terminate string */
			rx_buf[rx_buf_pos] = '\0';

			/* if queue is full, message is silently dropped */
			k_msgq_put(&uart_msgq, &rx_buf, K_NO_WAIT);

			/* reset the buffer (it was copied to the msgq) */
			rx_buf_pos = 0;
			k_mutex_unlock(&mut1);
			printk("Carregando dados\n");
		} else if (rx_buf_pos < (sizeof(rx_buf) - 1)) {
			rx_buf[rx_buf_pos++] = c;
			printk ("guardei %c\n",c);
		}
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

int i = 0;//desfazer
uint8_t j = 0; //variavel de contagem de elemntos dentro do vetor
int m = 1; // variavel auxiliar que faz a contagem de dados enviados por capsula
int k; // variavel auxiliar para a chamada do etx
uint8_t  d = 0;
int vetaux[7];
void transmissao (struct k_timer *tempo){ //transmissão por bit bang
	uint8_t palavra = 0b01010101; //capsula da mensagem
    uint8_t sync = 0b00010110;
    uint8_t st = 0b00000010;
    uint8_t id = 0b00001000;
    uint8_t etx = 0b00000011;
	uint8_t aux;
	
	char tx_buf[MSG_SIZE];

	if (k_mutex_lock(&mut1, K_FOREVER) == 0){
		//printk("to com o mutex\n");
		if(i ==  0 ){//desfazer
			k_msgq_get(&uart_msgq, tx_buf, K_NO_WAIT);
			while (tx_buf[j] != '\0'){
				printk("tx_buf = %c\n",tx_buf[j]);
				vetaux[j] = tx_buf[j];
				j++;
			}
			printk("vet  %d\n",j);
		}
			if (i<8){  // transmissão da parte inicial da capsula
				aux = (palavra >> i) & 0b00000001;
				gpio_pin_set(stx, 3, aux);
				//printk ("Palavra %d\n",aux);
			}
			else if (i<16){
				aux = (sync >> (i%8)) & 0b00000001;
				gpio_pin_set(stx, 3, aux);
				//printk ("Sync %d\n",aux);
			}
			else if (i<24){
				aux = (st >> (i%8)) & 0b00000001;
				gpio_pin_set(stx, 3, aux);
				//printk ("STX %d\n",aux);
			}
			else if (i<32){
				d = (id | j);
				aux = (d >> (i%8)) & 0b00000001;
				gpio_pin_set(stx, 3, aux);
				//printk ("id %d\n",aux);
			}
			if(i > 32){
				if ((m = (j - ((i-32)/8)) )> 0 ){ // transmissão da mensagem
					aux = (vetaux[(i-32)/8] >> (i%8) & 0b00000001);
					gpio_pin_set(stx, 3, aux);
					//printk (" dado %d  letra %c\n ",aux , vetaux[(i-32)/8]);
				}
			}
			if (m == 0){
				aux = (etx >> (i%8)) & 0b00000001;
				gpio_pin_set(stx, 3, aux);
				//printk ("ETX %d\n",aux);
				k++;
			}
			i++;
			if (k == 8){
                i = 0;// desfazer
				m = 1;
                j = 0;
				k = 0;
			}


		k_mutex_unlock(&mut1);
	}
}
	
	

K_TIMER_DEFINE(tempo1, transmissao, NULL); //define e inicializa meu timer com o respectivo nome e função a ser chamada

void trans (){
	gpio_pin_configure(stx, 3, GPIO_OUTPUT); 
	k_timer_start(&tempo1, K_MSEC(400), K_MSEC(400)); // associa um tempo de expiração do meu timer 

}
K_THREAD_DEFINE(transmis, STACKSIZE, trans , NULL, NULL, NULL,
		2, 0, 0);

//INICIO DA RECEPÇÂO

uint32_t reserva;
void recepcao (){
	reserva = reserva << 1;
	reserva = ( reserva | gpio_pin_get(stx, 2));
}
K_TIMER_DEFINE(tempo2, recepcao, NULL); //define e inicializa meu timer com o respectivo nome e função a ser chamada

void mainrecepcao(){
	gpio_pin_configure(stx, 2, GPIO_INPUT);
	k_timer_start(&tempo2, K_MSEC(100), K_MSEC(100));
}


K_THREAD_DEFINE(recept, STACKSIZE, mainrecepcao , NULL, NULL, NULL,
		3, 0, 0);