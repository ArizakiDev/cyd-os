# CYD-OS

> Un mini "OS" tactile pour la **ESP32-2432S028R** (« Cheap Yellow Display »), écrit avec **ESP-IDF**, **LVGL 9** et **NimBLE**.

![ESP-IDF](https://img.shields.io/badge/ESP--IDF-%E2%89%A5%205.3-red)
![Target](https://img.shields.io/badge/target-ESP32-blue)
![License](https://img.shields.io/badge/license-MIT-green)

> ⚠️ **Statut : en développement (v0.1).** Le projet est en cours de test sur le matériel réel.
> Certains réglages (orientation de l'écran, tactile) dépendent de la variante de ta carte : voir [Réglages](#-réglages-selon-ta-carte).

<!-- Ajoute ici une photo de ta carte : ![demo](docs/demo.jpg) -->

---

## ✨ Fonctionnalités (v0.1)

| Onglet | Fonction |
|--------|----------|
| **Info** | Version ESP-IDF, cœurs, flash, MAC, RAM libre, uptime, heure (NTP), état du Wi-Fi |
| **Scan** | Scanner Wi-Fi (SSID, force du signal, ouvert / sécurisé) |
| **Net** | Connexion Wi-Fi avec clavier tactile, identifiants sauvegardés (NVS) |
| **BLE** | Scanner Bluetooth Low Energy (adresse, nom, RSSI) |
| **LED** | LED RGB + luminosité de l'écran |
| **Lumi** | Capteur de lumière + luminosité automatique |

Une fois connecté au Wi-Fi :

- 🌐 **Dashboard web** : `http://<IP de la carte>/` (stats en direct + contrôle de la LED)
- 📦 **OTA** : mise à jour du firmware depuis la page web, sans câble USB, avec **rollback automatique** si le nouveau firmware plante
- 📡 **MQTT** (optionnel) : publication de l'état et contrôle de la LED
- 🕒 **Heure NTP** (fuseau Europe centrale, modifiable dans `main/main.c`)

---

## 🔧 Matériel

- **Carte** : ESP32-2432S028R (ESP-WROOM-32, écran TFT 2,8" 240×320 ILI9341, tactile résistif XPT2046, LED RGB, capteur de lumière, slot microSD)
- **Câble** USB (micro-USB ou USB-C selon ta variante), **qui transmet les données** (pas un câble de charge uniquement)

<details>
<summary>Brochage utilisé</summary>

| Périphérique | Broches |
|---|---|
| Écran ILI9341 | MOSI 13 · MISO 12 · SCLK 14 · CS 15 · DC 2 · Rétroéclairage 21 |
| Tactile XPT2046 | CLK 25 · MOSI 32 · MISO 39 · CS 33 |
| LED RGB (active à l'état bas) | R 4 · G 16 · B 17 |
| Capteur de lumière (LDR) | GPIO 34 |

</details>

---

## 🚀 Compiler et flasher

### 1. Installer ESP-IDF (version stable ≥ 5.3)

Guide officiel : <https://docs.espressif.com/projects/esp-idf/en/stable/esp32/get-started/>

**Windows**
1. Télécharge et lance l'**ESP-IDF Tools Installer** (choisis la version 5.4 ou plus récente).
2. Ouvre ensuite **« ESP-IDF PowerShell »** depuis le menu Démarrer (c'est lui qui a les bons outils dans le PATH).

**Linux / macOS**
```bash
git clone -b v5.4 --recursive https://github.com/espressif/esp-idf.git
cd esp-idf
./install.sh esp32
. ./export.sh        # à refaire dans chaque nouveau terminal
```

### 2. Récupérer le projet

```bash
git clone https://github.com/ArizakiDev/cyd-os.git
cd cyd-os
```

### 3. Configurer

```bash
idf.py set-target esp32
idf.py menuconfig
```

Dans le menu **`CYD-OS`** :

| Option | Rôle |
|---|---|
| Token du serveur web | Protège l'OTA et la LED. **Change-le** (défaut : `changeme`) |
| URI du broker MQTT | Ex. `mqtt://192.168.1.50:1883`. Vide = MQTT désactivé |
| Préfixe des topics MQTT | Défaut : `cyd` |

### 4. Compiler

```bash
idf.py build
```

Le premier build télécharge automatiquement LVGL et les pilotes (Component Manager) : il faut une connexion internet et ça peut prendre plusieurs minutes.

### 5. Flasher et suivre les logs

```bash
idf.py -p COM3 flash monitor        # Linux : /dev/ttyUSB0   macOS : /dev/cu.usbserial-xxxx
```

- Trouve ton port : *Gestionnaire de périphériques* (Windows) ou `ls /dev/ttyUSB*` (Linux).
- Quitter le moniteur : `Ctrl + ]`.
- Si tu vois `Failed to connect` : **maintiens le bouton BOOT** pendant le début du flash, ou installe le pilote **CH340 / CP210x** selon ta carte.

### 6. Mise à jour ultérieure (OTA, sans câble)

1. Recompile : `idf.py build`
2. Ouvre `http://<IP de la carte>/` (l'IP est affichée dans l'onglet **Info**)
3. Choisis `build/cyd-os.bin`, clique **Envoyer**, saisis ton token.

La carte redémarre sur le nouveau firmware. Si celui-ci plante, elle revient automatiquement à la version précédente.

> Si tu mets à jour depuis une version plus ancienne du projet, supprime le fichier `sdkconfig` (ou lance `idf.py fullclean`) pour que les nouveaux `sdkconfig.defaults` soient pris en compte.

---

## 🎛️ Réglages selon ta carte

Il existe plusieurs variantes de cette carte. Si l'affichage ou le tactile ne sont pas corrects, modifie les `#define` en haut de [`main/board.h`](main/board.h), puis recompile :

| Problème | Réglage |
|---|---|
| Couleurs inversées (effet négatif) | `LCD_INVERT_COLORS 1` |
| Image en miroir / à l'envers | `LCD_MIRROR_X`, `LCD_MIRROR_Y` |
| Tactile décalé ou inversé | `TOUCH_SWAP_XY`, `TOUCH_MIRROR_X`, `TOUCH_MIRROR_Y` |
| Capteur de lumière « à l'envers » | `LDR_INVERT 1` |
| Écran blanc / variante ST7789 | Remplacer le pilote `esp_lcd_ili9341` par `esp_lcd_st7789` |

---

## 🩺 Dépannage

- **Crash / manque de RAM (Wi-Fi + BLE + LVGL)** : dans `main/board.c`, passe `buffer_size` de `LCD_H_RES * 40` à `LCD_H_RES * 20`.
- **Firmware trop gros pour l'OTA** : les partitions OTA font 1,66 Mo. Active *Optimize for size* dans `menuconfig → Compiler options`.
- **Le Wi-Fi ne se connecte pas** : vérifie le mot de passe, utilise un réseau **2,4 GHz** (l'ESP32 ne supporte pas le 5 GHz).
- **Caractères accentués absents** : la police LVGL par défaut ne les contient pas (les textes de l'interface sont volontairement en ASCII).
- **Sécurité** : le serveur web est en HTTP, sans chiffrement. À utiliser **uniquement sur ton réseau local**, jamais exposé sur Internet.

---

## 📡 API (web et MQTT)

**HTTP**

| Route | Méthode | Auth | Description |
|---|---|---|---|
| `/` | GET | non | Page dashboard |
| `/api/status` | GET | non | JSON : version, RAM, uptime, luminosité, RSSI, IP |
| `/api/led?r=0&g=255&b=0` | POST | `X-Token` | Règle la LED RGB (0–255) |
| `/ota` | POST | `X-Token` | Corps = fichier `.bin` du firmware |

**MQTT** (si une URI est configurée)

| Topic | Sens | Contenu |
|---|---|---|
| `cyd/status` | publié toutes les 10 s | `{"heap":..., "ldr":..., "rssi":...}` |
| `cyd/led/set` | reçu | `r,g,b` (ex. `255,0,128`) |

---

## 🗂️ Structure du projet

```
cyd-os/
├── CMakeLists.txt
├── partitions.csv          # 2 partitions OTA + stockage
├── sdkconfig.defaults      # réglages par défaut (BLE, rollback OTA…)
├── .github/workflows/      # CI : compilation à chaque push
└── main/
    ├── main.c              # démarrage et services réseau
    ├── board.c / board.h   # écran, tactile, PWM, ADC, brochage
    ├── ui.c                # interface LVGL (onglets)
    ├── wifi_mgr.c          # connexion Wi-Fi + sauvegarde NVS
    ├── wifi_scan.c         # scanner Wi-Fi
    ├── ble_scan.c          # scanner BLE (NimBLE)
    ├── web_server.c        # serveur HTTP + OTA
    ├── mqtt_service.c      # client MQTT
    ├── Kconfig.projbuild   # options menuconfig
    └── web/index.html      # page du dashboard
```

---

## 🗺️ Roadmap

### ✅ v0.1 : Base (actuelle)
- [x] Interface tactile LVGL à onglets
- [x] Scanner Wi-Fi, scanner BLE
- [x] Connexion Wi-Fi avec clavier + sauvegarde
- [x] Dashboard web, OTA avec rollback
- [x] MQTT, heure NTP

### 🔜 v0.2 : Confort d'utilisation
- [ ] **Calibration tactile** au premier démarrage (sauvegardée en NVS)
- [ ] **Portail captif** : si aucun Wi-Fi n'est configuré, la carte crée son propre réseau pour être configurée depuis un téléphone
- [ ] **Thème sombre / clair** et réglages persistants (luminosité, fuseau horaire)
- [ ] Écran de démarrage (splash) et icônes

### 🔜 v0.3 : Mises à jour & connectivité
- [ ] **OTA depuis GitHub Releases** (HTTPS) : bouton « Rechercher une mise à jour » sur l'écran
- [ ] Workflow GitHub qui publie automatiquement le `.bin` à chaque tag de version
- [ ] Authentification renforcée du dashboard
- [ ] mDNS : accès via `http://cyd-os.local`

### 🔜 v0.4 : Nouvelles apps
- [ ] **Station météo** (API en ligne) avec icônes
- [ ] **Moniteur de capteurs** avec graphiques (historique du capteur de lumière, RAM)
- [ ] **Explorateur de carte SD** (le slot partage le bus SPI avec le tactile : à étudier)
- [ ] **Contrôle domotique** : boutons configurables (relais, MQTT, Home Assistant)

### 🔮 v1.0 : Stable
- [ ] Système d'applications modulaire (ajouter une app = un fichier)
- [ ] Support Matter / Home Assistant natif
- [ ] Documentation complète et photos / vidéos de démonstration

> Les idées et priorités peuvent évoluer. Propose les tiennes dans les [Issues](../../issues) !

---

## 🤝 Contribuer

1. Fork le dépôt, crée une branche (`git checkout -b ma-fonctionnalite`)
2. Vérifie que ça compile : `idf.py build`
3. Ouvre une Pull Request en décrivant ce qui change et sur quelle variante de carte tu as testé

## 📄 Licence

[MIT](LICENSE). Utilise, modifie et partage librement.

## 🙏 Crédits

[ESP-IDF](https://github.com/espressif/esp-idf) · [LVGL](https://lvgl.io) · [esp_lvgl_port](https://components.espressif.com/components/espressif/esp_lvgl_port) · [Apache NimBLE](https://mynewt.apache.org/)
