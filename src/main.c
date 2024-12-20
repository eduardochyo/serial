
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
#define periodo 10

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
 uint8_t mensagem = 0;
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
			k_mutex_unlock(&mut1);
			printk("Carregando dados\n");
			mensagem++;
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

uint8_t i = 0;//contador de transmissão
uint8_t j = 0; //variavel de contagem de elemntos dentro do vetor
 // variavel auxiliar que faz a contagem de dados enviados por capsula
uint8_t k; // variavel auxiliar para a chamada do etx
uint8_t  d = 0;// variável em que colocamos o id e o número de dados
int vetaux[7];// vetor auxiliar utilizado para chegar ao valor de dados
uint8_t vetmsg [12] = {0b01010101,0b00010110,0b00000010,0b00001000};
int c = 0;// sincronizador da recepção
// 					     palavra     sync        stx        id

uint8_t etx = 0b00000011;
bool transmitindo;//mostra se está transmitindo
bool aux;//suporte de valor
uint8_t permissao = 0;// verificador do progresso da leitura
uint32_t aux3;// recebe o valor aleatória de espera
bool recebendo;// variável de estado que mostra se estamos recebendo algo
void transmissao (struct k_timer *tempo){ //transmissão por bit bang

	char tx_buf[MSG_SIZE];//vetor em que colocamos os valores na fila
	//printk("c %d - i %d\n",c,i);
	if (k_mutex_lock(&mut1, K_FOREVER) == 0 && mensagem > 0){
		
		if(transmitindo ==  0 ){
			k_msgq_get(&uart_msgq, tx_buf, K_NO_WAIT);//faz pop na fila e coloca os valores no tx_buf
			while (tx_buf[j] != '\0'){
				//printk("tx_buf = %c\n",tx_buf[j]);
				vetaux[j] = tx_buf[j];//guarda os valores do tx_buf e vetaux
				j++;
			}
			vetmsg [3] = (vetmsg [3] | j);//id com o número de dados
			//printk("vet  %d\n",j);
		}
		if ((permissao == 0 && recebendo == 0)  || transmitindo == 1){//verifica se nada está sendo recebido e uma vez que a transmissão começou não para até terminar
			transmitindo = 1;
			 // transmissão da parte inicial da capsula
				aux = (vetmsg[i/8] >> (i%8)) & 0b00000001;//o contador escolhe exatamente um bit da palavra e joga no bit menos significativo
				//e faz uma mascara para colocar esse bit em aux

				gpio_pin_set(stx, 3, aux);
				
			if(i >= 32){//mudei aqui
				if (( (j - ((i-32)/8)) )> 0 ){ // transmissão da mensagem //como aqui eu não sei inicialmente a quantidade de bytes uteis a serem enviados
				//eu uso do contador j que me fala quantos são, assim vou bubtraindo conforme o contador avança
					aux = (vetaux[(i-32)/8] >> (i%8) & 0b00000001); //aqui eu rodo o contador do vetor, pois cada byte é uma posição do vetor
					gpio_pin_set(stx, 3, aux);
					vetmsg[i/8] = vetaux[(i-32)/8];
					//printk (" dado %d  letra %c\n ",aux , vetaux[(i-32)/8]);
				}else{
					aux = (etx >> (i%8)) & 0b00000001;
					vetmsg [i/8] = etx;
					gpio_pin_set(stx, 3, aux);
					k++;
					if(k == 8){
						i = 0;
               			j = 0;
						k = 0;  //aqui a mensagem é finalizada e os contadores são restaurados 
						transmitindo = 0;
						mensagem--;
					}
				}
			}
			i++;
		}else {
			
			aux3 = k_uptime_get_32()/100;

			k_msleep(aux3);
		}	
	k_mutex_unlock(&mut1);
	}
}	

K_TIMER_DEFINE(tempo1, transmissao, NULL); //define e inicializa meu timer com o respectivo nome e função a ser chamada

void trans (){
	gpio_pin_configure(stx, 3, GPIO_OUTPUT); 
	k_timer_start(&tempo1, K_MSEC(periodo), K_MSEC(periodo)); // associa um tempo de expiração do meu timer 
}
K_THREAD_DEFINE(transmis, STACKSIZE, trans , NULL, NULL, NULL,
		2, 0, 0);

//INICIO DA RECEPÇÂO

uint32_t reserva;
uint8_t traducao;

bool g;
uint8_t cont4;


K_MUTEX_DEFINE (mut2);
K_CONDVAR_DEFINE (condvar1);
void recepcao (){				
	k_mutex_lock(&mut2, K_FOREVER);
	g = gpio_pin_get(stx, 2);
	reserva = ((reserva | g) << 1);//guardo os valores da recepcao e faço um shift
	//printk("g %d \n",g);
	if (c%5 == 2){
		//printk("\n");
		if ((reserva & 0b110 ) == 0b110){//verifica se a amostra é um 1
			traducao = ((traducao >> 1) | 1<<7);
			g = 1;
			recebendo =1;										  
			//printk("g %d  c %d\n",g,c);
			k_condvar_signal(&condvar1);
			k_mutex_unlock(&mut2);
		}
		else if ((reserva & 0b110)  == 0b00){//verifica se a amostra é um 0
			traducao = (traducao >> 1);
			g = 0;
			recebendo = 1;
			//printk("g %d  c %d\n",g,c);
			k_condvar_signal(&condvar1);
			k_mutex_unlock(&mut2);
		}//guarda os bits na variável traducao
		else{
		//printk("caracter invalido\n");
			cont4++;
			if (cont4 == 4){// verifica a quantidade de caractéries errados estamos recebendo
				recebendo = 0;//caso o número de caracteries errados seja igual a 4 é dado um reset em recebendo
				cont4 = 0;
		}
	}
	}
	//printk("c %d\n",c);
		if (c == 40 ){
		c = 0;
	}
	if (transmitindo == 1 && c%5 == 0){//verifica se está ocorrendo perda de dados
		if (g == aux){
		}else{
			i = 0;
			traducao = 0;
			printk("reinicio\n");
		}
	}
	c++;
}

K_TIMER_DEFINE(tempo2, recepcao, NULL); //define e inicializa meu timer com o respectivo nome e função a ser chamada

void mainrecepcao(){
	gpio_pin_configure(stx, 2, GPIO_INPUT);
	k_timer_start(&tempo2, K_MSEC(periodo/5.0), K_MSEC(periodo/5.0));
}

K_THREAD_DEFINE(recept, STACKSIZE, mainrecepcao , NULL, NULL, NULL,
		3, 0, 0);


//INICIO DA INTERPRETAÇÃO

char vetor [10];//auxiliar que registra o que estamos lendo
int cont = 0;// registra o valor inicial de c em  que ele detecta o sync
int cont1 = 0;//roda as posições do vetor
int aux2;//registra o número de elementos do vetor
//bool fim; 



void interpretacao (){
	k_mutex_lock(&mut2, K_FOREVER);
	//printk("%d \n",traducao);
	k_condvar_wait(&condvar1, &mut2, K_FOREVER);
	if((traducao ^ vetmsg [1]) == 0 && permissao == 0){  //verifica se o recebido é igual ao que deveria ter sido recebido
		cont = c ;				
		vetor [cont1] = traducao;//guardo o que foi traduzido no vetor
		printk("sync %d\n",cont);
		permissao = 1;//muda a variável de estado caso tenha recebido o sync
		cont1++;
		//for (int s = 7; s>-1;s--){
		//		printk("%d ",(traducao >> s ) & 1 );
		//	}
		//	printk("c %d\n",c);
		
	}else if(cont == c){
		
		if (permissao > 0){
			//printk("cont %d c %d\n",cont,c);
			//for (int s = 7; s>-1;s--){
			//	printk("%d ",(traducao >> s ) & 1 );
			//}
		//	printk("c %d\n",c);
			if ((traducao ^ vetmsg [2] ) == 0 && permissao == 1 ){  ////verifica se o recebido é igual ao que deveria ter sido recebido
				vetor [cont1] = traducao;
				permissao = 2; //muda a variável de estado caso tenha recebido o stx
				//printk("stx %d\n",cont1);
				cont1++;
				
			}else if (permissao == 2){
				vetor [cont1] = traducao;
				aux2 = (traducao & 0b111);//extrai o número de bits
				permissao = 3; // indica se o id já foi lido
				//printk("n id %d\n",aux2);
				//printk("id %d\n",aux2);
				cont1++;
			}else if (permissao == 3){
				vetor [cont1] = traducao;
				//printk("cont1 = %d\n",cont1);
				//printk("%c \n",vetor[cont1]);
				cont1++;
				if (cont1 == 3 + aux2){
					permissao = 4;
					//printk("cont1 fim = %d\n",cont1);
				}
			}else if ((traducao ^ etx ) == 0 && permissao == 4) {//verifica se o etx foi recebido
				//printk("fim\n");
				vetor [cont1] = traducao;
				permissao = 0;// é dado um reset nas variáveis
				cont1 = 0;
				for (int s = 0; s < aux2 + 4; s++){
					printk("%c ",vetor [s]);
				}
				printk("\n");
			}else{
				printk("ruim\n");
				permissao = 0;
				cont1 = 0;
				for (int s = 0; s < 10; s++){
					vetor [s] = 0;
				}
			}
		} else if (traducao == 0){
			
		}
	}else if (traducao == 0b11111111){
		recebendo = 0;
	}else if (traducao == 0b0000000){
		recebendo = 0;
	}
	//printk("fui chamado\n");
	
	k_mutex_unlock(&mut2);
}
K_TIMER_DEFINE(tempo3, interpretacao, NULL);
void maintraducao (){
	k_timer_start(&tempo3, K_MSEC(periodo/5.0), K_MSEC(periodo/5.0));
		
}
K_THREAD_DEFINE(trad, STACKSIZE, maintraducao , NULL, NULL, NULL,
		3, 0, 0);