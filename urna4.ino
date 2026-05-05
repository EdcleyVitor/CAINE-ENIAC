#include <LiquidCrystal.h>
#include <Keypad.h>
#include <EEPROM.h>

// --- BUZZER ---
const int pinoBuzzer = 12;

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
byte pinosLinhas[LINHAS]   = {8, 9, 10, 11};
byte pinosColunas[COLUNAS] = {A0, A1, A2, A3};
Keypad teclado = Keypad(makeKeymap(teclas), pinosLinhas, pinosColunas, LINHAS, COLUNAS);

// --- CANDIDATOS ---
struct Candidato { int numero; String nome; int endereco; };
Candidato lista[] = {
  {67, "JONATHAN", 0}, {75, "ERICK", 4}, {13, "HESLANDER", 8},
  {69, "HUGO", 12},    {17, "MILA", 16}, {39, "ELLEN", 20},
  {51, "JULIENE", 24}, {0,  "NULOS", 28}
};
const int totalCandidatos = 8;
String entradaVoto = "";
bool   votoValido  = false;

// --- MODO ---
// false (padrao ao ligar) = BASICO:  sem cadastro, qualquer numero aceito, buzzer fisico
// true  (ativado por 3030) = AVANCADO: cadastro serial/celular, som vai pro celular
// Volta ao basico apenas reiniciando o Arduino
bool modoAvancado = false;

// --- JOGO DINO ---
const int JUMP_DURATION     = 700;
const int ADDR_RECORDE_DINO = 32;
int  dinoPos = 0, cactusPos = 15, score = 0;
unsigned long jumpStartTime = 0;

// --- CUSTOM CHARS ---
byte dinoChar[8]    = {0b00000,0b00111,0b00101,0b00111,0b10110,0b11111,0b01110,0b01010};
byte cactusChar[8]  = {0b00100,0b10100,0b10100,0b10101,0b10101,0b11111,0b00100,0b00100};
byte coracaoChar[8] = {0b00000,0b01010,0b11111,0b11111,0b11111,0b01110,0b00100,0b00000};

// ============================================================
//  SISTEMA DE ELEITORES (usado apenas no modo avancado)
//  EEPROM layout:
//   0  - 31 : contagem de votos dos candidatos
//   32 - 35 : recorde do Dino
//   40 - 41 : total de eleitores (int, 2 bytes)
//   50+     : dados dos eleitores (11 bytes cada)
// ============================================================
#define ADDR_TOTAL_ELEITORES  40
#define ADDR_BASE_ELEITORES   50
#define NOME_LEN              10
#define BYTES_POR_ELEITOR     11
#define MAX_ELEITORES         88
#define STATUS_REGISTRADO      0
#define STATUS_VOTOU           1

enum EstadoUrna { LIBERADA, AGUARDANDO_VOTO };
EstadoUrna    estadoUrna           = LIBERADA;
String        nomeEleitorAtual     = "";
int           sequenciaAtual       = 0;
unsigned long tempoEstado          = 0;
bool          alertaMesarioEnviado = false;

// Serial
String        serialBuffer       = "";
unsigned long ultimoStatusSerial = 0;
#define INTERVALO_STATUS 5000UL

// ============================================================
//  HARDWARE
// ============================================================
void bipeManual(int duracaoMs, int atrasoMicros) {
  // atrasoMicros em half-period -> frequencia = 1000000 / (2 * atrasoMicros)
  int freq = 1000000 / (2 * atrasoMicros);
  tone(pinoBuzzer, freq, duracaoMs);
  delay(duracaoMs);
  noTone(pinoBuzzer);
}

void somUrnaFim() {
  for (int i = 0; i < 5; i++) { bipeManual(90, 373); bipeManual(90, 348); }
  bipeManual(200, 373); 
}

// Som via celular/serial (modo avancado)
void tocarSomNoCelular() { Serial.println("SOM_URNA"); }

// ============================================================
//  EEPROM - ELEITORES
// ============================================================
int lerTotalEleitores() {
  int t; EEPROM.get(ADDR_TOTAL_ELEITORES, t);
  if (t < 0 || t > MAX_ELEITORES) return 0;
  return t;
}

void salvarEleitor(int seq, String nome, byte status) {
  int addr = ADDR_BASE_ELEITORES + (seq - 1) * BYTES_POR_ELEITOR;
  for (int i = 0; i < NOME_LEN; i++)
    EEPROM.write(addr + i, (i < (int)nome.length()) ? (byte)nome[i] : ' ');
  EEPROM.write(addr + NOME_LEN, status);
}

String lerNomeEleitor(int seq) {
  int addr = ADDR_BASE_ELEITORES + (seq - 1) * BYTES_POR_ELEITOR;
  String nome = "";
  for (int i = 0; i < NOME_LEN; i++) {
    char c = (char)EEPROM.read(addr + i);
    if (c == ' ' || c == (char)0xFF) break;
    nome += c;
  }
  return nome;
}

byte lerStatusEleitor(int seq) {
  return EEPROM.read(ADDR_BASE_ELEITORES + (seq - 1) * BYTES_POR_ELEITOR + NOME_LEN);
}

void marcarEleitorVotou(int seq) {
  EEPROM.write(ADDR_BASE_ELEITORES + (seq - 1) * BYTES_POR_ELEITOR + NOME_LEN, STATUS_VOTOU);
}

// ============================================================
//  DISPLAY
// ============================================================
void atualizarDisplay() {
  lcd.clear();
  if (!modoAvancado) {
    // Modo basico: sempre mostra VOTO:
    lcd.print("VOTO: ");
    lcd.print(entradaVoto);
  } else {
    // Modo avancado
    if (estadoUrna == LIBERADA) {
      lcd.print("URNA LIBERADA");
      lcd.setCursor(0, 1);
      lcd.print("PROX SEQ: ");
      lcd.print(lerTotalEleitores() + 1);
    } else {
      lcd.print("VOTO: ");
      lcd.print(entradaVoto);
      lcd.setCursor(0, 1);
      lcd.print(nomeEleitorAtual.substring(0, 16));
    }
  }
}

void resetarDisplay() {
  entradaVoto = ""; votoValido = false;
  atualizarDisplay();
}

// ============================================================
//  SERIAL - STATUS E LISTA (so no modo avancado)
// ============================================================
void enviarStatus() {
  if (!modoAvancado) return;
  if (estadoUrna == LIBERADA) {
    Serial.print("STATUS:LIBERADA|SEQ_PROX:");
    Serial.println(lerTotalEleitores() + 1);
  } else {
    unsigned long s = (millis() - tempoEstado) / 1000;
    Serial.print("STATUS:AGUARDANDO|NOME:");
    Serial.print(nomeEleitorAtual);
    Serial.print("|SEQ:");
    Serial.print(sequenciaAtual);
    Serial.print("|TEMPO:");
    Serial.print(s);
    Serial.println("s");
  }
}

void listarEleitoresSerial() {
  int total = lerTotalEleitores();
  Serial.println("=== LISTA DE ELEITORES ===");
  if (total == 0) {
    Serial.println("NENHUM ELEITOR REGISTRADO");
  } else {
    for (int i = 1; i <= total; i++) {
      Serial.print("SEQ:");    Serial.print(i);
      Serial.print("|NOME:");  Serial.print(lerNomeEleitor(i));
      Serial.print("|STATUS:");
      Serial.println(lerStatusEleitor(i) == STATUS_VOTOU ? "VOTOU" : "PENDENTE");
    }
  }
  Serial.println("=== FIM DA LISTA ===");
}

// ============================================================
//  LCD - LISTA NAVEGAVEL
// ============================================================
void mostrarListaEleitoresLCD() {
  int total = lerTotalEleitores();
  if (total == 0) { lcd.clear(); lcd.print("SEM ELEITORES"); delay(2000); return; }
  int  idx    = 1;
  bool saindo = false;
  while (!saindo) {
    String nome   = lerNomeEleitor(idx);
    byte   status = lerStatusEleitor(idx);
    lcd.clear();
    String l0 = String(idx) + ":" + nome;
    lcd.print(l0.substring(0, 16));
    lcd.setCursor(0, 1);
    lcd.print(status == STATUS_VOTOU ? "VOTOU" : "PENDT");
    lcd.print(" A^Cv 0:S");
    char t = teclado.waitForKey(); bipeManual(30, 500);
    if      (t == 'A') idx = (idx <= 1)     ? total : idx - 1;
    else if (t == 'C') idx = (idx >= total) ? 1     : idx + 1;
    else if (t == '0') saindo = true;
  }
}

// ============================================================
//  ANIMACAO E CREDITOS
// ============================================================
void animacaoInicio() {
  lcd.clear(); lcd.print("Starting System");
  lcd.setCursor(0, 1);
  for (int i = 0; i < 16; i++) { lcd.print("."); bipeManual(30, 600); delay(100); }
  lcd.clear(); lcd.setCursor(1, 0); lcd.print("URNA ELETRONICA"); delay(1500);
}

void mostrarCreditos() {
  lcd.clear(); lcd.print("EDCLEY: HW/SW");
  lcd.setCursor(0, 1); lcd.print("BIANCA: DESIGN");
  delay(2500); lcd.clear(); lcd.print("TIAGO: MONTAGEM"); delay(2500);
}

// ============================================================
//  MODOS ESPECIAIS
// ============================================================
void modoCalculadora() {
  teclado.begin(makeKeymap(teclasCalc));
  lcd.clear(); lcd.print("CALCULADORA"); delay(1000); lcd.clear();
  String n1="", n2="", op=""; bool modoN2=false;
  while (true) {
    char t = teclado.getKey();
    if (t) {
      bipeManual(30, 500);
      if      (isDigit(t))                           { if (!modoN2) { n1+=t; lcd.print(t); } else { n2+=t; lcd.print(t); } }
      else if (t=='+'||t=='-'||t=='*'||t=='/')       { if (n1!="") { op=t; modoN2=true; lcd.print(op); } }
      else if (t=='X')                               { lcd.clear(); n1=""; n2=""; op=""; modoN2=false; }
      else if (t=='#') {
        if (n1=="0000"||n2=="0000") break;
        if (n1!=""&&n2!=""&&op!="") {
          float v1=n1.toFloat(), v2=n2.toFloat(), res=0;
          if (op=="+") res=v1+v2; else if (op=="-") res=v1-v2;
          else if (op=="*") res=v1*v2; else if (op=="/") res=(v2!=0)?v1/v2:0;
          lcd.setCursor(0,1); lcd.print("="); lcd.print(res);
          delay(3000); lcd.clear(); n1=""; n2=""; op=""; modoN2=false;
        }
      }
    }
  }
  teclado.begin(makeKeymap(teclas));
}

void mostrarRelatorioNavegavel() {
  int indice=0; bool saindo=false; lcd.clear();
  while (!saindo) {
    lcd.setCursor(0,0); lcd.print("VER: "); lcd.print(lista[indice].nome);
    lcd.setCursor(0,1); lcd.print("A^ C v B:OK 0:S");
    char t=teclado.waitForKey(); bipeManual(30,500);
    if      (t=='A') { indice=(indice<=0)?totalCandidatos-1:indice-1; lcd.clear(); }
    else if (t=='C') { indice=(indice>=totalCandidatos-1)?0:indice+1; lcd.clear(); }
    else if (t=='B') {
      long v; EEPROM.get(lista[indice].endereco, v);
      lcd.clear(); lcd.print(lista[indice].nome);
      lcd.setCursor(0,1); lcd.print("VOTOS: "); lcd.print(v); delay(2500); lcd.clear();
    }
    else if (t=='0') saindo=true;
  }
}

void jogarDino() {
  lcd.createChar(0,dinoChar); lcd.createChar(1,cactusChar); lcd.createChar(2,coracaoChar);
  bool gameRunning=true; score=0; int cactoPos=15; int dPos=0; unsigned long lMove=0;
  lcd.clear(); lcd.print("Dino de Edcley"); delay(1000);
  while (gameRunning) {
    unsigned long tA=millis(); char tJ=teclado.getKey();
    if ((tJ=='B'||tJ=='#')&&dPos==0) { dPos=1; jumpStartTime=tA; bipeManual(20,500); }
    if (tJ=='0') break;
    if (dPos==1&&(tA-jumpStartTime>=JUMP_DURATION)) dPos=0;
    if (tA-lMove>=150) { cactoPos--; if (cactoPos<0) { cactoPos=15; score++; } lMove=tA; }
    if (cactoPos==0&&dPos==0) gameRunning=false;
    lcd.clear(); lcd.setCursor(0,1-dPos); lcd.write(byte(0));
    lcd.setCursor(cactoPos,1); lcd.write(byte(1));
    lcd.setCursor(10,0); lcd.write(byte(2)); lcd.print(":"); lcd.print(score); delay(50);
  }
}

// ============================================================
//  LIMPAR EEPROM
// ============================================================
void limparEEPROM() {
  lcd.clear(); lcd.print("LIMPANDO...");
  for (int i = 0; i < 44; i++) EEPROM.write(i, 0);
  EEPROM.put(ADDR_TOTAL_ELEITORES, (int)0);
  estadoUrna=LIBERADA; nomeEleitorAtual=""; sequenciaAtual=0; alertaMesarioEnviado=false;
  delay(1000); resetarDisplay();
  if (modoAvancado) { Serial.println("EEPROM:LIMPA"); enviarStatus(); }
}

// ============================================================
//  EXECUTAR COMANDO
// ============================================================
void executarComando(String codigo) {
  if      (codigo=="3325") { limparEEPROM(); }
  else if (codigo=="3307") { modoCalculadora();          resetarDisplay(); }
  else if (codigo=="2507") { mostrarRelatorioNavegavel(); resetarDisplay(); }
  else if (codigo=="2014") { jogarDino();                resetarDisplay(); }
  else if (codigo=="2025") { mostrarCreditos();           resetarDisplay(); }
  else if (codigo=="3030") {
    // Ativa modo avancado — so volta ao basico reiniciando
    modoAvancado = true;
    estadoUrna   = LIBERADA;
    resetarDisplay();
    Serial.println("MODO:AVANCADO_ATIVADO");
    enviarStatus();
  }
  else if (codigo=="2009") {
    listarEleitoresSerial();
    mostrarListaEleitoresLCD();
    resetarDisplay();
  }
}

// ============================================================
//  REGISTRO DE ELEITOR (modo avancado via serial)
// ============================================================
void registrarEleitor(String nome) {
  nome.trim();
  if (nome.length()==0)       { Serial.println("ERRO:NOME_VAZIO");   return; }
  if (estadoUrna!=LIBERADA)   { Serial.println("ERRO:URNA_OCUPADA"); return; }
  int total = lerTotalEleitores();
  if (total>=MAX_ELEITORES)   { Serial.println("ERRO:LISTA_CHEIA");  return; }

  total++;
  EEPROM.put(ADDR_TOTAL_ELEITORES, total);
  salvarEleitor(total, nome, STATUS_REGISTRADO);

  sequenciaAtual=total; nomeEleitorAtual=nome;
  estadoUrna=AGUARDANDO_VOTO; tempoEstado=millis(); alertaMesarioEnviado=false;

  Serial.print("REGISTRADO|SEQ:"); Serial.print(total);
  Serial.print("|NOME:");          Serial.println(nome);

  entradaVoto=""; votoValido=false;
  atualizarDisplay(); enviarStatus();
}

// ============================================================
//  PROTOCOLO SERIAL
//   NOME:2009:NomeDoEleitor  -> registra eleitor
//   CMD:XXXX                 -> executa comando
//   STATUS                   -> solicitar status
// ============================================================
void processarComandoSerial(String cmd) {
  cmd.trim();
  if (cmd.startsWith("NOME:")) {
    String resto = cmd.substring(5);
    int sep = resto.indexOf(':');
    if (sep==-1) { Serial.println("ERRO:FORMATO  Use: NOME:2009:NomeAqui"); return; }
    if (resto.substring(0,sep) != "2009") { Serial.println("ERRO:SENHA_INVALIDA"); return; }
    registrarEleitor(resto.substring(sep+1));
  }
  else if (cmd.startsWith("CMD:")) { executarComando(cmd.substring(4)); }
  else if (cmd=="STATUS")          { enviarStatus(); }
  else { Serial.println("ERRO:DESCONHECIDO  Cmds: NOME:2009:Nome | CMD:XXXX | STATUS"); }
}

void lerSerial() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c=='\n'||c=='\r') {
      if (serialBuffer.length()>0) { processarComandoSerial(serialBuffer); serialBuffer=""; }
    } else {
      if (serialBuffer.length()<80) serialBuffer+=c;
    }
  }
}

// ============================================================
//  SETUP & LOOP
// ============================================================
void setup() {
  pinMode(pinoBuzzer, OUTPUT);
  Serial.begin(9600);
  lcd.begin(16, 2);
  animacaoInicio();
  modoAvancado = false; // sempre inicia no modo basico
  estadoUrna   = LIBERADA;
  resetarDisplay();
}

void loop() {

  // Serial e alertas — so no modo avancado
  if (modoAvancado) {
    lerSerial();
    if (millis()-ultimoStatusSerial >= INTERVALO_STATUS) {
      ultimoStatusSerial=millis();
      enviarStatus();
    }
    if (estadoUrna==AGUARDANDO_VOTO && !alertaMesarioEnviado) {
      if (millis()-tempoEstado >= 15000UL) {
        alertaMesarioEnviado=true;
        Serial.print("ALERTA:CHAMAR_MESARIO|NOME:");
        Serial.print(nomeEleitorAtual);
        Serial.print("|SEQ:");
        Serial.println(sequenciaAtual);
        lcd.clear();
        lcd.print("CHAMAR MESARIO!");
        lcd.setCursor(0,1);
        lcd.print(nomeEleitorAtual.substring(0,8));
        lcd.print(" S:"); lcd.print(sequenciaAtual);
        delay(3000);
        atualizarDisplay();
      }
    }
  }

  char tecla=teclado.getKey();
  if (!tecla) return;
  bipeManual(30,500);

  // ==========================================================
  //  MODO BASICO (padrao ao ligar)
  //  - VOTO: direto na tela
  //  - Qualquer numero de 2 digitos aceito
  //  - Buzzer fisico toca ao confirmar
  //  - Para ativar modo avancado: digite 3030 + #
  // ==========================================================
  if (!modoAvancado) {

    if (tecla=='N') {
      entradaVoto="00"; votoValido=true;
      lcd.clear(); lcd.print("VOTO: NULO");
      lcd.setCursor(0,1); lcd.print("CONFIRMA (#)");
    }
    else if (tecla=='#') {
      if (entradaVoto=="3325"||entradaVoto=="3307"||entradaVoto=="2507"||
          entradaVoto=="2014"||entradaVoto=="2025"||entradaVoto=="3030") {
        executarComando(entradaVoto);
      }
      else if (votoValido && entradaVoto.length()==2) {
        int addr = -1;
        if (entradaVoto=="00") {
          addr = lista[totalCandidatos-1].endereco;
        } else {
          for (int i=0;i<totalCandidatos;i++) {
            if (lista[i].numero==entradaVoto.toInt()) { addr=lista[i].endereco; break; }
          }
          if (addr<0) addr = lista[totalCandidatos-1].endereco; // numero livre vai p/ nulos
        }
        long tot; EEPROM.get(addr,tot); tot++; EEPROM.put(addr,tot);
        lcd.clear(); lcd.print("CONFIRMADO"); somUrnaFim(); delay(2000); resetarDisplay();
      }
      else { resetarDisplay(); }
    }
    else if (tecla=='*') {
      if (entradaVoto.length()>0) {
        entradaVoto.remove(entradaVoto.length()-1);
        votoValido=false;
        lcd.clear(); lcd.print("VOTO: "); lcd.print(entradaVoto);
      }
    }
    else if (isDigit(tecla)) {
      if (entradaVoto.length()<4) {
        entradaVoto+=tecla;
        lcd.setCursor(6,0); lcd.print(entradaVoto);
        if (entradaVoto.length()==2) {
          if (entradaVoto=="00") {
            lcd.setCursor(0,1); lcd.print("VOTO NULO"); votoValido=true;
          } else {
            bool achou=false;
            for (int i=0;i<totalCandidatos;i++) {
              if (lista[i].numero==entradaVoto.toInt()&&lista[i].numero!=0) {
                lcd.setCursor(0,1); lcd.print(lista[i].nome); votoValido=true; achou=true; break;
              }
            }
            if (!achou && entradaVoto!="33"&&entradaVoto!="25"&&entradaVoto!="20"&&entradaVoto!="30") {
              lcd.setCursor(0,1); lcd.print("VOTO LIVRE"); votoValido=true;
            }
          }
        }
      }
    }

  // ==========================================================
  //  MODO AVANCADO (ativado por 3030)
  //  - Cadastro de eleitor via serial/celular
  //  - Som enviado ao celular via serial (SOM_URNA)
  //  - Volta ao basico apenas reiniciando
  // ==========================================================
  } else {

    if (tecla=='N') {
      if (estadoUrna==AGUARDANDO_VOTO) {
        entradaVoto="00"; votoValido=true;
        lcd.clear(); lcd.print("VOTO: NULO");
        lcd.setCursor(0,1); lcd.print("CONFIRMA (#)");
      }
    }
    else if (tecla=='#') {
      if (entradaVoto=="3325"||entradaVoto=="3307"||entradaVoto=="2507"||
          entradaVoto=="2014"||entradaVoto=="2025"||entradaVoto=="2009") {
        executarComando(entradaVoto);
      }
      else if (votoValido && entradaVoto.length()==2 && estadoUrna==AGUARDANDO_VOTO) {
        int addr=-1;
        if (entradaVoto=="00") {
          addr=lista[totalCandidatos-1].endereco;
        } else {
          for (int i=0;i<totalCandidatos;i++) {
            if (lista[i].numero==entradaVoto.toInt()) { addr=lista[i].endereco; break; }
          }
        }
        if (addr>=0) {
          long tot; EEPROM.get(addr,tot); tot++; EEPROM.put(addr,tot);
          lcd.clear(); lcd.print("CONFIRMADO");
          lcd.setCursor(0,1); lcd.print("SEQ: "); lcd.print(sequenciaAtual);
          marcarEleitorVotou(sequenciaAtual);
          Serial.print("VOTOU|NOME:"); Serial.print(nomeEleitorAtual);
          Serial.print("|SEQ:");       Serial.println(sequenciaAtual);
          tocarSomNoCelular(); // SOM NO CELULAR VIA SERIAL
          estadoUrna=LIBERADA; nomeEleitorAtual=""; sequenciaAtual=0; alertaMesarioEnviado=false;
          delay(2000); resetarDisplay(); enviarStatus();
        }
      }
      else { resetarDisplay(); }
    }
    else if (tecla=='*') {
      if (entradaVoto.length()>0) {
        entradaVoto.remove(entradaVoto.length()-1);
        votoValido=false;
        lcd.clear();
        if (estadoUrna==AGUARDANDO_VOTO) {
          lcd.print("VOTO: "); lcd.print(entradaVoto);
          lcd.setCursor(0,1); lcd.print(nomeEleitorAtual.substring(0,16));
        } else {
          lcd.print("COD: "); lcd.print(entradaVoto);
        }
      }
    }
    else if (isDigit(tecla)) {
      if (entradaVoto.length()<4) {
        entradaVoto+=tecla;
        if (estadoUrna==AGUARDANDO_VOTO) {
          lcd.setCursor(6,0); lcd.print(entradaVoto);
          if (entradaVoto.length()==2) {
            if (entradaVoto=="00") {
              lcd.setCursor(0,1); lcd.print("VOTO NULO      "); votoValido=true;
            } else {
              bool achou=false;
              for (int i=0;i<totalCandidatos;i++) {
                if (lista[i].numero==entradaVoto.toInt()&&lista[i].numero!=0) {
                  lcd.setCursor(0,1); lcd.print(lista[i].nome); lcd.print("          ");
                  votoValido=true; achou=true; break;
                }
              }
              if (!achou&&entradaVoto!="33"&&entradaVoto!="25"&&entradaVoto!="20") {
                lcd.setCursor(0,1); lcd.print("NUM. INVALIDO  "); votoValido=false;
              }
            }
          }
        } else {
          lcd.clear(); lcd.print("COD: "); lcd.print(entradaVoto);
        }
      }
    }

  } // fim modo avancado
}
