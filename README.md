# 🎤 Speech-to-ESP32-Trigger

**Use your voice to trigger an ESP32 in real time.** Open a webpage in Chrome, speak into your microphone, and when a trigger word or phrase is detected, a command is sent to an ESP32 over **Wi-Fi (WebSocket)** or **Bluetooth Low Energy (BLE)**.

Built for haptic feedback, interactive installations, accessibility tools, performance art — or anything else where you need speech to control hardware.

---

## How It Works

```
Microphone → Chrome Speech Recognition → Trigger Match → WebSocket / BLE → ESP32 → Motor / LED / Anything
```

1. Open `speech_haptic_controller.html` in **Google Chrome** (desktop or Android).
2. Add your trigger words or phrases (e.g. *"stop"*, *"watch out"*, *"go"*).
3. Connect to your ESP32 via **WebSocket** (Wi-Fi) or **Bluetooth (BLE)**.
4. Hit **Start Listening** — when the speech recogniser detects a trigger phrase, a JSON command is sent to the ESP32 instantly.

Each trigger can have its own vibration pattern, duration, and PWM intensity.

---

## Features

- **Real-time speech recognition** via the Web Speech API (Chrome)
- **Custom trigger phrases** — add as many as you like, each with independent settings
- **Two connection modes:** Wi-Fi WebSocket and Bluetooth Low Energy (Nordic UART Service)
- **Configurable haptic patterns:** single pulse, double pulse, triple pulse, long buzz, SOS
- **Per-trigger settings:** duration (ms), pattern, PWM intensity (0–255), case sensitivity
- **Live transcript** with highlighted trigger words and confidence bar
- **Activity log** for debugging connections and triggers
- **2-second debounce** to prevent repeated firing from continuous speech
- **Interim matching** — triggers fire as words stream in, not just on final transcript

---

## Hardware

### What You Need

| Component | Notes |
|---|---|
| **ESP32 board** | Developed on a DFRobot FireBeetle 2 ESP32-S3, but any ESP32 should work with pin adjustments |
| **Vibration motor** | ERM coin motor, LRA, or any DC motor via a MOSFET driver |
| **MOSFET / motor driver** | To drive the motor from a GPIO pin (e.g. IRLZ44N, or a DRV2605 breakout) |
| **Computer or phone** | Running Google Chrome with a microphone |

### Default Wiring (FireBeetle ESP32-S3)

| Function | GPIO |
|---|---|
| Motor (PWM output) | 4 |
| Trigger LED (mirrors motor) | 21 |
| BLE status LED | 2 |

> Adjust `MOTOR_PIN`, `LED_PIN`, and `BLE_LED_PIN` in `main.cpp` for your board.

---

## Getting Started

### 1. Flash the ESP32

This project uses **PlatformIO**. Clone the repo and flash:

```bash
git clone https://github.com/ZeroOneTwo-CreativeTechnologyHub/Speech-to-ESP32-Trigger.git
cd Speech-to-ESP32-Trigger
```

Open the project in [PlatformIO](https://platformio.org/) (VS Code extension recommended), then build and upload to your board.

The `platformio.ini` is pre-configured for the DFRobot FireBeetle 2 ESP32-S3. If you're using a different board, change the `board` setting accordingly.

**Dependencies** (pulled automatically by PlatformIO):
- `NimBLE-Arduino` ^1.4.2
- `ArduinoJson` ^7.0.0

### 2. Open the Web Controller

Open `speech_haptic_controller(2).html` directly in **Google Chrome** — no server needed, just double-click the file.

> **Note:** The Web Speech API requires Chrome (or Chromium-based browsers). It will not work in Firefox or Safari.

### 3. Connect

**Via WebSocket (Wi-Fi):**
- Make sure your ESP32 is running a WebSocket server on port 81 (you'll need to add Wi-Fi + WebSocket server code, or use the BLE mode).
- Enter the ESP32's IP address in the WebSocket URL field (e.g. `ws://192.168.1.100:81`).
- Click **Connect**.

**Via Bluetooth (BLE):**
- The ESP32 firmware advertises as `Haptic-Controller` using the Nordic UART Service.
- Switch to the **Bluetooth (BLE)** tab in the web UI.
- Click **Scan & Connect** and select your device from the browser's Bluetooth picker.

### 4. Add Triggers and Listen

- Type a word or phrase into the trigger input and click **Add**.
- Customise each trigger's duration, pattern, intensity, and case sensitivity.
- Click **Start Listening** and start talking!

---

## JSON Command Format

The web controller sends JSON commands to the ESP32 in this format:

```json
{
  "cmd": "vibrate",
  "phrase": "stop",
  "duration": 500,
  "pattern": "double",
  "intensity": 255
}
```

The ESP32 responds (over BLE notify) with an acknowledgement when the pattern finishes:

```json
{
  "ack": "done",
  "pattern": "double"
}
```

---

## Vibration Patterns

| Pattern | Description |
|---|---|
| `single` | One pulse for the full duration |
| `double` | Two pulses with a short gap |
| `triple` | Three pulses with short gaps |
| `long` | Continuous buzz for the full duration |
| `sos` | SOS morse code pattern (· · · — — — · · ·) |

---

## Project Structure

```
Speech-to-ESP32-Trigger/
├── speech_haptic_controller(2).html   # Web UI — open in Chrome
├── main.cpp                           # ESP32 firmware (PlatformIO / Arduino)
├── platformio.ini                     # PlatformIO build configuration
├── LICENSE                            # MIT License
└── README.md                          # This file
```

---

## Configuration

### ESP32 Firmware (`main.cpp`)

Feature toggles at the top of the file let you enable/disable components:

| Toggle | Default | Description |
|---|---|---|
| `ENABLE_MOTOR` | `true` | PWM output to vibration motor |
| `ENABLE_LED` | `true` | Onboard LED mirrors motor activity |
| `ENABLE_BLE` | `true` | BLE advertising and data receive |
| `ENABLE_ACK` | `true` | Send acknowledgement back to browser when pattern finishes |

### Web UI

- **Global Motor Defaults** — set default duration, pattern, and intensity for new triggers
- **Per-trigger overrides** — each trigger card has its own duration, pattern, intensity, and case sensitivity settings

---

## Browser Compatibility

| Browser | Supported |
|---|---|
| Google Chrome (desktop) | ✅ |
| Google Chrome (Android) | ✅ |
| Chromium-based (Edge, Brave, etc.) | ✅ (Speech API) |
| Firefox | ❌ (no Web Speech API) |
| Safari | ❌ (no Web Speech API) |

> BLE Web Bluetooth also requires Chrome (or Chromium) and HTTPS or localhost.

---

## Use Cases

- **Haptic feedback wearables** — vibrate a wristband when a keyword is spoken
- **Interactive installations** — trigger lights, motors, or effects with voice
- **Accessibility tools** — voice-activated physical alerts
- **Performance & theatre** — cue effects hands-free
- **Prototyping** — quickly test speech-to-hardware pipelines

---

## Troubleshooting

**Speech recognition isn't starting:** Make sure you're using Chrome and have granted microphone permissions. The page must be served over HTTPS or opened as a local file.

**WebSocket won't connect:** Check that your ESP32 and computer are on the same Wi-Fi network. Verify the IP address and port (default: 81).

**BLE won't connect:** Make sure Chrome has Bluetooth permissions. The ESP32 must be advertising (check the BLE status LED on GPIO 2). BLE Web Bluetooth requires a secure context (HTTPS or localhost).

**Triggers firing too often:** Each trigger has a 2-second debounce built in. You can adjust this in the `checkTriggers` function in the HTML file.

**Motor not responding:** Check your wiring — the motor needs a MOSFET or driver circuit, not a direct GPIO connection. Verify `MOTOR_PIN` matches your wiring.

---

## Contributing

Contributions are welcome! Feel free to open issues or submit pull requests.

---

## License

This project is licensed under the [MIT License](LICENSE). You're free to use, modify, and distribute it — just include the original copyright notice.

---

## Credits

Created by [ZeroOneTwo Creative Technology Hub](https://github.com/ZeroOneTwo-CreativeTechnologyHub).
