# Zignalmeister 2000 - Käyttöopas

ESP32-pohjainen langaton LoRa-sensorijärjestelmä. Tämä on **käyttöopas** — tekninen kuvaus löytyy [LAITEKUVAUS.md](../docs/LAITEKUVAUS.md).

**Pääalusta:** XIAO ESP32S3 + Wio-SX1262 LoRa
**Versio:** 2.7.0

---

## Quick Navigation

| Haluan... | Siirry |
|-----------|--------|
| **Koota laitteen ensimmäistä kertaa** | [Laitteisto](#laitteisto) → [Pikaohje](#pikaohje) |
| **Ladata firmwaren** | [Pikaohje](#pikaohje) |
| **Muuttaa asetuksia** | [Konfigurointi](#konfigurointi) |
| **Säätää LoRa-parametreja** | [LoRa-asetukset](#lora-asetukset) |
| **Käyttää Mesh-verkkoa** | [Mesh-verkko](#mesh-verkko) |
| **Asentaa näytön** | [Näytöt](#näytöt) |
| **Tallentaa dataa Raspberry Pi:lle** | [Raspberry Pi Deployment](#raspberry-pi-deployment) |
| **Korjata ongelman** | [Vianmääritys](#vianmääritys) |

**Aloita tästä:** [Laitteisto](#laitteisto) → [Pikaohje](#pikaohje)

---

## Sisällysluettelo

1. [Yleiskatsaus](#yleiskatsaus)
2. [Laitteisto](#laitteisto)
3. [Pikaohje](#pikaohje)
4. [Konfigurointi](#konfigurointi)
5. [LoRa-asetukset](#lora-asetukset)
6. [Mesh-verkko](#mesh-verkko)
7. [Näytöt](#näytöt)
8. [Raspberry Pi Deployment](#raspberry-pi-deployment)
9. [Vianmääritys](#vianmääritys)
10. [Lisätiedot](#lisätiedot)

---

## Yleiskatsaus

### Mikä tämä on?

Langaton kaksisuuntainen kommunikaatiojärjestelmä LoRa-radioteknologialla. Kantama jopa useita kilometrejä riippuen ympäristöstä.

**Pääominaisuudet:**
- ✅ **Plug-and-play** — sama firmware kaikkiin laitteisiin, rooli tunnistetaan automaattisesti
- 📡 **LoRa 868 MHz** — pitkä kantama, matala virrankulutus
- 🔄 **Mesh-verkko** — välityssolmut laajentavat kantamaa
- 💡 **LM393 valotunnistus** — savuilmaisimen LED-välähdysten tunnistus
- 📊 **TFT/LCD-näytöt** — reaaliaikainen tila ja telemetria
- 💾 **Raspberry Pi -integraatio** — automaattinen datan tallennus ja web-dashboard
- 🛡️ **Luotettava** — watchdog, crash report -järjestelmä, automaattinen palautuminen

**Käyttökohteet:**
- Paloturvallisuus (savuilmaisimen tilan valvonta)
- Rakennusautomaatio
- Teollisuuden sensorit

**Tekniset tiedot:** [LAITEKUVAUS.md](../docs/LAITEKUVAUS.md)

---

## Laitteisto

> **Katso täydelliset kytkentäkaaviot ja pinnimääritykset:** [LAITEKUVAUS.md § 2](../docs/LAITEKUVAUS.md#2-laitteisto)

### Tarvittavat komponentit

**Perusyksikkö (XIAO ESP32S3):**
- 1× Seeed XIAO ESP32S3
- 1× Wio-SX1262 LoRa-moduuli (pinotaan XIAO:n alle B2B-liittimellä)
- 1× USB-C-kaapeli

**Roolin mukaan:**

| Rooli | Lisälaitteet | Näyttö (DISPLAY_MODE=2) |
|-------|--------------|-------------------------|
| **SENDER** | LM393 valotunnistin (4 johtoa) | LCD automaattisesti jos kytketty |
| **RECEIVER** | TFT UART-näyttö (4 johtoa) | TFT automaattisesti |
| **RELAY** | Ei pakollisia | LCD automaattisesti jos kytketty |

**Optiot:**
- LCD 16x2 I2C (SENDER/RELAY diagnostiikka, tunnistetaan automaattisesti)
- Reset-nappi (momentary NO, RST ↔ GND)

### Pinnit (XIAO ESP32S3)

Käytettävissä 6 pinniä (D0-D5). LoRa käyttää sisäisesti D8-D10 + B2B-liitintä.

| Pinni | GPIO | SENDER | RECEIVER | RELAY |
|-------|------|--------|----------|-------|
| D0 | 1 | (vapaa) | (vapaa) | jumpperi → D1 |
| D1 | 2 | (vapaa) | jumpperi → D2 | jumpperi → D0 |
| D2 | 3 | LM393 AO | jumpperi → D1 | (vapaa) |
| D3 | 4 | LM393 DO | (vapaa) | (vapaa) |
| D4 | 5 | LCD SDA | TFT TX | LCD SDA |
| D5 | 6 | LCD SCL | TFT RX | LCD SCL |
| 3V3 | — | LM393 VCC, LCD VCC | TFT VCC | LCD VCC |
| GND | — | LM393 GND, LCD GND | TFT GND | LCD GND |

> **Varatut pinnit (älä käytä):**
> D8-D10 (GPIO 7-9) = SPI → SX1262
> B2B (GPIO 38-42) = SX1262 ohjaussignaalit

### Roolitunnistus

Laite tunnistaa roolinsa automaattisesti käynnistyksen yhteydessä:

| Rooli | Jumpperi | Tunnistus |
|-------|----------|-----------|
| **SENDER** | Ei jumppereita | Oletus |
| **RECEIVER** | D1 ↔ D2 | D2 luetaan LOW:ksi |
| **RELAY** | D0 ↔ D1 | D0 luetaan LOW:ksi |

**Huom:** `DISPLAY_MODE=2` (auto) tunnistaa näyttötyypin roolin perusteella. Ei vaadi config.h-muutoksia laitteiden välillä.

### Kytkennät

**SENDER (LM393 + optio LCD):**
```
LM393 → XIAO          LCD → XIAO (optio)
──────────────        ─────────────────
VCC → 3V3             VCC → 3V3
GND → GND             GND → GND
AO  → D2 (GPIO 3)     SDA → D4 (GPIO 5)
DO  → D3 (GPIO 4)     SCL → D5 (GPIO 6)

Ei jumppereita
```

**RECEIVER (TFT UART):**
```
TFT → XIAO
──────────
VCC → 3V3
GND → GND
TX  → D4 (GPIO 5)
RX  → D5 (GPIO 6)

Jumpperi: D1 ↔ D2 (RECEIVER-roolin tunnistus)
```

**RELAY (optio LCD):**
```
LCD → XIAO (optio)
──────────────────
VCC → 3V3
GND → GND
SDA → D4 (GPIO 5)
SCL → D5 (GPIO 6)

Jumpperi: D0 ↔ D1
```

### Akkukäyttö ja jännitteen mittaus

**Akku XIAO:n BAT-pineihin:**
```
Li-Ion 3.7V (18650 tai pouch)
────────────────────────────
BAT+  → XIAO BAT+ (piirilevyn pohjassa)
BAT-  → XIAO BAT- (piirilevyn pohjassa)

USB lataa akkua automaattisesti (U8-piiri).
```

**Jännitteenjakaja akun jännitteen mittaukseen (valinnainen):**
```
BAT+ ─── 100kΩ ─── D0 (GPIO1) ─── 100kΩ ─── GND
```

- Jakosuhde 2:1 → 4.2V akku → 2.1V ADC:lle (ESP32S3 ADC range 0–3.3V)
- D0/GPIO1 on vapaa SENDER- ja RECEIVER-rooleissa
- ⚠️ **Relay-moodi:** D0 on osa relay-jumpperia (D0↔D1). Käytä D1/GPIO2 tai irrota jumpperi mittauksen ajaksi.

**Aktivointi config.h:ssa:**
```cpp
#define ENABLE_BATTERY_MONITOR true
```

**Aktivointi platform_hal.h:ssa:**
```cpp
.batteryAdc = 1,   // D0/GPIO1 (tai 2 jos D1/GPIO2)
```

Tämän jälkeen sarjamonitorissa näkyy akkutila:
```
[Battery] LOW: 3.25V (21%)
[Battery] CRITICAL: 2.98V (0%)
```

CSV-dataan lisätään sarakkeet `battery_voltage` ja `battery_percent`.

---

### Reset-nappi

XIAO:n piirilevyn pohjassa on RST-pad. Kytke painonappi RST:n ja GND:n väliin:

```
RST pad ──[momentary NO nappi]── GND pad
```

- Takuuvarma laitteistotason reset (toimii vaikka firmware jumiutuisi)
- Ei vaadi vastuksia (sisäinen pull-up)
- Reset-syy näkyy logissa: `Last reset: External reset`

### K1-nappi (Wio-SX1262)

Wio-SX1262 -lisälevyssä on K1-painonappi (GPIO21, sama kuin XIAO:n LED-pinni).

| Tilanne | Toiminta |
|---------|----------|
| **Hereillä** | Painallus (>100 ms) → uudelleenkäynnistys (`esp_restart()`) |
| **Deep sleep** | EXT1-herätys → käynnistys normaalisti |
| **Light sleep** | GPIO21-pollaus 2s välein → käynnistys normaalisti |

> **Huom:** OTA firmware-päivitystoiminto (WiFi AP via K1-nappi) on suunniteltu mutta ei vielä toteutettu. Ks. `docs/OTA_PLAN.md`.

---

## Pikaohje

### 10 minuutin käyttöönotto (XIAO ESP32S3)

**Vaihe 1: Kokoa laitteisto**
1. Pinoa Wio-SX1262 XIAO:n alle (B2B-liitin)
2. Kytke periferiat roolin mukaan (katso [Kytkennät](#kytkennät))
3. Aseta jumpperi roolin mukaan (RELAY/RECEIVER) tai jätä pois (SENDER)

**Vaihe 2: Lataa firmware**
1. Avaa projekti VS Code + PlatformIO:ssa
2. Valitse environment: `xiao_esp32s3`
3. Tarkista `firmware/config.h`:
   ```cpp
   #define USE_XIAO_SX1262 true
   #define DISPLAY_MODE 2  // Oletus: automaattinen (suositeltu)
   ```
4. PlatformIO: Upload → **lataa kaikkiin laitteisiin** (sama koodi!)

**Vaihe 3: Testaa**
1. Käynnistä laitteet
2. Avaa Serial Monitor (115200 baud)
3. Tarkista logeista:
   - Sender: `>>> SENDER MODE` ja `TX [1]: SEQ:0...`
   - Receiver: `>>> RECEIVER MODE` ja `RX [2]: SEQ:0...`
   - Relay: `>>> RELAY MODE`

**Toimii?** ✅ Valmis käyttöön!
**Ei toimi?** → [Vianmääritys](#vianmääritys)

---

## Konfigurointi

Kaikki asetukset ovat `firmware/config.h`:ssa. Muokkaa tiedostoa ja lataa uudelleen.

### Perusasetukset

```cpp
// Alusta (PAKOLLINEN!)
#define USE_XIAO_SX1262 true        // true = XIAO+SX1262, false = DevKit+RYLR896

// Näyttö (DISPLAY_MODE=2 on suositeltu oletus kaikille laitteille)
#define DISPLAY_MODE 2              // 0=ei näyttöä, 1=LCD pakotettuna, 2=automaattinen
#define FORCE_LCD false             // true = pakota LCD kaikille rooleille
#define DISPLAY_UPDATE_INTERVAL 2000 // Päivitysväli (ms)

// LoRa-verkko
#define LORA_NETWORK_ID 6           // Verkkotunnus (sama kaikissa!)
#define LORA_ADDRESS_RECEIVER 1     // Vastaanottajan osoite
#define LORA_ADDRESS_SENDER 2       // Lähettäjän osoite
#define LORA_SPREADING_FACTOR 12    // SF7-12 (12=max kantama)
#define LORA_TX_POWER 20            // 0-20 dBm (20=max teho)

// Lähetysvälit
#define DATA_OUTPUT_INTERVAL 2000   // Lähetystaajuus (ms)
```

### Mesh-verkko

```cpp
// Mesh-toiminnot
#define ENABLE_MESH_NETWORK true    // Aktivoi mesh
#define MESH_MAX_HOPS 3             // Max välityshyppyjä
#define MESH_JITTER_MAX_MS 100      // Törmäyksenesto (0-100ms)

// Duplikaattien esto (SUOSITELTU!)
#define ENABLE_MESH_DEDUPLICATION true
#define DEDUP_BUFFER_SIZE 20
#define DEDUP_TIMEOUT_MS 10000      // 10s muisti

// RSSI-suodatus
#define ENABLE_MESH_RSSI_FILTER true
#define MESH_RELAY_MIN_RSSI -100    // Min RSSI välitykseen
```

### Anturit

```cpp
// LM393 valotunnistus (SENDER)
#define ENABLE_LIGHT_DETECTION true
#define LIGHT_POLL_INTERVAL 100     // Näytteenotto (ms)
#define LIGHT_THRESHOLD 1000        // Analoginen kynnys
#define LIGHT_TRIGGER_DURATION 50   // Min kesto (ms)

// Akkumonitorointi (vaatii ulkoisen jännitteenjakajan, ks. Kytkennät)
#define ENABLE_BATTERY_MONITOR false    // true kun jakaja kytketty
#define BATTERY_LOW_THRESHOLD 3.3       // Varoitus (V)
#define BATTERY_CRITICAL_THRESHOLD 3.0  // Kriittinen (V)
#define BATTERY_CHECK_INTERVAL 60000   // Tarkistusväli (ms)

// Muut optiot (oletuksena pois)
#define ENABLE_AUDIO_DETECTION false
#define ENABLE_CURRENT_MONITOR false
```

### Luotettavuus

```cpp
// TX Retry
#define ENABLE_TX_RETRY true
#define TX_MAX_RETRIES 3
#define TX_RETRY_BACKOFF_MS 100     // Eksponentiaalinen backoff

// Watchdog
#define ENABLE_WATCHDOG true
#define WATCHDOG_TIMEOUT_MS 10000   // 10s timeout

// NVS-muisti
#define ENABLE_NVS true             // Tallenna tilastot flash-muistiin
```

### Data-output

```cpp
// PC/Raspberry Pi
#define ENABLE_CSV_OUTPUT true      // CSV-muoto
#define DATA_OUTPUT_INTERVAL 2000   // Lähetysväli (ms)

// Telemetria (optio)
#define ENABLE_EXTENDED_TELEMETRY false // Uptime, heap, temp
#define ENABLE_PACKET_STATS false       // Yksityiskohtaiset tilastot
```

---

## LoRa-asetukset

### Optimoidut asetukset maksimikantamalle

```cpp
// config.h:
#define LORA_NETWORK_ID 6               // Verkkotunnus (sama molemmissa!)
#define LORA_ADDRESS_RECEIVER 1         // Vastaanottajan osoite
#define LORA_ADDRESS_SENDER 2           // Lähettäjän osoite
#define LORA_SPREADING_FACTOR 12        // SF12 = max kantama
#define LORA_BANDWIDTH 125              // 125 kHz
#define LORA_CODING_RATE 1              // 4/5
#define LORA_TX_POWER 20                // 20 dBm = max teho
```

### Spreading Factor (SF)

| SF | Nopeus | Kantama | Käyttö |
|----|--------|---------|--------|
| SF7 | Nopea | Lyhyt | Sisätilat, lähietäisyys |
| SF9 | Keskitaso | Keskitaso | Rakennukset |
| SF12 | Hidas | Pitkä | **Ulkotilat, max kantama** |

**Etäisyysarviot (SF12):**
- Lähellä (0-10 m): RSSI > -70 dBm, pakettihäviö < 1%
- Keskietäisyys (10-100 m): RSSI -70 to -90 dBm
- Pitkä (100+ m): RSSI < -90 dBm, vaatii näköyhteyden

### Viestiformaatti

**Lähettäjä → Vastaanottaja:**
```
SEQ:42,LED:1,TOUCH:0,SPIN:2,COUNT:42
```

**Vastaanottaja → Lähettäjä (ACK):**
```
ACK,SEQ:5,LED:0,TOUCH:1,SPIN:3,RSSI:-75,SNR:9
```

> ACK sisältää vastaanottajan mittaamat RSSI- ja SNR-arvot kaksisuuntaista signaalinlaatuseurantaa varten.

### Signaalin laatu

**RSSI (Received Signal Strength Indicator):**
| RSSI | Laatu | Kuvaus |
|------|-------|--------|
| -40 dBm | ⭐⭐⭐⭐⭐ | Erinomainen (lähellä) |
| -70 dBm | ⭐⭐⭐⭐ | Hyvä |
| -90 dBm | ⭐⭐ | Heikko |
| -120 dBm | ⭐ | Huono (yhteys katkeaa pian) |

**SNR (Signal-to-Noise Ratio):**
| SNR | Laatu |
|-----|-------|
| +10 dB | Erinomainen |
| 0 dB | Hyvä |
| -10 dB | Heikko |
| -20 dB | Huono |

### TX Retry (automaattinen uudelleenlähetys)

Parantaa viestien läpimenoastetta häiriintyneissä ympäristöissä.

**Konfiguraatio:**
```cpp
#define ENABLE_TX_RETRY true
#define TX_MAX_RETRIES 3
#define TX_RETRY_BACKOFF_MS 100
```

**Toiminta:**
1. Jos lähetys epäonnistuu → odota 100 ms, yritä uudelleen
2. Jos epäonnistuu → odota 200 ms, yritä uudelleen
3. Jos epäonnistuu → odota 400 ms, yritä uudelleen
4. Jos kaikki epäonnistuvat → hylkää viesti

**Käyttö koodissa:**
```cpp
// Perusfunktio (ei retryjä):
bool success = sendLoRaMessage(message, targetAddress);

// Luotettava versio (automaattinen retry):
bool success = sendLoRaMessageReliable(message, targetAddress);
```

---

## Mesh-verkko

### Mikä on Mesh-verkko?

Mesh-verkko mahdollistaa viestien välittämisen useiden laitteiden kautta → laajempi kantama ja parempi luotettavuus.

**Roolit:**
- **Receiver (1):** Keskussolmu, vastaanottaa kaikki viestit
- **Sender (2+):** Lähettäjät, voivat olla useita
- **Relay (3+):** Välityssolmut, toistavat viestejä eteenpäin

**Esimerkki:**
```
SENDER ──── (100m) ───→ RELAY ──── (100m) ───→ RECEIVER
         (heikko RSSI)          (heikko RSSI)

Ilman relayta: 200m → yhteys katkeaa
Relayn kanssa: 2× 100m → toimii!
```

### Relay-solmun aktivointi

**Laitteisto:**
```
Jumpperi: D0 ↔ D1 (GPIO 1 ↔ GPIO 2)
```

**Relay-solmu:**
1. Vastaanottaa viestejä muilta laitteilta
2. Tarkistaa ettei viesti ole duplikaatti (hash-pohjainen)
3. Kasvattaa hop-laskuria
4. Välittää viestin eteenpäin (jos hoppeja jäljellä)

### Mesh-konfiguraatio

```cpp
// Perusasetukset
#define ENABLE_MESH_NETWORK true
#define MESH_MAX_HOPS 3             // Max välityshyppyjä
#define MESH_JITTER_MAX_MS 100      // Random jitter (törmäyksenesto)

// Duplikaattien esto (SUOSITELTU!)
#define ENABLE_MESH_DEDUPLICATION true
#define DEDUP_BUFFER_SIZE 20        // Muistettavien viestien määrä
#define DEDUP_TIMEOUT_MS 10000      // 10s muisti

// RSSI-suodatus
#define ENABLE_MESH_RSSI_FILTER true
#define MESH_RELAY_MIN_RSSI -100    // Min RSSI välitykseen
```

### Mesh-viestiformaatti

```
+RCV=<osoite>,<pituus>,<hop>|<srcId>|<dstId>|<payload>,<rssi>,<snr>
```

**Esimerkki:**
```
+RCV=1,25,2|142|0|SEQ:42,LED:1,-67,9
```
- `hop=2`: Viesti kulkenut 2 välityssolmun kautta
- `srcId=142`: Alkuperäinen lähettäjä
- `dstId=0`: Broadcast kaikille

### Duplikaattien esto

Mesh-verkossa viesti voi saapua useaa reittiä pitkin. Duplikaattien esto varmistaa, että jokainen viesti käsitellään vain kerran:

1. Viesti saapuu → lasketaan hash (lähettäjä + sekvenssinumero)
2. Tarkistetaan onko hash nähty aiemmin (DEDUP_BUFFER_SIZE)
3. Jos nähty → hylätään duplikaattina
4. Jos uusi → käsitellään ja tallennetaan hash (DEDUP_TIMEOUT_MS)

---

## Näytöt

Zignalmeister tukee kahta näyttötyyppiä: LCD 16x2 I2C (diagnostiikka) ja TFT UART (koko UI).

### DISPLAY_MODE — Näyttöasetus

| Moodi | Kuvaus | Käyttö |
|-------|--------|--------|
| `0` | Ei näyttöä | Minimaalinen virrankulutus |
| `1` | LCD pakotettu | LCD aina päällä (kaikki roolit probaavat I2C) |
| **`2`** | **Automaattinen (suositeltu)** | **Rooli määrää näytön automaattisesti** |

**`DISPLAY_MODE=2` on suositeltu oletus kaikille laitteille.** Voit flashata saman firmwaren jokaiseen laitteeseen ilman config.h-muutoksia:

| Rooli | Näyttö | Toiminta käynnistyksessä |
|-------|--------|--------------------------|
| RECEIVER | TFT (UART) | TFT aktivoituu automaattisesti |
| SENDER | LCD (I2C) tai ei näyttöä | I2C-probe osoitteeseen 0x27 — LCD jos kytketty |
| RELAY | LCD (I2C) tai ei näyttöä | I2C-probe osoitteeseen 0x27 — LCD jos kytketty |

LCD-tunnistus on takuuvarma: I2C ACK/NACK on laitteistotason signaali (<1 ms). Jos LCD:tä ei ole kytketty, laite jatkaa normaalisti ilman näyttöä (Serial fallback).

**`FORCE_LCD`-override:**
Jos haluat LCD:n nimenomaan RECEIVER-laitteelle (TFT:n sijaan), aseta `FORCE_LCD true` config.h:ssa. Tämä toimii vain kun `DISPLAY_MODE=2`.

### LCD 16x2 I2C (SENDER/RELAY)

**Käyttö:** Yksinkertainen diagnostiikka (RSSI, pakettilaskurit, LM393-arvot).

**Kytkentä:**
```
LCD → XIAO
──────────────────
SDA → D4 (GPIO 5)
SCL → D5 (GPIO 6)
VCC → 3V3
GND → GND

I2C-osoite: 0x27 (tunnistetaan automaattisesti)
```

**Konfiguraatio (DISPLAY_MODE=2, oletus):**
```cpp
#define DISPLAY_MODE 2               // Auto: LCD tunnistetaan SENDER/RELAY:lle
#define LCD_LAYOUT_NUMBER 5          // 1-7, katso layoutit alla
#define LCD_SENDER_ENABLED true      // LCD myös SENDER:lle
```

**Layoutit:**

| # | Nimi | Sisältö |
|---|------|---------|
| 1 | Minimalist Signal | RSSI dBm |
| 2 | LoRa Range Test | RSSI, SNR, pakettihäviö |
| 3 | Light Sensor | LM393 DO, välähdyslaskuri, hälytys |
| 4 | Developer Debug | Sekvenssi, LED, heap, lämpötila |
| 5 | Mesh Receiver Test | Laitteet, dBm, hyppyluku, lähde, valo, häviö |
| 6 | LM393 Detailed | Kirkkaus%, välähdykset, hälytys, sekvenssi |
| 7 | Mesh/RELAY Stats | RX, välitetyt, pudotetut, avg hops |

### TFT UART-näyttö (RECEIVER)

**Käyttö:** Täysvärinäyttö (320×240 px) LoRa-datan visualisointiin. Aktivoituu automaattisesti RECEIVER-roolille kun `DISPLAY_MODE=2`.

**Kytkentä:**
```
TFT → XIAO
──────────────────
VCC → 3V3
GND → GND
TX  → D4 (GPIO 5)
RX  → D5 (GPIO 6)

Baudrate: 115200
```

**Konfiguraatio (DISPLAY_MODE=2, oletus):**
```cpp
#define DISPLAY_MODE 2               // Auto: TFT aktivoituu RECEIVER:lle
#define DISPLAY_UPDATE_INTERVAL 2000 // Päivitysväli (ms)
```

**TFT-näytön ominaisuudet:**
- Reaaliaikainen RSSI/SNR
- Pakettilaskurit (RX, Lost, Relay)
- Sensoridatan visualisointi
- Mesh-topologia (hop-laskuri)

**TFT-firmwaren kustomointi:**
Jos haluat muokata TFT-näytön omaa firmwarea (värit, layoutit, fontit), katso:
→ **[TFT_firmware/README.md](../TFT_firmware/README.md)**

---

## Raspberry Pi Deployment

Vastaanota ESP32:n lähettämä LoRa-data USB:n kautta, tallenna SQLite-tietokantaan ja näytä web-dashboardissa.

### Yleiskuvaus

Raspberry Pi toimii keskuspalvelimena, joka:
1. Vastaanottaa ESP32:n lähettämän datan USB-sarjaportin kautta
2. Tallentaa datan SQLite-tietokantaan
3. Tarjoaa web-dashboardin datan tarkasteluun
4. (Optio) Lähettää hälytykset Discord-bottiin

### Asennus

#### 1. One-Command Asennus (Suositeltu)

```bash
cd ~
git clone https://github.com/YOUR_REPO/zignalmeister.git
cd zignalmeister

# Kytke ESP32 USB-kaapelilla (RECEIVER-laite)
ls -la /dev/ttyUSB* /dev/ttyACM*   # → /dev/ttyUSB0 tai /dev/ttyACM0

# Aja asennus
sudo bash deployment/scripts/quick_install.sh
```

**Asentaa automaattisesti:**
- Data logger (USB serial → SQLite-tietokanta)
- Web Dashboard (http://raspberrypi.local:5000)
- Simple Data Viewer (http://raspberrypi.local:8080)
- Discord bot (valinnainen, vaatii tokenin)
- Automaattiset varmuuskopiot (päivittäin klo 02:00)
- Systemd-palvelut (käynnistyy automaattisesti bootissa)

#### 2. Tarkista että toimii

```bash
bash deployment/scripts/check_status.sh
```

Pitäisi näyttää:
```
✓ zignalmeister-logger is running
✓ zignalmeister-dashboard is running
✓ avahi-daemon is running (.local domain enabled)
✓ USB device found
✓ Database exists: /home/pi/lora_data/lora_data.db
✓ Database has data: 42 readings
```

**Jos servicet eivät toimi:**
```bash
sudo bash deployment/scripts/fix_services.sh
bash deployment/scripts/check_status.sh
```

`fix_services.sh` korjaa: avahi-daemonin, Werkzeug-virheet dashboardissa, Discord-botin (pysäyttää jos ei tokenia).

#### 3. Avaa Web-UI

**Simple Viewer (yksinkertainen taulukko):**
```bash
python3 deployment/simple_viewer/simple_viewer.py ~/lora_data/lora_data.db 8080
```
Avaa: http://raspberrypi.local:8080

**Full Dashboard (graafit, real-time):**
```
http://raspberrypi.local:5000
```

**Dashboard ominaisuudet:**
- 🏠 Pääsivu: Kaikki laitteet, RSSI-graafit
- 📊 Device-sivu: Yksityiskohtaiset graafit per laite
- 💾 CSV-export: Lataa data CSV-muodossa

### Päivitykset ja Ylläpito

#### Päivitä Koodi (Git Pull)

```bash
cd ~/zignalmeister
git pull

# TÄRKEÄ: Käynnistä palvelut uudelleen
# (Git pull EI automaattisesti päivitä käynnissä olevia palveluita!)
sudo systemctl restart zignalmeister-dashboard
sudo systemctl restart zignalmeister-logger

# Tarkista status
sudo systemctl status zignalmeister-dashboard
```

#### Systemd-palveluiden Hallinta

```bash
# Tarkista tila
sudo systemctl status zignalmeister-dashboard
sudo systemctl status zignalmeister-logger

# Käynnistä / pysäytä
sudo systemctl restart zignalmeister-dashboard
sudo systemctl stop zignalmeister-dashboard
sudo systemctl start zignalmeister-dashboard

# Seuraa lokeja reaaliajassa
sudo journalctl -u zignalmeister-dashboard -f
sudo journalctl -u zignalmeister-logger -f

# Viimeisimmät lokit
sudo journalctl -u zignalmeister-dashboard -n 50
sudo journalctl -u zignalmeister-logger -n 50

# Ota käyttöön / pois bootissa
sudo systemctl enable zignalmeister-dashboard
sudo systemctl disable zignalmeister-dashboard
```

#### Päivitä Asennus (Quick Install)

```bash
# Quick install on idempotent — voit ajaa uudelleen turvallisesti
cd ~/zignalmeister && git pull
sudo bash deployment/scripts/quick_install.sh
```

#### Poista Asennus (Uninstall)

```bash
sudo bash deployment/scripts/uninstall.sh
# Kyselee: haluatko poistaa tietokannat? varmuuskopiot?
```

### Turvallisuus ja Palomuuri

⚠️ **TÄRKEÄÄ:** Jos Raspberry Pi on kytketty internetiin, noudata näitä ohjeita!

#### 1. Päivitä Järjestelmä

```bash
sudo apt-get update && sudo apt-get upgrade -y && sudo reboot
```

#### 2. Vaihda Oletussalasana

```bash
passwd  # Min. 12 merkkiä
```

#### 3. Aktivoi Palomuuri (UFW)

```bash
sudo apt-get install -y ufw
sudo ufw allow 22/tcp comment 'SSH'
# Vaihda 192.168.1.0/24 omaan aliverkkoon!
sudo ufw allow from 192.168.1.0/24 to any port 5000 proto tcp comment 'Dashboard (LAN only)'
sudo ufw allow from 192.168.1.0/24 to any port 8080 proto tcp comment 'Simple Viewer (LAN only)'
sudo ufw enable
sudo ufw status verbose
```

#### 4. SSH-turvallisuus

```bash
sudo nano /etc/ssh/sshd_config
# Aseta: PermitRootLogin no
# Aseta: PasswordAuthentication yes (tai 'no' jos SSH-avaimet)
sudo systemctl restart ssh
```

**SSH-avaimet (suositeltu):**
```bash
# PC:ltä (ei Raspberry Pi:ltä!)
ssh-keygen -t ed25519 -C "your_email@example.com"
ssh-copy-id pi@raspberrypi.local
```

#### 5. Fail2ban (Brute-force suojaus)

```bash
sudo apt-get install -y fail2ban
sudo nano /etc/fail2ban/jail.local
```

Sisältö:
```ini
[DEFAULT]
bantime = 3600
findtime = 600
maxretry = 5

[sshd]
enabled = true
port = 22
logpath = %(sshd_log)s
```

```bash
sudo systemctl enable fail2ban && sudo systemctl start fail2ban
```

#### 6. Web-palveluiden Autentikointi

⚠️ **KRIITTINEN:** Oletuksena Web Dashboard ja Discord bot **eivät vaadi kirjautumista!**

- Käytä palomuurisääntöjä rajoittamaan pääsy vain paikalliseen verkkoon
- ÄLÄ avaa portteja 5000/8080 internetiin ilman autentikointia
- Autentikointi (käyttäjätunnus/salasana) on suunnitteilla — ks. [FUTURE_DEVELOPMENT.md](../FUTURE_DEVELOPMENT.md)

| Ominaisuus | Paikallinen (LAN) | Internetiin Avattu |
|------------|-------------------|---------------------|
| Palomuuri | Suositeltu | **PAKOLLINEN** |
| Autentikointi | Valinnainen | **PAKOLLINEN** |
| HTTPS | Ei tarvita | **PAKOLLINEN** |
| Vahva salasana | Suositeltu | **PAKOLLINEN** |
| SSH-avaimet | Suositeltu | **PAKOLLINEN** |
| Fail2ban | Valinnainen | **PAKOLLINEN** |
| VPN | Ei tarvita | **SUOSITELTU** |

**Suositus:** Jos tarvitset etäkäyttöä, käytä VPN:ää (esim. WireGuard, OpenVPN).

---

## Vianmääritys

### Yleisiä ongelmia

#### 1. Laite ei käynnisty

**Oireet:** Ei lokeja Serial Monitorissa, ei LED-vilkkumista.

**Ratkaisut:**
- Tarkista USB-kaapeli (kokeile toista kaapelia)
- Paina RST-nappia tai irrota + kytke USB
- Tarkista että PlatformIO latasi firmwaren onnistuneesti (ei virheitä)
- XIAO: Tarkista että Wio-SX1262 on kunnolla kiinni B2B-liittimessä

#### 2. LoRa ei lähetä/vastaanota

**Oireet:** `TX failed` tai `No messages received`.

**Ratkaisut:**
- Tarkista että `LORA_NETWORK_ID` on **sama molemmissa laitteissa**
- Tarkista osoitteet: `LORA_ADDRESS_RECEIVER` ja `LORA_ADDRESS_SENDER`
- Tarkista että laitteet ovat samalla SF:llä (`LORA_SPREADING_FACTOR`)
- XIAO: Tarkista että `USE_XIAO_SX1262 true`
- Tarkista RSSI-arvot: jos < -120 dBm → liian kaukana tai esteitä
- Kokeile lähentää laitteita (< 5 m) testausta varten

#### 3. TFT-näyttö ei päivity

**Oireet:** TFT näyttää vanhaa dataa tai "No data".

**Ratkaisut:**
- Tarkista UART-kytkentä: D4 (GPIO 5) → TFT RX, D5 (GPIO 6) → TFT TX
- Tarkista rooli: TFT aktivoituu vain RECEIVER-laitteelle (jumpperi D1↔D2)
- Tarkista `DISPLAY_MODE 2` config.h:ssa
- Tarkista Serial Monitor: pitäisi näkyä `RECEIVER -> TFT display (UART)` käynnistyksessä
- Tarkista TFT-näytön baudrate (115200)

#### 4. LCD näyttää roskaa

**Oireet:** LCD näyttää vääriä merkkejä tai on tyhjä.

**Ratkaisut:**
- Tarkista I2C-osoite: kokeile `0x27` tai `0x3F`
- Tarkista kytkentä: D4 (GPIO 5) → SDA, D5 (GPIO 6) → SCL
- Säädä kontrastipotentiometriä (LCD:n takana sininen ruuvi)

#### 5. LM393 ei tunnista LED-välähdyksiä

**Oireet:** `LED:0` aina, vaikka savuilmaisimen LED vilkkuu.

**Ratkaisut:**
- Tarkista kytkentä: D2 (GPIO 3) → AO, D3 (GPIO 4) → DO
- Säädä sinistä potentiometriä (herkkyys)
- Kytke LCD ja käytä `LCD_LAYOUT_NUMBER=5` → näet analogiarvot reaaliajassa
- Kirkas valo → matala ADC-arvo (< 1000), pimeä → korkea (> 3000)
- Tarkista `ENABLE_LIGHT_DETECTION true` ja `LIGHT_THRESHOLD 1000`

#### 6. Mesh-verkko ei välitä viestejä

**Oireet:** RELAY vastaanottaa mutta ei välitä.

**Ratkaisut:**
- Tarkista jumpperi: D0 ↔ D1 (RELAY-rooli)
- Tarkista `ENABLE_MESH_NETWORK true` ja `MESH_MAX_HOPS 3` (> 0)
- Tarkista Serial Monitor: pitäisi näkyä `>>> RELAY MODE`
- Tarkista että RSSI > `MESH_RELAY_MIN_RSSI` (-100 dBm)
- Katso [deployment/docs/STATE_MACHINE.md](../deployment/docs/STATE_MACHINE.md) yksityiskohtaiseen debuggaukseen

#### 7. Raspberry Pi ei vastaanota dataa

**Oireet:** Dashboard tyhjä, ei dataa tietokannassa.

**Ratkaisut:**
- Tarkista USB-kaapeli: `ls -la /dev/ttyUSB* /dev/ttyACM*`
- Tarkista servicet: `bash deployment/scripts/check_status.sh`
- Tarkista lokit: `sudo journalctl -u zignalmeister-logger -n 50`
- Tarkista baudrate: 115200 (sekä ESP32 että Python-skripti)
- Testaa manuaalisesti: `cat /dev/ttyUSB0` (pitäisi näkyä CSV-dataa)

### Pinnit (XIAO ESP32S3) — Pikakatsaus

| Pinni | GPIO | SENDER | RECEIVER | RELAY |
|-------|------|--------|----------|-------|
| D0 | 1 | (vapaa) | (vapaa) | jumpperi → D1 |
| D1 | 2 | (vapaa) | jumpperi → D2 | jumpperi → D0 |
| D2 | 3 | LM393 AO | jumpperi → D1 | (vapaa) |
| D3 | 4 | LM393 DO | (vapaa) | (vapaa) |
| D4 | 5 | LCD SDA | TFT TX | LCD SDA |
| D5 | 6 | LCD SCL | TFT RX | LCD SCL |

**Muista:** D8-D10 ja B2B varattu LoRa:lle (älä käytä).

### Reset-vaihtoehdot

| Tyyppi | Miten | Käyttö |
|--------|-------|--------|
| **K1-nappi** | Paina K1 >100 ms | Uudelleenkäynnistys |
| **K1 deep sleepissä** | Paina K1 | EXT1-herätys → käynnistys |
| **Software reset** | `ESP.restart()` | Normaali uudelleenkäynnistys koodissa |
| **Hardware reset** | RST-nappi → GND | Takuuvarma reset (toimii aina) |
| **Watchdog reset** | Automaattinen (10s timeout) | Jos koodi jumiutuu |
| **Power cycle** | Irrota + kytke USB | Täysi nollaus |

### Serial Monitor ei näytä mitään

**Ratkaisut:**
- Tarkista baudrate: **115200**
- Paina RST-nappia → käynnistyslokit näkyvät
- Vaihda USB-kaapelia (jotkin kaapelit eivät tue dataa)
- Tarkista että oikea portti valittuna (`/dev/ttyUSB0` tai `COM3`)

### Firmware-flashaus epäonnistuu (COM port busy / Access denied)

**Syy:** Serial Monitor tai muu ohjelma pitää COM-porttia varattuna.

**Ratkaisut:**
1. **Sulje Serial Monitor** ennen flashausta
2. **Bootloader-moodi** (ohittaa firmwaren kokonaan):
   - Pidä **BOOT-nappi** pohjassa (pieni nappi laudan reunassa)
   - Paina **RESET** (K1) kerran samalla
   - Vapauta BOOT → laite on bootloader-moodissa
   - Flashaa normaalisti
3. Jos portti ei näy: vaihda USB-kaapelia (datan tukevia)

---

## Lisätiedot

### Dokumentaatio

| Tiedosto | Kuvaus |
|----------|--------|
| [docs/LAITEKUVAUS.md](../docs/LAITEKUVAUS.md) | Tekninen kuvaus, laitteisto, algoritmit |
| [deployment/docs/STATE_MACHINE.md](../deployment/docs/STATE_MACHINE.md) | Mesh-verkon relay-tilakone |
| [TFT_firmware/README.md](../TFT_firmware/README.md) | TFT-näytön kustomointi ja API |
| [deployment/monitoring/DISCORD_AUTH_SETUP.md](../deployment/monitoring/DISCORD_AUTH_SETUP.md) | Discord bot -setup |
| [docs/OTA_PLAN.md](../docs/OTA_PLAN.md) | OTA firmware-päivityksen suunnitelma |
| [FUTURE_DEVELOPMENT.md](../FUTURE_DEVELOPMENT.md) | Kehityssuunnitelma ja prioriteetit |
| [CHANGELOG.md](../CHANGELOG.md) | Versiohistoria |

### Ohjelmiston rakenne

```
zignalmeister/
├── firmware/
│   ├── firmware.ino              # Pääohjelma (~2,600 riviä)
│   ├── config.h                  # Kaikki asetukset täällä
│   ├── structs.h                 # Jaetut rakenteet
│   ├── platform_hal.h            # Alustatunnistus, pin-mappaus
│   ├── debug_hal.h               # Debug-reititys (USB CDC / UART)
│   ├── display_hal.h             # Näyttöabstraktio (LCD/TFT/Serial)
│   ├── lora_hal.h                # LoRa-abstraktio (SX1262 / RYLR896)
│   ├── lora_handler.h            # RYLR896 AT-komento-ajuri
│   ├── lora_sx1262_handler.h     # SX1262 RadioLib-ajuri
│   ├── non_blocking_state_v2.h   # Relay-tilakone (non-blocking, 4 viestin jono)
│   ├── fire_alarm_detector.h     # Valo- ja äänitunnistus
│   ├── system_monitoring.h       # Terveys, RSSI-tilastot, watchdog
│   ├── power_management.h        # CPU-skaalaus, unimekanismit
│   ├── lcd_display.h             # LCD 16x2 I2C
│   ├── display_sender.h          # TFT UART-näyttö
│   └── DisplayClient.h           # Näyttöprotokolla
├── deployment/
│   ├── scripts/
│   │   ├── quick_install.sh      # One-command asennus
│   │   ├── check_status.sh       # Terveystarkistus
│   │   └── fix_services.sh       # Ongelmien korjaus
│   ├── dashboard/app.py          # Web-UI (Flask)
│   ├── monitoring/               # Discord bot
│   └── docs/                     # Deployment-dokumentaatio
├── data/
│   └── data_logger_v2.py         # USB serial → SQLite
└── docs/                         # Tekniset dokumentit
```

### Versiohistoria

- **v2.7.0 (29.01.2026):** Modulaarinen HAL-arkkitehtuuri, dual-platform (SX1262+RYLR896), crash report -järjestelmä, DISPLAY_MODE=2 automaattinen näyttövalinta
- **v2.6.0 (15.01.2026):** Mesh-verkko uudelleenaktivoitu non-blocking state machine -toteutuksella
- **v2.5.0:** XIAO ESP32S3 + SX1262 pääalustaksi
- **v2.4.0:** LM393 valotunnistus EN 54-5 -yhteensopiva
- **v2.3.0:** Raspberry Pi deployment (quick_install.sh)
- **v2.0.0:** TFT UART-näyttö, mesh-topologia

### Muistinkäyttö

Tyypillinen muistinkäyttö (XIAO ESP32S3, PSRAM käytössä):

```
RAM:       ~80 KB / 512 KB (16%)
PSRAM:     ~200 KB / 8 MB (2.5%)
Flash:     ~1.2 MB / 8 MB (15%)
```

**PSRAM-käyttö:**
- Mesh deduplication buffer (20 hashia × 4 bytes)
- Relay message queue (4 viestia × 256 bytes)
- Display output buffer (512 bytes)

### Tuki ja Palaute

- **GitHub Issues:** https://github.com/YOUR_REPO/zignalmeister/issues
- **Kehityssuunnitelma:** [FUTURE_DEVELOPMENT.md](../FUTURE_DEVELOPMENT.md)

---

## Yhteenveto

**Tärkeimmät pointit:**
1. ✅ Sama firmware kaikkiin laitteisiin → rooli tunnistetaan automaattisesti
2. 📡 LoRa SF12 → max kantama (useita kilometrejä)
3. 🔄 Mesh-verkko → RELAY-solmut laajentavat kantamaa
4. 💡 LM393 valotunnistus → savuilmaisimen LED-välähdykset
5. 💾 Raspberry Pi → automaattinen datan tallennus + web-dashboard

**Aloita tästä:** [Laitteisto](#laitteisto) → [Pikaohje](#pikaohje)

**Ongelma?** → [Vianmääritys](#vianmääritys)

**Tekninen kuvaus?** → [LAITEKUVAUS.md](../docs/LAITEKUVAUS.md)

---

*Päivitetty: 2026-03-19*
*Versio: 2.7.0*
