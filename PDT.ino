/* PROJETO DESENVOLVIDO POR:
 *  MARCOS DINÍS LAVARDA
 *  ADEMIR LUIZ DO PRADO
 *  GUSTAVO DA PAZ
 *  
 *  UNIVERSIDADE FEDERAL DO PARANÁ
 *  UNIVERSIDADE TECNOLÓGICA FEDERAL DO PARANÁ
 *  INSTITUTO FEDERAL DO PARANÁ
 *  
 *  PINAGEM DO PHOTODYNAMIC THERAPY
 * 
 * Display LCD I2C: SDA GPIO21
 *                  SCL GPIO22
 * 
 * Potenciômetros: pRed  GPIO15
 *                 pGreen  GPIO2
 *                 pBlue GPIO4
 * 
 * pinoTouchStart 13  	//Botão de Click
 * pinoTouchLess 12  	//-
 * pinoTouchMore 14  	//+
 *  
 * Fita de Led WS1228b: leds GPIO5
 * 
 * Sensor de Lux BH1750FVI:  SDA GPIO21
 *                           SCL GPIO22
 *                           
 * Buzzer: GPIO23                          
 * 
 */

/*INCLUSÃO DE BIBLIOTECAS*/
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

#include <Adafruit_Sensor.h>
#include <Adafruit_TSL2561_U.h>

#include <Adafruit_NeoPixel.h>
#ifdef __AVR__
#include <avr/power.h> // Required for 16 MHz Adafruit Trinket
#endif

LiquidCrystal_I2C lcd(0x27, 16, 2);

void displaySensorDetails();
void configureSensor();

Adafruit_TSL2561_Unified tsl = Adafruit_TSL2561_Unified(TSL2561_ADDR_FLOAT, 12345);

/*DEFINIÇÃO DE PINOS*/

//Pinos Ajustes Intensidade de cada cor de LED
#define pRed 15 //Potenciômetro Vermelho
#define pGreen 2  //Potenciômetro Verde
#define pBlue 4 //Potenciômetro Azul

#define pLeds 5 //Pino dos Leds
#define NUMPIXELS 30 // número de leds
Adafruit_NeoPixel pixels(NUMPIXELS, pLeds, NEO_GRB + NEO_KHZ800);

//Pinos de Ajuste de Menu
#define pinoTouchStart 13  //Botão de Click
#define pinoTouchLess 14  //menos-
#define pinoTouchMore 12  //mais+
 
#define buzzer 23  //buzzer

/*CRIAÇÃO E INICIALIZAÇÃO DE VARIÁVEIS*/
byte ajusteRed = 0; //ajuste da intensidade da cor Vermelha
byte ajusteGreen = 0;  //ajuste da intensidade da cor Verde
byte ajusteBlue = 0;   //ajuste da intensidade da cor Azul
byte operaRed = 0; //intensidade de operação da cor Vermelha
byte operaGreen = 0;  //intensidade de operação da cor Verde
byte operaBlue = 0;   //intensidade de operação da cor Azul


//variáveis do encoder rotativo
int contPosicao = 0; //conta posições
int posicaoAnt; //registro de última posição
int verificaGiro; //armazena leitura do pino CLK de giro
boolean verificaSentido; //verifica o sentido de giro horário/anti-horário do encoder

//variáveis de configuração e operação
unsigned int tempo = 61; //valor inicial do tempo que aparece no display
unsigned int tempo_inicial = 60;
byte menu = 0; //0-menu de configuração | 1-exibe operação | 2-Finalizou processo
int leitura_lux = 0; //leitura do sensor em lux
double leitura_joule = 0; //leitura do sensor em Joule/cm^2
unsigned int tempo_decorrido = 0; //tempo em minutos

bool flagI2C = false; //I2C compartilhado entre sensor lux e LCD

bool flagTempo = LOW; //1 em funcionamento, 0 desligado

//Inicia a fila tarefas
QueueHandle_t fila;
int tamanhodafila = 50;

/*CONFIGURAÇÕES*/

void setup() 
{ 
  //inicia Comunicação Serial
  Serial.begin(115200); 

  //inicia Display LCD
  lcd.begin ();
  lcd.backlight();
  lcd.setCursor(0,0);
  lcd.print("Starting...");
  lcd.setCursor(0,1);

  //Configura Pino de Buzzer
  pinMode(buzzer, OUTPUT);
  digitalWrite(buzzer, HIGH);
  delay(1000);
  digitalWrite(buzzer, LOW);
    
  //Configura Pinos de Ajuste do Led
  pinMode(pRed, INPUT);
  pinMode(pGreen, INPUT);
  pinMode(pBlue, INPUT);

  //Configura e inicializa Leds
  #if defined(__AVR_ATtiny85__) && (F_CPU == 16000000)
    clock_prescale_set(clock_div_1);
  #endif
  pixels.begin();
  pixels.clear(); // desliga os leds
    
  //Configura Sensor de Lux TSL2561
  //use tsl.begin() to default to Wire, 
  //tsl.begin(&Wire2) directs api to use Wire2, etc.
  if(!tsl.begin())
  {
    Serial.print("Erro na detecção do sensor de lux TSL2561, checar o endereçamento");
         for(int i=0;i<6;i++)
        {
          digitalWrite(buzzer,HIGH);
          delay(300);
          digitalWrite(buzzer,LOW);
          delay(300);
        }  
  }  
  displaySensorDetails();
  configureSensor();

   xTaskCreatePinnedToCore
  (
    TarefaLeLux,  /* Tarefa ou Função  */
    "TarefaLeLux",/* Nome da Tarefa ou Função  */
    10000,            /* Tamanho da Pilha  */
    NULL,             /* Parâmetro de Entrada  */
    2,                /* Prioridade da Tarefa  */
    NULL,              /* Identificador da Tarefa  */
    0                 /* Processador da Tarefa - 0 ou 1  */
  ); 

   xTaskCreatePinnedToCore
  (
    TarefaContaTempo,  /* Tarefa ou Função  */
    "TarefaContaTempo",/* Nome da Tarefa ou Função  */
    10000,            /* Tamanho da Pilha  */
    NULL,             /* Parâmetro de Entrada  */
    2,                /* Prioridade da Tarefa  */
    NULL,              /* Identificador da Tarefa  */
    1                 /* Processador da Tarefa - 0 ou 1  */
  ); 
  
  xTaskCreatePinnedToCore
  (
    TarefaLeAjustes,  /* Tarefa ou Função  */
    "TarefaLeAjustes",/* Nome da Tarefa ou Função  */
    10000,            /* Tamanho da Pilha  */
    NULL,             /* Parâmetro de Entrada  */
    2,                /* Prioridade da Tarefa  */
    NULL,              /* Identificador da Tarefa  */
    0                 /* Processador da Tarefa - 0 ou 1  */
  ); 

  xTaskCreatePinnedToCore
  (
    TarefaDisplayLCD,  /* Tarefa ou Função  */
    "TarefaDisplayLCD",/* Nome da Tarefa ou Função  */
    10000,            /* Tamanho da Pilha  */
    NULL,             /* Parâmetro de Entrada  */
    2,                /* Prioridade da Tarefa  */
    NULL,              /* Identificador da Tarefa  */
    1                 /* Processador da Tarefa - 0 ou 1  */
  ); 

  xTaskCreatePinnedToCore
  (
    TarefaAtualizaLeds,  /* Tarefa ou Função  */
    "TarefaAtualizaLeds",/* Nome da Tarefa ou Função  */
    10000,            /* Tamanho da Pilha  */
    NULL,             /* Parâmetro de Entrada  */
    2,                /* Prioridade da Tarefa  */
    NULL,              /* Identificador da Tarefa  */
    1                 /* Processador da Tarefa - 0 ou 1  */
  ); 

  Serial.println("Setup concluded.");

}

void loop() 
{
  delay(5000);
    Serial.println("Running Loop.");
}
 
void TarefaLeAjustes( void * parameter)
{
  while(1)
  { 
    delay(300);
    Serial.println("Task 1 - Read settings.");
    int aux_ajusteRed = analogRead(pRed); //ajuste do Led Vermelho
    int aux_ajusteGreen = analogRead(pGreen);   //ajuste do Led Verde
    int aux_ajusteBlue = analogRead(pBlue);     //ajuste do Led Azul

    ajusteRed = map(aux_ajusteRed, 4095, 0, 0, 255); //ajuste o intervalo para 0-255
    if(ajusteRed<0)ajusteRed=0;if(ajusteRed>255)ajusteRed=255;
    ajusteGreen = map(aux_ajusteGreen, 4095, 0, 0, 255);   //ajuste o intervalo para 0-255
    if(ajusteGreen<0)ajusteGreen=0;if(ajusteGreen>255)ajusteGreen=255;
    ajusteBlue = map(aux_ajusteBlue, 4095, 0, 0, 255);     //ajuste o intervalo para 0-255
    if(ajusteBlue<0)ajusteBlue=0;if(ajusteBlue>255)ajusteBlue=255;
    
      if(touchRead(pinoTouchMore) < 20)
      {
        Serial.print("Time: "); 
        Serial.println(tempo); 
        delay(200);
        if(tempo<1440)tempo++;
     }
      if(touchRead(pinoTouchLess) < 20)
      {
        Serial.print("Time: "); 
        Serial.println(tempo); 
        delay(200);
        if(tempo>1)tempo--;
        
     }
      if(touchRead(pinoTouchStart) < 10)
      {
        delay(300);
        if(touchRead(pinoTouchStart) < 10){
        Serial.print("Start button pressed."); 
        menu = 1;
        digitalWrite(buzzer,HIGH);
        delay(1500);
        digitalWrite(buzzer,LOW);
        delay(500);
        }
     
     }
 
  delay(5);
  }
   
  /* Exclui a tarefa atual */ 
  vTaskDelete( NULL ); 
}

void TarefaDisplayLCD( void * parameter)
{
  while(1)
  {
    delay(500);
    Serial.println("Task 2 - Display LCD");
    while(flagI2C == true){delay(5);} //se I2C está em uso aguarda
    flagI2C = true;

    if(menu==0)//exibe menu configurações
    {
      lcd.begin();
      //lcd.clear();
      //delay(10);
      
      //delay(10);
      lcd.setCursor(0,0);
      lcd.print("R");
      lcd.print(ajusteRed);
      
      //delay(10);
      lcd.setCursor(6,0);
      lcd.print("G");
      lcd.print(ajusteGreen);
      
      //delay(10);
      lcd.setCursor(12,0);
      lcd.print("B");
      lcd.print(ajusteBlue);
      
      
      lcd.setCursor(0,1);
      lcd.print("Time:");
      lcd.print(tempo);
      lcd.print("min");
    }
    else if(menu==1) //exibe tela de operação
    {
      
      lcd.begin();
      //lcd.clear();
      //delay(10);
      
      //delay(10);
      lcd.setCursor(0,0);
      lcd.print(leitura_lux);
      lcd.print(" lx ");
      lcd.print("T:");
      lcd.print(tempo_decorrido);
      lcd.print("'");

      lcd.setCursor(0,1);
      lcd.print(leitura_joule);
      lcd.print(" J/cm2");
      delay(5000);
    }
    else if(menu==2) //exibe processo concluído
    {
      lcd.begin();
      lcd.setCursor(0,0);
      lcd.print("Finished.");

      lcd.setCursor(0,1);
      lcd.print("Tot:");
      double aux = leitura_joule*tempo_inicial*60/1000;
      lcd.print(aux);
      lcd.print("kJ/cm2");

      while(1){delay(1000);}

    }

    flagI2C = false;

  }
  /* Exclui a tarefa atual */ 
  vTaskDelete( NULL ); 
}

void TarefaLeLux( void * parameter)
{
  while(1)
  {
    delay(4000);
    while(flagI2C == true){delay(5);} //se I2C está em uso aguarda
    flagI2C = true;
    
    Serial.println("Task 3 - Read Lux.");
    /* Get a new sensor event */ 
    sensors_event_t event;
    tsl.getEvent(&event);
   
    /* Display the results (light is measured in lux) */
    if (event.light)
    {
      Serial.print(event.light);
      Serial.print(" lux - ");
    }
    else
    {
      /* If event.light = 0 lux the sensor is probably saturated
         and no reliable data could be generated! */
      Serial.println("Sensor overload.");
    }
      
    leitura_lux = event.light; //medição em lux ou lúmens/m^2
    
    leitura_joule = double(leitura_lux)/10000; //agora temos lúmens/cm^2
    leitura_joule = leitura_joule *683; //agora temos Watts/cm^2 ou J/cm^2.s
    Serial.print(leitura_joule);
    Serial.println(" J/cm^2");
    flagI2C = false;
    delay(1000);
    if(menu==2)
    {
      while(1){delay(1000);}
    }
  }
  vTaskDelete(NULL);
}

void TarefaContaTempo( void * parameter)
{
  while(1)
  {
    Serial.println("Task 4 - Time Account");
    if(menu==0) //menu 0 - configura as cores e tempo
    {
      tempo_decorrido=tempo;
      tempo_inicial = tempo;
      delay(500);
    }
    else if (menu==1) //menu 1 - operação de contagem do tempo regressivo
    {
      delay(60000);
      tempo_decorrido--;
      if(tempo_decorrido<=0)
      {tempo_decorrido=0;
        menu = 2; //encerra o processo
        for(int i=0;i<4;i++)
        {
          digitalWrite(buzzer,HIGH);
          delay(1500);
          digitalWrite(buzzer,LOW);
          delay(500);
        }
        while(1){delay(1000);}
      }
    }    
  }
  vTaskDelete(NULL);
}

void TarefaAtualizaLeds( void * parameter)
{
  while(1)
  {
    Serial.println("Task 5 - LEDs");
    if(menu==0) //menu 0 - configura as cores e tempo
    {
       pixels.clear(); //desliga os Leds
       /*for(int i=0; i<NUMPIXELS; i++) {
        pixels.setPixelColor(i, pixels.Color(ajusteRed, ajusteGreen, ajusteBlue));    
        pixels.show();   // atualiza a cor dos leds
        delay(30);*/
        operaRed = ajusteRed;
        operaGreen = ajusteGreen;
        operaBlue = ajusteBlue;
      //}
      delay(300);
    }
    else if(menu == 1) //menu 1 - operação com cor dos leds fixa
    {
       for(int i=0; i<NUMPIXELS; i++)
       {
        pixels.setPixelColor(i, pixels.Color(operaGreen, operaBlue, operaRed));    
        pixels.show();   // atualiza a cor dos leds
        delay(20);
       }
       while(menu==1){delay(1000);}
    }
    else if(menu == 2) //menu 2 - concluiu o processo
    {
       pixels.clear(); //desliga os Leds
       for(int i=0; i<NUMPIXELS; i++)
       {
        pixels.setPixelColor(i, pixels.Color(0, 0, 0));    
        pixels.show();   // atualiza a cor dos leds
        delay(20);
       }
       while(1){delay(1000);}
    }
  }
  vTaskDelete(NULL);
}

//Funções de Configuração de sensor
void displaySensorDetails(void)
{
  sensor_t sensor;
  tsl.getSensor(&sensor);
  Serial.println("------------------------------------");
  Serial.print  ("Sensor:       "); Serial.println(sensor.name);
  Serial.print  ("Driver Ver:   "); Serial.println(sensor.version);
  Serial.print  ("Unique ID:    "); Serial.println(sensor.sensor_id);
  Serial.print  ("Max Value:    "); Serial.print(sensor.max_value); Serial.println(" lux");
  Serial.print  ("Min Value:    "); Serial.print(sensor.min_value); Serial.println(" lux");
  Serial.print  ("Resolution:   "); Serial.print(sensor.resolution); Serial.println(" lux");  
  Serial.println("------------------------------------");
  Serial.println("");
  delay(500);
}

void configureSensor(void)
{
  /* You can also manually set the gain or enable auto-gain support */
  // tsl.setGain(TSL2561_GAIN_1X);      /* No gain ... use in bright light to avoid sensor saturation */
  // tsl.setGain(TSL2561_GAIN_16X);     /* 16x gain ... use in low light to boost sensitivity */
  tsl.enableAutoRange(true);            /* Auto-gain ... switches automatically between 1x and 16x */
  
  /* Changing the integration time gives you better sensor resolution (402ms = 16-bit data) */
  tsl.setIntegrationTime(TSL2561_INTEGRATIONTIME_13MS);      /* fast but low resolution */
  // tsl.setIntegrationTime(TSL2561_INTEGRATIONTIME_101MS);  /* medium resolution and speed   */
  // tsl.setIntegrationTime(TSL2561_INTEGRATIONTIME_402MS);  /* 16-bit data but slowest conversions */

  /* Update these values depending on what you've set above! */  
  Serial.println("------------------------------------");
  Serial.print  ("Gain:         "); Serial.println("Auto");
  Serial.print  ("Timing:       "); Serial.println("13 ms");
  Serial.println("------------------------------------");
}
