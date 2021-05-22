/*
 * Sprint novo.c
 *
 * Created: 30/04/2021 22:10:39
 * Author : pc
 */ 

#define F_CPU 16000000UL
#define BAUD 9600
#define MYUBRR F_CPU/16/BAUD-1

#include <util/delay.h>
#include <avr/io.h>
#include <stdio.h>
#include <avr/interrupt.h>

#include "nokia5110.c"

uint8_t FreqRespiracao = 5, animacao = 0, flag_intervalo = 0, flag_controle = 0, implementa = 0;
uint8_t selecao = 0, BVM = 0, PEEP = 5, controle_resp = 0, stop_expira = 0, anestesico = 0;
uint32_t tempo_ms = 0;
uint16_t bpm = 0, Temperatura = 0, SpO2 = 0, nivel = 0;

void respiracao(uint8_t *entrada);
void lcd(uint8_t *entrada, uint16_t bat, uint8_t frequencia, uint16_t variacao, uint8_t vol, uint8_t peep);
void USART_Init(unsigned int ubrr);
void USART_Transmit(unsigned char data);
unsigned char USART_Receive(void);

ISR(INT0_vect){  //Captura variações com cada descida do botão
	switch(selecao){
		case 1:
		if(FreqRespiracao<30)
		FreqRespiracao++;
		break;
		
		case 2:
		if(nivel < 100)
		nivel+=10;
		OCR1B = 2000 + (nivel * 20);
		break;
		
		case 3:
		if(BVM < 8)
		BVM ++;
		break;
		
		case 4:
		if(PEEP < 30)
		PEEP ++;
		if(PEEP > 25) //Se a PEEP atinge 26cmH2O
		controle_resp = 1; //Ativa o controle
		else
		controle_resp = 0;
		break;
		
		case 5:
		if(anestesico < 1)   //Se o anestésico estiver desligado
		anestesico++;        //Liga o anestésico ao clicar no botão
		if(anestesico == 1)  //Anestésico ligado
		FreqRespiracao = 12; //Respiração de repouso
		else
		anestesico = 0;
		break;
	}
}

ISR(INT1_vect){  //Captura o decrescimento da frequência de respiração com cada descida do botão e subtrai 1 da saída
	switch(selecao){
		case 1:
		if(FreqRespiracao>5)
		FreqRespiracao--;
		break;
		
		case 2:
		if(nivel > 0)
		nivel-=10;
		OCR1B = 2000 + (nivel * 20);
		break;
		
		case 3:
		if(BVM > 0)
		BVM --;
		break;
		
		case 4:
		if(PEEP > 5)
		PEEP --;
		break;
		
		case 5:
		if(anestesico > 0)
		anestesico--;
		break;
	}
}

ISR(PCINT0_vect){
	static uint8_t press = 0;
	
	if(press){
	if(selecao < 5)
		selecao ++;
	else
		selecao = 0;
	}
	press = !press;
}

ISR(TIMER0_COMPA_vect){ //Interrupção de TC0 a cada 1ms = (64*(249+1))/16MHz
	tempo_ms++;

	if((tempo_ms % (3750/FreqRespiracao)) == 0) //Verdadeiro a cada 1/16 do período de respiração
	animacao = 1; //Animação da barra de leds
	if((tempo_ms % 150) == 0)//Verdadeiro a cada 150ms
	flag_controle = 1;
	if((tempo_ms % 200) == 0) //Verdadeiro a cada 200ms
	flag_intervalo = 1;
	//if((tempo_ms % 500) == 0)//Verdadeiro a cada 500ms
	//stop_expira = 1;
	
}

ISR(PCINT2_vect){ //Interrupção 2 por mudança de pino
	static uint32_t tempoAntes = 0;
	bpm = 60000/((tempo_ms - tempoAntes)*2); // Batimentos por minuto
	tempoAntes = tempo_ms;
}

ISR(ADC_vect){
	static uint8_t afericao = 0;
	
	if(flag_controle){
		switch(afericao){
			case 0: 
			Temperatura = (ADC*(5.0/1023)*(45/3.5));
			ADMUX = 0b01000001;  // Fonte de tensão Vcc em canal ADC1  SpO2 (caso 1)
			break;
			
			case 1:
			SpO2 = ((125.0/1023)*ADC);
			ADMUX = 0b01000000; // Fonte de tensão Vcc em canal ADC0 Temperatura (caso 0)
			break;
			
		}
		
		if(afericao < 1)
		afericao++;
		else
		afericao = 0;
		
		if((Temperatura < 34)||(Temperatura > 41)||(SpO2 < 60)) //Configurações do Buzzer (por algum motivo está com ruído)
		PORTD |= 0b10000000;
		else
		PORTD &= 0b01111111;
		
	}
	flag_controle = 0;

}

char dado[9];
int contador=0, erro=0;

ISR(USART_RX_vect){
	char recebido;
	
	recebido = UDR0;
	
	dado[contador] = recebido; // Vai armazenando cada entrada
	contador++;
	
		if(contador==9){ // Entra ao receber todos os caracteres esperados
			for(contador=0;contador<9;contador++){
				if((dado[0]==';') && (dado[4]=='x') && (dado[8]==':')){
					erro = 1; // Variável de controle para acerto
					for(contador=1;contador<8;contador++)
					USART_Transmit(dado[contador]);
				}
				else{
					erro = 2; // Variável de controle para erro
				}
				}
			contador = 0;
		}

}

void USART_Init(unsigned int ubrr)
{
	UBRR0H = (unsigned char)(ubrr>>8); //Ajusta a taxa de transmissão
	UBRR0L = (unsigned char)ubrr;
	UCSR0B = (1<<RXCIE0)|(1<<RXEN0)|(1<<TXEN0);
	UCSR0C = (0<<USBS0)|(3<<UCSZ00); //Ajusta o formato do frame: 8 bits de dados e 1 de parada
}

void USART_Transmit(unsigned char data)
{
	while(!(UCSR0A & (1<<UDRE0)));//Espera a limpeza do registr. de transmissão
	UDR0 = data; //Coloca o dado no registrador e o envia
}

unsigned char USART_Receive(void)
{
	while(!(UCSR0A & (1<<RXC0))); //Espera o dado ser recebido
	return UDR0; //Lê o dado recebido e retorna
}

void respiracao(uint8_t *entrada){  //Animação do servo considerando o tempo completo de uma respiração
	static uint8_t sobe = 1;
	
	if(*entrada){
		if(sobe){
			if(OCR1A == (2000 + (250*BVM))){ //Nível inferior do servo controlado pelo ângulo máximo de rotação
				sobe = 0;

				if((controle_resp)&&(nivel<60)){//Se a PEEP for igual a 26cmH2O e o volume de abertura for menor ou igual a 50%
					//if(stop_expira==1){ //Trava a expiração por 500ms
						_delay_ms(500);
						OCR1A = (2000 + (250*BVM)) - 250; //Inicia a expiração
						//stop_expira = 0;
					//}
					//controle_resp = 0;
				}
				else{
					OCR1A = (2000 + (250*BVM)) - 250;
				}
			}
			else{
				OCR1A += 250;
			}
			
		}
		else{
			if(OCR1A == 2000){ //Nível superior do servo
				sobe = 1;
				OCR1A = 2250;
			}
			else{
				OCR1A -= 250;
				}
		}
		*entrada = 0;
	}

	
}

void lcd(uint8_t *entrada, uint16_t bat, uint8_t frequencia, uint16_t variacao, uint8_t vol, uint8_t peep){
	char val[4], pressao[8], valor[4], valor2[4], volume[4], pressao_exp[4];
	char volt_SpO2[4], volt_Temp[4];
	int a=0;
	
	if(*entrada){
		switch(selecao){
		case 0: // sinais vitais
		sprintf(val, "%u", bat);
		sprintf(volt_Temp, "%u", Temperatura);
		sprintf(volt_SpO2, "%u", SpO2);
		
		nokia_lcd_clear();
		
		nokia_lcd_set_cursor(0,0);
		nokia_lcd_write_string("Sinais Vitais",1);
		
		nokia_lcd_set_cursor(0,10);
		nokia_lcd_write_string(val, 1);
		nokia_lcd_set_cursor(40,10);
		nokia_lcd_write_string("bpm",1);
		
		nokia_lcd_set_cursor(0,20);
		nokia_lcd_write_string(volt_Temp, 1);
		nokia_lcd_set_cursor(40,20);
		nokia_lcd_write_string("ºC",1);
		
		nokia_lcd_set_cursor(0,30);
		nokia_lcd_write_string(volt_SpO2, 1);
		nokia_lcd_set_cursor(40,30);
		nokia_lcd_write_string("%SpO2",1);
		
		if(erro==1){ // Se tudo estiver certo
			for(a=0;a<7;a++)
			pressao[a]=dado[a+1];
			nokia_lcd_set_cursor(0,40);
			nokia_lcd_write_string(pressao, 1);
		}
		if(erro==2){ // Se alguma coisa estiver errada
			nokia_lcd_set_cursor(0,40);
			nokia_lcd_write_string("Erro!", 1);
		}
		
		nokia_lcd_set_cursor(50,40);
		nokia_lcd_write_string("mmHg",1);
		break;
		
		case 1: //Altera BVM
		sprintf(valor, "%u", frequencia);
		sprintf(valor2, "%u", variacao);
		sprintf(volume, "%u", vol);
		sprintf(pressao_exp, "%u", peep);
		
		nokia_lcd_clear();
		
		nokia_lcd_set_cursor(0,0);
		nokia_lcd_write_string("Parametros",1);
		
		nokia_lcd_set_cursor(0,10);
		nokia_lcd_write_string(valor, 1);
		nokia_lcd_set_cursor(20,10);
		nokia_lcd_write_string("->Resp/min",1);
		
		nokia_lcd_set_cursor(0,20);
		nokia_lcd_write_string(valor2, 1);
		nokia_lcd_set_cursor(20,20);
		nokia_lcd_write_string("  % de O2",1);
		
		nokia_lcd_set_cursor(0,30);
		nokia_lcd_write_string(volume, 1);
		nokia_lcd_set_cursor(20,30);
		nokia_lcd_write_string("   vol", 1);
		
		nokia_lcd_set_cursor(0,40);
		nokia_lcd_write_string(pressao_exp, 1);
		nokia_lcd_set_cursor(20,40);
		nokia_lcd_write_string("   cmH2O", 1);
		break;
		
		case 2: //Altera nível O2
		sprintf(valor, "%u", frequencia);
		sprintf(valor2, "%u", variacao);
		sprintf(volume, "%u", vol);
		sprintf(pressao_exp, "%u", peep);
		
		nokia_lcd_clear();
		
		nokia_lcd_set_cursor(0,0);
		nokia_lcd_write_string("Parametros",1);
		
		nokia_lcd_set_cursor(0,10);
		nokia_lcd_write_string(valor, 1);
		nokia_lcd_set_cursor(20,10);
		nokia_lcd_write_string("  Resp/min",1);
		
		nokia_lcd_set_cursor(0,20);
		nokia_lcd_write_string(valor2, 1);
		nokia_lcd_set_cursor(20,20);
		nokia_lcd_write_string("->% de O2",1);
		
		nokia_lcd_set_cursor(0,30);
		nokia_lcd_write_string(volume, 1);
		nokia_lcd_set_cursor(20,30);
		nokia_lcd_write_string("   vol", 1);
		
		nokia_lcd_set_cursor(0,40);
		nokia_lcd_write_string(pressao_exp, 1);
		nokia_lcd_set_cursor(20,40);
		nokia_lcd_write_string("   cmH2O", 1);
		break;
		
		case 3: //Altera o nível de rotação do BVM
		sprintf(valor, "%u", frequencia);
		sprintf(valor2, "%u", variacao);
		sprintf(volume, "%u", vol);
		sprintf(pressao_exp, "%u", peep);
		
		nokia_lcd_clear();
		
		nokia_lcd_set_cursor(0,0);
		nokia_lcd_write_string("Parametros",1);
		
		nokia_lcd_set_cursor(0,10);
		nokia_lcd_write_string(valor, 1);
		nokia_lcd_set_cursor(20,10);
		nokia_lcd_write_string("  Resp/min",1);
		
		nokia_lcd_set_cursor(0,20);
		nokia_lcd_write_string(valor2, 1);
		nokia_lcd_set_cursor(20,20);
		nokia_lcd_write_string("  % de O2",1);
		
		nokia_lcd_set_cursor(0,30);
		nokia_lcd_write_string(volume, 1);
		nokia_lcd_set_cursor(20,30);
		nokia_lcd_write_string("-> vol", 1);
		
		nokia_lcd_set_cursor(0,40);
		nokia_lcd_write_string(pressao_exp, 1);
		nokia_lcd_set_cursor(20,40);
		nokia_lcd_write_string("   cmH2O", 1);
		break;
		
		case 4: //Altera o PEEP
		sprintf(valor, "%u", frequencia);
		sprintf(valor2, "%u", variacao);
		sprintf(volume, "%u", vol);
		sprintf(pressao_exp, "%u", peep);
		
		nokia_lcd_clear();
		
		nokia_lcd_set_cursor(0,0);
		nokia_lcd_write_string("Parametros",1);
		
		nokia_lcd_set_cursor(0,10);
		nokia_lcd_write_string(valor, 1);
		nokia_lcd_set_cursor(20,10);
		nokia_lcd_write_string("  Resp/min",1);
		
		nokia_lcd_set_cursor(0,20);
		nokia_lcd_write_string(valor2, 1);
		nokia_lcd_set_cursor(20,20);
		nokia_lcd_write_string("  % de O2",1);
		
		nokia_lcd_set_cursor(0,30);
		nokia_lcd_write_string(volume, 1);
		nokia_lcd_set_cursor(20,30);
		nokia_lcd_write_string("   vol", 1);
		
		nokia_lcd_set_cursor(0,40);
		nokia_lcd_write_string(pressao_exp, 1);
		nokia_lcd_set_cursor(20,40);
		nokia_lcd_write_string("-> cmH2O", 1);
		break;
		
		case 5://Gás anestésico
		nokia_lcd_clear();
		
		nokia_lcd_set_cursor(0,0);
		nokia_lcd_write_string("Gas Anestesico", 1);
		
		if(anestesico == 1){
			nokia_lcd_set_cursor(0,10);
			nokia_lcd_write_string("Ligado", 1);
			nokia_lcd_set_cursor(0,30);
			nokia_lcd_write_string("Pressione (-) para desligar", 1);
		}
		else{
			nokia_lcd_set_cursor(0,10);
			nokia_lcd_write_string("Desligado", 1);
			nokia_lcd_set_cursor(0,30);
			nokia_lcd_write_string("Pressione (+) para ligar", 1);
		}
		break;
		
		}
		
		nokia_lcd_render();
	
		*entrada = 0;
	}
}

int main(void)
{
	//Configurações de Pino
	DDRB   = 0b10111111; //Habilita B6 como entrada
	DDRC   = 0b11111100; //Habilita C0 e C1 como entradas
	DDRD   = 0b11110000; //Habilita os pinos D0 a D3 como entradas
	PORTD  = 0b10001100; //Habilita o resistor de pull-up dos pinos D2, D3 e D7
	PORTB  = 0b01000000; //Habilita o resistor de pull-up do pino B6
	
	//Configurações de ADC
	ADMUX  = 0b01000000; // Fonte de tensão Vcc em canal ADC0  Temperatura
	ADCSRA = 0b11101111; // Habilita o AD, conversão contínua, prescaler = 128
	ADCSRB = 0b00000000; // Conversão contínua
	DIDR0  = 0b00000011; // Desabilita como entrada digital
	
	//Configurações interrupções externas
	EICRA   = 0b00001010; //Interrupções INT0 e INT1 na borda de descida
	EIMSK   = 0b00000011; //Habilita as interrupções INT0 e INT1
	PCICR   = 0b00000101; // Habilita as interrupções nos pinos PORTD e PORTB
	PCMSK2  = 0b00010000; //Ativa a interrupção do pino D4
	PCMSK0  = 0b01000000; //Ativa a interrupção do pino B6 
	
	//Configurações timer 0
	TCCR0A  = 0b00000010; //Habilita modo TCT do TC0
	TCCR0B  = 0b00000011; //Liga TC0 com prescaler = 64
	OCR0A   = 249;		  //Ajusta o comparador para o TC0 contar até 249
	TIMSK0  = 0b00000010; //Ativa a interrupção de TC0 na igualdade da comparação com OCR0A
	
	//Configurações Timer 1
	ICR1 = 39999;        //Configura período 20ms
	TCCR1A = 0b10100010; //PWM rápido, modo não invertido para OC1A e OC1B
	TCCR1B = 0b00011010; //prescaler = 8

	sei();
	
	//Configurações do LCD
	nokia_lcd_init();
	nokia_lcd_clear();
	nokia_lcd_render();
	
	//Iniciar o USART
	USART_Init(MYUBRR);

	while (1){
			
		lcd(&flag_intervalo, bpm, FreqRespiracao, nivel, BVM, PEEP);
		respiracao(&animacao);
		
		if(anestesico==1)
		PORTD |= 0b01000000;
		else
		PORTD &= 0b10111111;
		
	}
}
