#include <LiquidCrystal.h>
#include <Keypad.h>
#include <EEPROM.h>

// --- PINOS E CONFIGURAÇÕES DO CHIP 74HC595 ---
const int pinoDados = 1;  
const int pinoLatch = 0;  
const int pinoClock = A5; 

// --- LCD ---
LiquidCrystal lcd(7, 6, 5, 4, 3, 2);

// --- TECLADO ---
const byte LINHAS = 4;
const byte COLUNAS = 4;
char teclas[LINHAS][COLUNAS] = {
  {'1', '4', '7', '#'},
  {'2', '5', '8', '0'},
  {'3', '6', '9', '*'},
  {'A', 'B', 'C', 'N'} 
};
char teclasCalc[LINHAS][COLUNAS] = {
  {'1', '4', '7', '#'},
  {'2', '5', '8', '0'},
  {'3', '6', '9', 'X'},
  {'+', '-', '*', '/'} 
};

byte pinosLinhas[LINHAS] = {8, 9, 10, 11}; 
byte pinosColunas[COLUNAS] = {A0, A1, A2, A3}; 
Keypad teclado = Keypad(makeKeymap(teclas), pinosLinhas, pinosColunas, LINHAS, COLUNAS);

// --- ESTRUTURA DE DADOS DA URNA ---
struct Candidato {
  int numero;
  String nome;
  int endereco;
};

Candidato lista[] = {
  {67, "JONATHAN", 0}, {75, "ERICK", 4}, {13, "HESLANDER", 8},
  {69, "HUGO", 12}, {17, "MILA", 16}, {39, "ELLEN", 20},
  {51, "JULIENE", 24}, {0,  "NULOS", 28} 
};
const int totalCandidatos = 8;
String entradaVoto = ""; 
bool votoValido = false;

// --- VARIÁVEIS DO JOGO DINO ---
const int JUMP_DURATION = 700; 
const int ADDR_RECORDE_DINO = 32; 
int dinoPos = 0, cactusPos = 15, score = 0; 
unsigned long jumpStartTime = 0;

// --- CUSTOM CHARS ---
byte dinoChar[8]    = { 0b00000, 0b00111, 0b00101, 0b00111, 0b10110, 0b11111, 0b01110, 0b01010 };
byte cactusChar[8]  = { 0b00100, 0b10100, 0b10100, 0b10101, 0b10101, 0b11111, 0b00100, 0b00100 };
byte coracaoChar[8] = { 0b00000, 0b01010, 0b11111, 0b11111, 0b11111, 0b01110, 0b00100, 0b00000 };

// --- FUNÇÕES DE HARDWARE ---
void atualizarShiftRegister(byte dado) {
  digitalWrite(pinoLatch, LOW);
  shiftOut(pinoDados, pinoClock, MSBFIRST, dado);
  digitalWrite(pinoLatch, HIGH);
}

void bipeManual(int duracaoMs, int atrasoMicros) {
  long tempoFinal = millis() + duracaoMs;
  while(millis() < tempoFinal) {
    atualizarShiftRegister(0x01); delayMicroseconds(atrasoMicros);
    atualizarShiftRegister(0x00); delayMicroseconds(atrasoMicros);
  }
}

void somUrnaFim() {
  for (int i = 0; i < 5; i++) { bipeManual(90, 373); bipeManual(90, 348); }
  bipeManual(200, 373); 
}

// --- ANIMAÇÃO DE INÍCIO ---
void animacaoInicio() {
  lcd.clear();
  lcd.print("Starting System");
  lcd.setCursor(0, 1);
  for (int i = 0; i < 16; i++) {
    lcd.print(".");
    bipeManual(30, 600); 
    delay(100);
  }
  lcd.clear();
  lcd.setCursor(1, 0);
  lcd.print("URNA ELETRONICA");
  delay(1500);
}

// --- FUNÇÕES ESPECIAIS ---
void mostrarCreditos() {
  lcd.clear(); lcd.print("EDCLEY: HW/SW");
  lcd.setCursor(0, 1); lcd.print("BIANCA: DESIGN");
  delay(2500); lcd.clear(); lcd.print("TIAGO: MONTAGEM"); delay(2500);
}

void modoCalculadora() {
  teclado.begin(makeKeymap(teclasCalc));
  lcd.clear(); lcd.print("CALCULADORA"); delay(1000); lcd.clear();
  String n1 = "", n2 = "", op = ""; bool modoN2 = false;
  while (true) {
    char t = teclado.getKey();
    if (t) {
      bipeManual(30, 500);
      if (isDigit(t)) { if (!modoN2) { n1 += t; lcd.print(t); } else { n2 += t; lcd.print(t); } } 
      else if (t == '+' || t == '-' || t == '*' || t == '/') { if (n1 != "") { op = t; modoN2 = true; lcd.print(op); } } 
      else if (t == 'X') { lcd.clear(); n1 = ""; n2 = ""; op = ""; modoN2 = false; } 
      else if (t == '#') {
        if (n1 == "0000" || n2 == "0000") break;
        if (n1 != "" && n2 != "" && op != "") {
          float v1 = n1.toFloat(), v2 = n2.toFloat(), res = 0;
          if (op == "+") res = v1 + v2; else if (op == "-") res = v1 - v2;
          else if (op == "*") res = v1 * v2; else if (op == "/") res = (v2 != 0) ? v1 / v2 : 0;
          lcd.setCursor(0, 1); lcd.print("="); lcd.print(res); delay(3000); lcd.clear(); n1 = ""; n2 = ""; op = ""; modoN2 = false;
        }
      }
    }
  }
  teclado.begin(makeKeymap(teclas));
}

void mostrarRelatorioNavegavel() {
  int indice = 0; bool saindo = false; lcd.clear();
  while (!saindo) {
    lcd.setCursor(0, 0); lcd.print("VER: "); lcd.print(lista[indice].nome);
    lcd.setCursor(0, 1); lcd.print("A^ C v B:OK 0:S");
    char t = teclado.waitForKey(); bipeManual(30, 500);
    if (t == 'A') { indice = (indice <= 0) ? totalCandidatos - 1 : indice - 1; lcd.clear(); } 
    else if (t == 'C') { indice = (indice >= totalCandidatos - 1) ? 0 : indice + 1; lcd.clear(); } 
    else if (t == 'B') {
      long v; EEPROM.get(lista[indice].endereco, v);
      lcd.clear(); lcd.print(lista[indice].nome); lcd.setCursor(0, 1); lcd.print("VOTOS: "); lcd.print(v);
      delay(2500); lcd.clear();
    } 
    else if (t == '0') saindo = true;
  }
}

void jogarDino() {
  lcd.createChar(0, dinoChar); lcd.createChar(1, cactusChar); lcd.createChar(2, coracaoChar);
  bool gameRunning = true; score = 0; int cactoPos = 15; int dPos = 0; unsigned long lMove = 0;
  lcd.clear(); lcd.print("Dino de Edcley"); delay(1000);
  while (gameRunning) {
    unsigned long tAtual = millis(); char tJogo = teclado.getKey();
    if ((tJogo == 'B' || tJogo == '#') && dPos == 0) { dPos = 1; jumpStartTime = tAtual; bipeManual(20, 500); }
    if (tJogo == '0') break;
    if (dPos == 1 && (tAtual - jumpStartTime >= JUMP_DURATION)) dPos = 0;
    if (tAtual - lMove >= 150) { cactoPos--; if (cactoPos < 0) { cactoPos = 15; score++; } lMove = tAtual; }
    if (cactoPos == 0 && dPos == 0) gameRunning = false;
    lcd.clear(); lcd.setCursor(0, 1 - dPos); lcd.write(byte(0)); lcd.setCursor(cactoPos, 1); lcd.write(byte(1));
    lcd.setCursor(10, 0); lcd.write(byte(2)); lcd.print(":"); lcd.print(score); delay(50);
  }
}

// --- SETUP E LOOP ---
void resetarDisplay() { entradaVoto = ""; votoValido = false; lcd.clear(); lcd.print("VOTO: "); }

void setup() {
  pinMode(pinoDados, OUTPUT); pinMode(pinoLatch, OUTPUT); pinMode(pinoClock, OUTPUT);
  lcd.begin(16, 2); 
  animacaoInicio(); // Animação ativada ao ligar
  resetarDisplay();
}

void loop() {
  char tecla = teclado.getKey(); 
  if (tecla) {
    bipeManual(30, 500);
    if (tecla == 'N') {
      entradaVoto = "00"; votoValido = true;
      lcd.clear(); lcd.print("VOTO: NULO"); lcd.setCursor(0, 1); lcd.print("CONFIRMA (#)");
    }
    else if (tecla == '#') {
      if (entradaVoto == "3325") { 
        lcd.clear(); lcd.print("LIMPANDO..."); for (int i = 0 ; i < 40 ; i++) EEPROM.write(i, 0);
        delay(1000); resetarDisplay(); 
      }
      else if (entradaVoto == "3307") { modoCalculadora(); resetarDisplay(); }
      else if (entradaVoto == "2507") { mostrarRelatorioNavegavel(); resetarDisplay(); }
      else if (entradaVoto == "2014") { jogarDino(); resetarDisplay(); } 
      else if (entradaVoto == "2025") { mostrarCreditos(); resetarDisplay(); }
      else if (votoValido && entradaVoto.length() == 2) {
        int addr = -1;
        if (entradaVoto == "00") addr = lista[totalCandidatos-1].endereco;
        else {
          for(int i=0; i<totalCandidatos; i++) { if(lista[i].numero == entradaVoto.toInt()) addr = lista[i].endereco; }
        }
        long total; EEPROM.get(addr, total); total++; EEPROM.put(addr, total);
        lcd.clear(); lcd.print("CONFIRMADO"); somUrnaFim(); delay(2000); resetarDisplay();
      }
    } 
    else if (tecla == '*') {
      if (entradaVoto.length() > 0) {
        entradaVoto.remove(entradaVoto.length() - 1);
        votoValido = false; lcd.clear(); lcd.print("VOTO: "); lcd.print(entradaVoto);
      }
    } 
    else if (isDigit(tecla)) {
      if (entradaVoto.length() < 4) {
        entradaVoto += tecla; lcd.setCursor(6, 0); lcd.print(entradaVoto);
        if (entradaVoto.length() == 2) {
          if (entradaVoto == "00") {
             lcd.setCursor(0, 1); lcd.print("VOTO NULO"); votoValido = true;
          } else {
            bool achou = false;
            for(int i=0; i<totalCandidatos; i++) {
              if(lista[i].numero == entradaVoto.toInt() && lista[i].numero != 0) {
                lcd.setCursor(0, 1); lcd.print(lista[i].nome); votoValido = true; achou = true; break;
              }
            }
            if(!achou && entradaVoto != "33" && entradaVoto != "25" && entradaVoto != "20") {
              lcd.setCursor(0, 1); lcd.print("NUM. INVALIDO"); votoValido = false;
            }
          }
        }
      }
    }
  }
}
