# LoraMeister - Testipäiväkirja

## Testisuunnitelma

### Tarvittava laitteisto
- 2x ESP32 DevKit V1
- 2x RYLR890 (tai RYLR896) LoRa-moduuli
- 2x LCD 16x2 I2C (osoite 0x27)
- 1x GY MAX4466 mikrofonimmoduuli
- Hyppylankoja (jumppereita roolin valintaan)
- USB-kaapelit (virta + serial monitor)

### Kytkennät

**SENDER (ei jumpperia GPIO16-17):**
```
RYLR890 TX  → GPIO25
RYLR890 RX  → GPIO26
RYLR890 VCC → 3.3V
RYLR890 GND → GND
LCD SDA     → GPIO21
LCD SCL     → GPIO22
LCD VCC     → 5V (VIN)
LCD GND     → GND
MAX4466 OUT → GPIO34
MAX4466 VCC → 3.3V
MAX4466 GND → GND
```

**RECEIVER (jumpperi GPIO16 ↔ GPIO17):**
```
RYLR890 TX  → GPIO25
RYLR890 RX  → GPIO26
LCD SDA     → GPIO21
LCD SCL     → GPIO22
(jumpperi GPIO16 ↔ GPIO17)
```

---

## T1: Boot ja roolin tunnistus

**Tarkoitus:** Varmista että firmware käynnistyy ja rooli tunnistetaan oikein.

**Testit:**
- [ ] SENDER: GPIO16 kelluu → Serial: "MODE: SENDER"
- [ ] RECEIVER: GPIO16↔17 jumpperi → Serial: "MODE: RECEIVER"
- [ ] Auto-osoite generoidaan MAC:sta (Serial: "Auto Address: xxx")
- [ ] LCD näyttää "LoraMeister v0.2.0" käynnistyksessä
- [ ] LCD näyttää roolin ja osoitteen
- [ ] Watchdog ei laukea normaalissa käytössä (ei uudelleenkäynnistyksiä)

**Serial-monitori:** 115200 baud, tarkista banner ja init-viestit.

---

## T2: LoRa-yhteys (perusviestintä)

**Tarkoitus:** Varmista LoRa-lähetys ja vastaanotto.

**Testit:**
- [ ] SENDER lähettää paketteja (Serial: "[TX] #1 SEQ:1 ...")
- [ ] RECEIVER vastaanottaa (Serial: "[RX] SEQ:1 RSSI:-xx SNR:xx ...")
- [ ] RSSI ja SNR ovat järkeviä arvoja (-30 ... -120 dBm, SNR > -5)
- [ ] Viestilaskuri kasvaa molemmissa
- [ ] Lähetysväli ~3.5s (config: QS_LORA_SEND_INTERVAL_MS)

---

## T3: LCD-näytöt

**Tarkoitus:** Varmista LCD:n toiminta molemmissa laitteissa.

**Testit:**
- [ ] Layout 8 (mikrofoni): Näyttää "MIC: xxdB" ja palkkigraafi
- [ ] Layout 1: Näyttää "LoraMeister" ja RSSI
- [ ] Layout 2: Näyttää RSSI, SNR, packet loss
- [ ] Layout 4: Näyttää heap, sequence, LED state

**Layout vaihto:** Muuta `QS_LCD_LAYOUT` config.h:ssa ja uploadaa uudelleen.

---

## T4: MAX4466 mikrofoni

**Tarkoitus:** Varmista mikrofonidatan luku ja LoRa-lähetys.

**Testit:**
- [ ] SENDER: Serial näyttää MIC dB-arvon TX-viesteissä
- [ ] Hiljainen huone: ~30-40 dB
- [ ] Puhe läheltä: ~50-65 dB
- [ ] Kovaääninen musiikki/taputus: ~70-90 dB
- [ ] RECEIVER: näkee MIC-datan payloadissa
- [ ] RECEIVER LCD (Layout 8): näyttää etälaitteen dB-arvon
- [ ] Debug: aseta `QS_DEBUG_MIC true` → Serial: "[MIC] p2p=xxx dB=xx"

---

## T5: Bidirectional ACK

**Tarkoitus:** Varmista ACK-viestit ja RSSI-raportointi.

**Testit:**
- [ ] RECEIVER lähettää ACK:n joka 30. paketissa (config: QS_ACK_INTERVAL)
- [ ] SENDER vastaanottaa ACK:n (Serial: "[ACK] ✓ RX_RSSI:xx RX_SNR:xx")
- [ ] SENDER näyttää receiverin mittaaman RSSI:n (kaksisuuntainen)
- [ ] Stale RSSI nollataan jos ACK:ta ei tule 2x intervallin sisällä

---

## T6: Kill-switch

**Tarkoitus:** Varmista hätäkäynnistys.

**Testit:**
- [ ] Yhdistä GPIO13 ↔ GPIO14, pidä 3s → laite käynnistyy uudelleen
- [ ] Serial: "KILL-SWITCH ACTIVATED"
- [ ] Irrottaminen ennen 3s → ei uudelleenkäynnistystä

---

## T7: CSV data output

**Tarkoitus:** Varmista dataloggauksen toimivuus.

**Testit:**
- [ ] Serial: `DATA_CSV,V3,timestamp,role,rssi,snr,seq,count,mic_p2p,mic_db,heap`
- [ ] CSV tulee 2s välein (config: DATA_OUTPUT_INTERVAL)
- [ ] Arvot ovat järkeviä ja vastaavat LCD:n näyttämiä

---

## Testiloki

| Pvm | Testi | Tulos | Huomiot |
|-----|-------|-------|---------|
| | | | |
