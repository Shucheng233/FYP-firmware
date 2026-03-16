# FYP Firmware

ESP32 firmware for the Smart Home Edge Intelligence project.

This repository contains the firmware running on ESP32 devices.

## device_node
ESP32 responsible for controlling smart home devices such as lights, fans, and relays.

## voice_node
ESP32 responsible for voice interaction:
- audio input
- speech-to-text
- text-to-speech
- communication with the coordinator service

## Development Framework

ESP-IDF v4.4.7

## Project Structure


FYP-firmware
│
├── device_node
│ └── main
│ ├── device_control.c
│ ├── device_control.h
│ └── esp32_controller.c
│
└── voice_node
└── main
├── voice_node.c
├── audio_input.c
├── audio_input.h
├── audio_output.c
├── audio_output.h
├── stt_client.c
├── stt_client.h
├── tts_client.c
└── tts_client.h

### device_node

Firmware running on the ESP32 responsible for controlling smart home devices 
such as lights, fans, and relays.

### voice_node

Firmware running on the ESP32 responsible for voice interaction, including:

- audio capture (microphone)
- speech-to-text (STT)
- text-to-speech (TTS)
- communication with the coordinator service
