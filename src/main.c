

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


K_MUTEX_DEFINE	(mut1); //define e inicializa o mutex 1


/* queue to store up to 10 messages (aligned to 4-byte boundary) */
K_MSGQ_DEFINE(uart_msgq, MSG_SIZE, 10, 4);

static const struct device *const uart_dev = DEVICE_DT_GET(UART_DEVICE_NODE); //coloca as informações da uart 
//no ponteiro uart_dev

/* receive buffer used in UART ISR callback */
static char rx_buf[MSG_SIZE]; //vetor rx buf
static int rx_buf_pos = 0; //contador de elemento do vetor

/*
 * Read characters from UART until line end is detected. Afterwards push the
 * data to the message queue.
 */
void serial_cb(const struct device *dev, void *user_data) //callback da serial
{// aceita como parametros o ponteiro dev, que contem as informaçoes da device tree sobre a uart
// e o ponteiro user data
	uint8_t c;
	
	/* read until FIFO empty */
	if (k_mutex_lock(&mut1, K_FOREVER) == 0){//pega o mutex 1
	while (uart_fifo_read(uart_dev, &c, 1) == 1) {//enquanto existir informações na uart
		
		if ((c == '\n' || c == '\r') && rx_buf_pos > 0) {//se detectar enter ou mudança de linha
			/* terminate string */
			rx_buf[rx_buf_pos] = '\0'; //volta o vetor do buf para 0

			/* if queue is full, message is silently dropped */
			k_msgq_put(&uart_msgq, &rx_buf, K_NO_WAIT);//sobe o vetor numa fila de mensagens

			/* reset the buffer (it was copied to the msgq) */
			rx_buf_pos = 0;
			k_mutex_unlock(&mut1);//quando o 
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
uint8_t palavra = 0b01010101; //capsula da mensagem
uint8_t sync = 0b00010110;
uint8_t st = 0b00000010;
uint8_t id = 0b00001000;
uint8_t etx = 0b00000011;
void transmissao (struct k_timer *tempo){ //transmissão por bit bang

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
				aux = (palavra >> i) & 0b00000001;//o contador escolhe exatamente um bit da palavra e joga no bit menos significativo
				//e faz uma mascara para colocar esse bit em aux
				gpio_pin_set(stx, 3, aux);
				//printk ("Palavra %d\n",aux);
			}
			else if (i<16){
				aux = (sync >> (i%8)) & 0b00000001;//como a cada 8 bits é precisso rodar de novo o byte, o % serve bem, pois independente do tamanho
				//do contador, ele varia ciclicamente de 0 a 7 
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
				if ((m = (j - ((i-32)/8)) )> 0 ){ // transmissão da mensagem //como aqui eu não sei inicialmente a quantidade de bytes uteis a serem enviados
				//eu uso do contador j que me fala quantos são, assim vou bubtraindo conforme o contador avança
					aux = (vetaux[(i-32)/8] >> (i%8) & 0b00000001); //aqui eu rodo o contador do vetor, pois cada byte é uma posição do vetor
					gpio_pin_set(stx, 3, aux);
					//printk (" dado %d  letra %c\n ",aux , vetaux[(i-32)/8]);
				}
			}
			if (m == 0){
				aux = (etx >> (i%8)) & 0b00000001;
				gpio_pin_set(stx, 3, aux);
				//printk ("ETX %d\n",aux);
				k++;// como eu não sei no inicio do codigo quantos bytes eu vou ter que transmitir não faz sentido usar o contador i aqui
				// por isso criei o contador k que depende de certas condições para começar a ser contado
			}
			i++;
			if (k == 8){
                i = 0;// desfazer
				m = 1;
                j = 0;
				k = 0;  //aqui a mensagem é finalizada e os contadores são restaurados 
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
uint8_t traducao;
int c = 0;
bool g;
int y = 0;

K_MUTEX_DEFINE (mut2);
K_CONDVAR_DEFINE (condvar1);
void recepcao (){
	k_mutex_lock(&mut2, K_FOREVER);
	g = gpio_pin_get(stx, 2);
	reserva = ((reserva | g) << 1);
	//printk("r %d\n",g);
	if (((reserva & 0b110 ) == 0b110) && (c%4 == 2)){
		traducao = ((traducao >> 1) | 1<<7);
		g = 1;
		//printk("g %d\n",g);
		k_condvar_signal(&condvar1);
		k_mutex_unlock(&mut2);
	}
	else if (((reserva & 0b110)  == 0b00) && (c%4 == 2)){
		traducao = (traducao >> 1);
		g = 0;
		//printk("g %d\n",g);
		k_condvar_signal(&condvar1);
		k_mutex_unlock(&mut2);
	}else {
		//c = 0;
		//printk("dado lixo\n");

	}
	
	c++;
	//printk("c %d\n",c);
		if (c == 32 ){
		c = 0;
		y = 0;
	}
}


K_TIMER_DEFINE(tempo2, recepcao, NULL); //define e inicializa meu timer com o respectivo nome e função a ser chamada

void mainrecepcao(){
	gpio_pin_configure(stx, 2, GPIO_INPUT);
	k_timer_start(&tempo2, K_MSEC(100), K_MSEC(100));
}

K_THREAD_DEFINE(recept, STACKSIZE, mainrecepcao , NULL, NULL, NULL,
		3, 0, 0);


//INICIO DA INTERPRETAÇÃO

uint8_t texto [8];
uint8_t suporte;
uint8_t vetor [10];
int cont = 0;
int cont1 = 2;
void interpretacao (){
	k_mutex_lock(&mut2, K_FOREVER);
	//printk("%d = %d\n",traducao, sync);
	for (int s = 7; s>-1;s--){
		printk("%d ",(traducao >> s ) & 1 );
	}
		printk("c %d\n",c);
	if((traducao ^ sync) == 0){
		cont = c ;
		vetor [0] = traducao;
		printk("sync %d\n",cont);

	}
	if(cont == c){
	printk("sim\n");
		if ((traducao ^ st ) == 0 ){
			vetor [1] = traducao;
			printk("stx\n");
			y = 1;
		}
		if (y == 1){
			vetor [cont1] = traducao;
			
		}


	}
	//printk("fui chamado\n");
	k_condvar_wait(&condvar1, &mut2, K_FOREVER);
	k_mutex_unlock(&mut2);

}
K_TIMER_DEFINE(tempo3, interpretacao, NULL);
void maintraducao (){
	k_timer_start(&tempo3, K_MSEC(100), K_MSEC(100));
		
		
		printk("condvar release\n");

}
K_THREAD_DEFINE(trad, STACKSIZE, maintraducao , NULL, NULL, NULL,
		3, 0, 0);