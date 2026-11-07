// --- INCLUSÃO DE BIBLIOTECAS ---
#define CUSTOM_SETTINGS
#define INCLUDE_GAMEPAD_MODULE
#define MAX_PWM 700 // Reduzido para futebol de robôs
#include <DabbleESP32.h>

// --- MAPEAMENTO DE PINOS MOTORES ---
const int motorA_IN1 = 23;
const int motorA_IN2 = 22;
const int motorB_IN1 = 19;
const int motorB_IN2 = 21;

// --- PINOS ADC ---
#define ADC_B1_PIN 35    // Célula 1 (Divisor 100k/300k)
#define ADC_TOTAL_PIN 32 // Tensão Total (Divisor 200k/100k)
#define LED_BUILTIN 2    

// --- CONFIGURAÇÕES DE PROTEÇÃO ---
#define LIMITE_TENSAO_BAIXA 3.0     
#define LIMITE_DESBALANCEAMENTO 0.5 
#define BLINK_LENTO 1000  
#define BLINK_RAPIDO 200  

// --- FATORES DE CALIBRAÇÃO ---
const float FATOR_B1 = 1.279;
const float FATOR_TOTAL = 3.119;
const float VREF = 3.3;

// --- CONFIGURAÇÕES DE PWM (LEDC) ---
const int freq = 20000;
const int canalPWM_A1 = 0;
const int canalPWM_A2 = 1;
const int canalPWM_B1 = 2;
const int canalPWM_B2 = 3;
const int resolucao = 10;

const int MIN_PWM = 600; 
const int TURN_PWM = 600;

// --- Variáveis Globais ---
int pDireita = 100;
int pEsquerda = 100;
int botao_pressionado = 0;

// --- Variáveis para o Filtro de Tensão ---
float filtro_V_Cell1 = 0.0;
float filtro_V_Total = 0.0;
const float ALPHA = 0.005;

// --- FUNÇÕES DE SEGURANÇA ---
void pararMotoresImediatamente() {
  ledcWrite(canalPWM_A1, 0);
  ledcWrite(canalPWM_A2, 0);
  ledcWrite(canalPWM_B1, 0);
  ledcWrite(canalPWM_B2, 0);
}

void alertaTensaoBaixa() {
  /*
  pararMotoresImediatamente(); 
  Serial.println("!!! ALERTA: Tensão Baixa - Sistema Travado !!!");
  while(true) {
    digitalWrite(LED_BUILTIN, HIGH);
    delay(BLINK_LENTO);
    digitalWrite(LED_BUILTIN, LOW);
    delay(BLINK_LENTO);
  }
  */
}

void alertaDesbalanceamento() {
  /*
  pararMotoresImediatamente(); 
  Serial.println("!!! ALERTA: Desbalanceamento - Sistema Travado !!!");
  while(true) {
    digitalWrite(LED_BUILTIN, HIGH);
    delay(BLINK_RAPIDO);
    digitalWrite(LED_BUILTIN, LOW);
    delay(BLINK_RAPIDO);
  }
  */
}

// --- FUNÇÕES DE MOVIMENTO ---

// Função única e direta para os motores (sem rampa)
void acionarMotores(int a1, int a2, int b1, int b2) {
  ledcWrite(canalPWM_A1, a1);
  ledcWrite(canalPWM_A2, a2);
  ledcWrite(canalPWM_B1, b1);
  ledcWrite(canalPWM_B2, b2);
}

void parar()   { acionarMotores(0, 0, 0, 0); }
void frente()  { acionarMotores(MAX_PWM, 0, MAX_PWM, 0); }
void tras()    { acionarMotores(0, MAX_PWM, 0, MAX_PWM); }
void esquerda(){ acionarMotores(0, TURN_PWM, TURN_PWM, 0); } 
void direita() { acionarMotores(TURN_PWM, 0, 0, TURN_PWM); } 

// --- SETUP ---
void setup() {
  Serial.begin(115200);
  Dabble.begin("Carrinho");

  pinMode(motorA_IN1, OUTPUT);
  pinMode(motorA_IN2, OUTPUT);
  pinMode(motorB_IN1, OUTPUT); 
  pinMode(motorB_IN2, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);

  analogSetAttenuation(ADC_11db);
  analogSetWidth(12);

  ledcSetup(canalPWM_A1, freq, resolucao);
  ledcSetup(canalPWM_A2, freq, resolucao);
  ledcSetup(canalPWM_B1, freq, resolucao);
  ledcSetup(canalPWM_B2, freq, resolucao);

  ledcAttachPin(motorA_IN1, canalPWM_A1);
  ledcAttachPin(motorA_IN2, canalPWM_A2);
  ledcAttachPin(motorB_IN1, canalPWM_B1);
  ledcAttachPin(motorB_IN2, canalPWM_B2);

  // Inicializa o filtro com uma primeira leitura
  int raw_adc1 = analogRead(ADC_B1_PIN);
  int raw_adc_total = analogRead(ADC_TOTAL_PIN); 
  filtro_V_Cell1 = (raw_adc1 * (VREF / 4095.0)) * FATOR_B1;
  filtro_V_Total = (raw_adc_total * (VREF / 4095.0)) * FATOR_TOTAL;

  pararMotoresImediatamente();
  Serial.println("Sistema Iniciado!");
}

// --- LOOP ---
void loop() {
  // --- LEITURA COM FILTRO (Média Exponencial) ---
  int raw_adc1 = analogRead(ADC_B1_PIN);
  int raw_adc_total = analogRead(ADC_TOTAL_PIN); 

  float v_inst_c1 = (raw_adc1 * (VREF / 4095.0)) * FATOR_B1;
  float v_inst_total = (raw_adc_total * (VREF / 4095.0)) * FATOR_TOTAL;

  filtro_V_Cell1 = (v_inst_c1 * ALPHA) + (filtro_V_Cell1 * (1.0 - ALPHA));
  filtro_V_Total = (v_inst_total * ALPHA) + (filtro_V_Total * (1.0 - ALPHA));
  
  float V_Cell2 = filtro_V_Total - filtro_V_Cell1;

  // --- PROTEÇÃO ---
  if (filtro_V_Total > 1.0) { 
    if (filtro_V_Cell1 < LIMITE_TENSAO_BAIXA || V_Cell2 < LIMITE_TENSAO_BAIXA) {
      alertaTensaoBaixa();
    }
    if (abs(filtro_V_Cell1 - V_Cell2) > LIMITE_DESBALANCEAMENTO) {
      alertaDesbalanceamento();
    }
  }

  digitalWrite(LED_BUILTIN, LOW);

  // --- CONTROLE DO ROBÔ ---
  Dabble.processInput();
  
  int angle = GamePad.getAngle();
  int radius = GamePad.getRadius();
  float Xaxis = map(GamePad.getXaxisData(), -7, 7, 0, 1023);

  if(GamePad.isSquarePressed() || GamePad.isCirclePressed() || GamePad.isCrossPressed() || GamePad.isTrianglePressed()){
    botao_pressionado = 1;
  } else {
    botao_pressionado = 0;
  }

  if (Xaxis < 511) { 
    pDireita = map(Xaxis, 511, 0, 100, 0);
    pEsquerda = 100;
  } else { 
    pDireita = 100;
    pEsquerda = map(Xaxis, 512, 1023, 100, 0);
  }

  // Lógica de Movimento via Joystick e Botões
  if ((angle < 345 && angle > 195) && botao_pressionado == 0) {

    int velocidade = map(radius, 0, 7, 0, MAX_PWM);
    int velDir = (velocidade * pDireita / 100);
    int velEsq = (velocidade * pEsquerda / 100);
    if (velDir > 0) velDir = max(velDir, MIN_PWM);
    if (velEsq > 0) velEsq = max(velEsq, MIN_PWM);
    acionarMotores(0, velDir, 0, velEsq);

  } else if ((angle < 165 && angle > 15) && botao_pressionado == 0) {

    int velocidade = map(radius, 0, 7, 0, MAX_PWM);
    int velDir = (velocidade * pDireita / 100);
    int velEsq = (velocidade * pEsquerda / 100);
    if (velDir > 0) velDir = max(velDir, MIN_PWM);
    if (velEsq > 0) velEsq = max(velEsq, MIN_PWM);
    acionarMotores(velDir, 0, velEsq, 0);

  } else if ((angle <= 195 && angle >= 165) && botao_pressionado == 0) {

    int velEsq = map(radius, 0, 7, 0, MAX_PWM);
    if (velEsq > 0) velEsq = max(velEsq, MIN_PWM);
    acionarMotores(0, velEsq, velEsq, 0);

  } else if ((angle <= 15 && angle >= 0 || angle < 360 && angle >= 345) && botao_pressionado == 0) {

    int velDir = map(radius, 0, 7, 0, MAX_PWM);
    if (velDir > 0) velDir = max(velDir, MIN_PWM);
    acionarMotores(velDir, 0, 0, velDir);

  } else if (GamePad.isUpPressed()) {
    frente();
  } else if (GamePad.isDownPressed()) {
    tras();
  } else if (GamePad.isLeftPressed()) {
    esquerda(); 
  } else if (GamePad.isRightPressed()) {
    direita();
  } else if (GamePad.isSquarePressed()) {
    esquerda(); 
  } else if (GamePad.isCirclePressed()) {
    direita();
  } else if (GamePad.isCrossPressed()) {
    tras();
  } else if (GamePad.isTrianglePressed()) {
    frente();
  } else {
    parar();
  }
}