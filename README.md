# CardioIA — Fase 3 | FIAP

Protótipo acadêmico de monitoramento cardíaco simulado com ESP32, Wokwi, Edge Computing, MQTT, EMQX Cloud e FlowFuse Dashboard (Node-RED).

## Simulação

[Projeto no Wokwi](https://wokwi.com/projects/479488006027224065)

## Estrutura

- `parte1-edge-computing/`: sketch do ESP32, circuito e evidências de resiliência offline.
- `parte2-mqtt-dashboard/`: exportação do fluxo Node-RED e evidências do broker e dashboard.
- `relatorios/`: relatório técnico das Partes 1 e 2.

## Arquitetura

DHT22 (temperatura e umidade ambiente) + botão (batimentos simulados) → ESP32 → fila circular em RAM (até 1.000 registros) → EMQX Cloud via MQTT → Node-RED → cards, histórico e alertas.

A coleta ocorre a cada 2 segundos. A conexão pode ser simulada como desligada ou ligada via monitor serial. As leituras pendentes são enviadas quando o serviço volta a ficar disponível. A fila RAM é volátil e não equivale a armazenamento persistente SPIFFS.

## Executar

1. Abra o projeto Wokwi ou importe `sketch.ino` e `diagram.json`.
2. Crie um arquivo `secrets.h` a partir de `secrets.example.h`, com suas próprias credenciais MQTT. **Não publique esse arquivo.**
3. Configure o broker EMQX Cloud e o tópico `cardioia/paciente01/sinais`.
4. Importe `parte2-mqtt-dashboard/fluxo-node-red.json` no Node-RED com FlowFuse Dashboard 2.0 instalado.
5. Configure as credenciais do nó MQTT e, se necessário, o certificado CA. Abra o dashboard e observe os dados.

## Evidências e limitações

Veja as imagens nas pastas `evidencias/`. O botão **não é sensor cardíaco clínico**; ele apenas simula pulsos. A temperatura é **ambiental**, não corporal. Limiares de BPM são demonstrativos, sem finalidade diagnóstica. A fila perde dados se a simulação reiniciar. O sketch atual utiliza `setInsecure()`, portanto não valida o certificado TLS do broker; isso deve ser corrigido antes de qualquer uso fora de demonstração. O JSON do Node-RED tem configuração de verificação de certificado, mas as credenciais devem ser inseridas localmente.

## Segurança

O arquivo `secrets.h` não é incluído neste repositório. Se credenciais reais já foram compartilhadas ou publicadas, altere-as no EMQX Cloud. O broker no JSON é um endereço de exemplo de ambiente configurado, e deve ser ajustado conforme a instalação.

## Entrega

Atividade principal: Parte 1 (Edge Computing) e Parte 2 (MQTT, Fog/Cloud e dashboard). Os desafios opcionais **Ir Além 1** e **Ir Além 2** não fazem parte deste pacote.
