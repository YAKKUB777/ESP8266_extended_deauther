# All files created by AI
# ESP8266 Deauther Extended

Розширена версія популярного WiFi Deauther з додатковими функціями.

## 🚀 Features

- 💀 **WiFi Deauth Attack** - відключення клієнтів від мережі
- 📡 **Beacon Spam** - створення фейкових точок доступу
- 📨 **Probe Flood** - флуд пробних запитів
- 💡 **IR Jammer** - глушіння ІЧ сигналів (38kHz)
- 📟 **Status Display** - 0.96" ST7735 для відображення статусу
- 🌐 **Web Interface** - зручне керування через браузер

## 📦 Pinout (ESP8266 NodeMCU)

| Component | GPIO | NodeMCU |
|-----------|------|---------|
| TFT CS    | 15   | D8 |
| TFT DC    | 0    | D3 |
| TFT RST   | 2    | D4 |
| TFT SCK   | 14   | D5 |
| TFT MOSI  | 13   | D7 |
| IR LED    | 14   | D5 |

## 🔧 Installation

1. Clone repository
2. Open in PlatformIO
3. Build and upload
4. Upload filesystem (data folder)

## 📱 Usage

1. Connect to WiFi `Deauther` (password: `deauther`)
2. Open browser at `192.168.4.1`
3. Select attack type and start

## 📄 License

Based on SpaceHuhn/esp8266_deauther v2