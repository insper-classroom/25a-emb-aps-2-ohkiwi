# Volante para Euro Truck Simulator

## Descrição do Projeto

Este projeto consiste no desenvolvimento de um controle customizado em formato de volante para o jogo **Euro Truck Simulator**.

## Jogo

[Euro Truck Simulator 2](https://eurotrucksimulator2.com/) é um simulador de caminhão desenvolvido pela SCS Software que permite aos jogadores dirigir caminhões por uma representação da Europa, entregando cargas de uma cidade para outra. O jogo simula com precisão a direção de caminhões reais, incluindo câmbio manual, limitações de velocidade, consumo de combustível e regulamentos de trânsito.

## Ideia do Controle

Nosso controle é um volante de caminhão em escala reduzida mas com elementos realistas, incluindo:

- Volante com rotação
- Pedais analógicos (acelerador e freio)
- Botões para funções essenciais como luzes, ignição e troca de marcha.

## Inputs e Outputs

### Inputs (Sensores)

1. **Entradas Analógicas:**
   - **2x potenciômetros rotativos** para detectar a pressão no pedal do acelerador/freio.
   - **Enconder** para detectar rotação do volante.

2. **Entradas Digitais:**
   - 2 Botões para troca de marcha
   - Botão para ligar e desligar o caminhão
   - Botão de luzes

### Outros componentes:
   - Botão de liga/desliga (status do controle)
   - LED para indicar status de conexão com o computador

## Protocolo de Comunicação

O volante se comunica com o computador através de **USB**, utilizando o protocolo UART que envia as informações dos sensores e botões em tempo real. As entradas operam via **interrupções** da GPIO, garantindo baixa latência e resposta rápida.

## Diagrama de Blocos do Firmware
![Diagrama de blocos do Firmware](volante.drawio.png)

### Design Proposto
![Protótipo do Volante](volante.png)



---
### Equipe de Desenvolvimento
- **Henrique Leite dos Santos**
- **Pedro Henrique Viegas Ribeiro**