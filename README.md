# 🗳️ Urna Eletrônica Arduino

> Sistema embarcado multifuncional: votação eletrônica, calculadora remapeável, apuração navegável e jogo do Dino — tudo em um Arduino.

---

## 📌 Visão Geral

Este projeto é uma plataforma de sistemas embarcados completa desenvolvida sobre o microcontrolador Arduino. Simula um processo eleitoral real com interface de teclado matricial, display LCD 16×2, saída sonora via Shift Register 74HC595 e persistência total dos dados em memória EEPROM não-volátil.

O firmware opera com um sistema de **códigos de gatilho secretos** que alternam entre os modos de operação sem necessidade de reinicialização.

---

## 🏗️ Hardware e Componentes

| Componente | Função |
|---|---|
| Arduino Uno / Nano | Núcleo de processamento e controle |
| LCD 16×2 | Interface visual em modo 4 bits |
| Teclado Matricial 4×4 | Entrada de dados multiplexada |
| Shift Register 74HC595 | Expansão de saídas e controle do Buzzer |
| Buzzer | Saída sonora (bipes e som de confirmação) |
| EEPROM interna | Persistência de votos e recorde do jogo |
| Potenciômetro 10kΩ | Ajuste de contraste do LCD |

---

## 📐 Mapa de Pinos

| Componente | Pino(s) Arduino | Observação |
|---|---|---|
| LCD — RS / EN | `7`, `6` | Controle do display |
| LCD — D4 a D7 | `5`, `4`, `3`, `2` | Barramento 4 bits |
| Teclado — Linhas | `8`, `9`, `10`, `11` | Saídas digitais |
| Teclado — Colunas | `A0`, `A1`, `A2`, `A3` | Analógico como digital |
| 74HC595 — Dados | `1` | Serial Data (SER) |
| 74HC595 — Latch | `0` | Storage Register Clock |
| 74HC595 — Clock | `A5` | Shift Register Clock |
| Buzzer | Saída Q0 do 74HC595 | Via expansor |
| Contraste LCD | Pino V0 (Potenciômetro) | Ajuste manual |

> ⚠️ **Atenção:** Os pinos `0` e `1` são compartilhados com a comunicação USB/Serial do Arduino. **Desconecte o 74HC595 desses pinos antes de fazer upload do firmware** para evitar falhas de gravação.

---

## 💾 Mapa de Memória EEPROM

Cada candidato ocupa **4 bytes** (`long`) na EEPROM. Os dados sobrevivem a reinicializações e cortes de energia.

| Endereço | Candidato / Dado |
|---|---|
| `0x00` (0) | JONATHAN |
| `0x04` (4) | ERICK |
| `0x08` (8) | HESLANDER |
| `0x0C` (12) | HUGO |
| `0x10` (16) | MILA |
| `0x14` (20) | ELLEN |
| `0x18` (24) | JULIENE |
| `0x1C` (28) | NULOS |
| `0x20` (32) | Recorde do Jogo Dino |

---

## 🗳️ Candidatos Cadastrados

| Número | Nome |
|---|---|
| `67` | JONATHAN |
| `75` | ERICK |
| `13` | HESLANDER |
| `69` | HUGO |
| `17` | MILA |
| `39` | ELLEN |
| `51` | JULIENE |
| `00` | NULO (tecla N) |

---

## 🔑 Códigos de Acesso

Digite o código e confirme com **`#`** para ativar o modo.

| Código | Modo | Descrição |
|---|---|---|
| `2507 #` | 📊 Relatório de Apuração | Menu navegável. `A`↑ / `C`↓ para trocar candidato, `B` para ver votos, `0` para sair |
| `3307 #` | 🔢 Calculadora | Remapeia o teclado em tempo real (+, -, *, /). Sair: digite `0000` |
| `2014 #` | 🦖 Jogo do Dino | Caracteres customizados no LCD. Salto com `B` ou `#` |
| `2025 #` | ❤️ Créditos | Exibe a equipe responsável pelo projeto |
| `3325 #` | ⚠️ Reset Total | Zera os endereços 0–40 da EEPROM. Use antes de nova sessão de votação |

---

## 🖥️ Como Usar — Votação

### Voto Nominal
1. Digite os **2 dígitos** do candidato (ex: `67`)
2. O nome aparece na linha inferior do LCD
3. Pressione `#` para confirmar — o sistema emite o som oficial e grava na EEPROM

### Voto Nulo
- Digite `00` **ou** pressione a tecla `N`
- Confirme com `#`

### Correção
- Pressione `*` para apagar o último dígito antes de confirmar

---

## 🔢 Modo Calculadora (`3307 #`)

O mapa do teclado é alterado dinamicamente via software:

| Tecla Original | Função na Calculadora |
|---|---|
| `A` | `+` |
| `B` | `-` |
| `C` | `*` |
| `N` | `/` |
| `*` | Limpar (Clear) |
| `#` | `=` (calcular) |
| `0000 #` | Sair da calculadora |

---

## 📊 Modo Relatório (`2507 #`)

Interface de auditoria de votos:

- `A` → Candidato anterior
- `C` → Próximo candidato
- `B` → Exibe o total de votos do candidato selecionado
- `0` → Sair do relatório

---

## 🦖 Jogo do Dino (`2014 #`)

Jogo de reflexos com caracteres customizados gravados na CGRAM do LCD (Dino, Cacto e Coração).

- Teclas `B` ou `#` → Saltar
- Tecla `0` → Sair
- O cacto se move da direita para a esquerda; colisões encerram o jogo
- O recorde é salvo no endereço `0x20` da EEPROM

---

## 🔧 Passo a Passo de Montagem

1. **Barramento de Energia** — Conecte as trilhas da protoboard ao 5V e GND do Arduino
2. **Display LCD** — Monte o LCD e o potenciômetro. Ajuste o contraste antes de prosseguir
3. **Shift Register 74HC595** — Insira no centro da protoboard. Conecte Dados→1, Latch→0, Clock→A5. Ligue o Buzzer na saída Q0
4. **Teclado Matricial** — Conecte os 8 fios: Linhas nos pinos 8–11 e Colunas em A0–A3. Atenção à ordem
5. **Upload do Firmware** — Desconecte os pinos 0 e 1 antes de gravar; reconecte após o upload
6. **Teste** — Ao ligar, a animação de início com bipes e barra de progresso confirma que a montagem está correta

---

## 👥 Equipe

| Membro | Função |
|---|---|
| **Edcley** | Hardware & Software |
| **Bianca** | Design |
| **Tiago** | Montagem Física |

---

## 📚 Bibliotecas Utilizadas

```cpp
#include <LiquidCrystal.h>  // Display LCD
#include <Keypad.h>          // Teclado matricial
#include <EEPROM.h>          // Persistência de dados
```

---

*Sistema Embarcado · Plataforma Arduino · Desenvolvido por Edcley & Equipe*
