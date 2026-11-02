# Smart-Laundry-Color-Classifier
Arduino Project of a smart color classifier for laundry

Progetto per l'insegnamento di **Laboratorio di Sensori** — Università degli Studi di Bergamo,
Corso di Laurea in Ingegneria Informatica.

- **Candidato:** Andrea Vaerini (1067398)
- **Docenti:** Prof. Gianluca Traversi, Prof. Paolo Lazzaroni

## 📄 Relazione

**[Apri la relazione in PDF](report/main.pdf)** — sorgente LaTeX in [`report/main.tex`](report/main.tex).

## Il progetto

Sistema embedded TinyML su **Arduino Nano 33 BLE Sense** che classifica il colore dei capi
in 8 classi (`BLACK`, `WHITE`, `RED`, `BLUE`, `GREEN`, `PINK`, `PURPLE`, `YELLOW`) e invia
il risultato a uno smartphone via **Bluetooth Low Energy**, usando solo i sensori integrati
sulla scheda.

- **Sensore:** APDS-9960 integrato (canali RGBC + prossimità IR)
- **Pre-elaborazione:** coordinate cromatiche normalizzate `r = R/(R+G+B)`, `g`, `b`,
  per rendere le letture indipendenti dall'intensità della luce
- **Proximity Gate:** il sensore IR distingue un capo nero dall'assenza di capi;
  senza capo il dispositivo notifica `NESSUN_CAPO`
- **Modello:** rete neurale densa (3 → 16 → 8) addestrata con Edge Impulse ed esportata
  con EON Compiler in float32
- **BLE:** servizio `19B10000-E8F2-537E-4F6C-D104768A1214`, caratteristica Notify
  `19B10001-E8F2-537E-4F6C-D104768A1214` con payload del tipo `BLUE (94%)`

## Struttura del repository

```
├── firmware/SmartLaundry/   Sketch Arduino (SmartLaundry.ino)
└── report/
    ├── main.tex             Sorgente LaTeX della relazione
    ├── main.pdf             Relazione compilata
    ├── data/                Misure del sensore (luce diretta, luce ambiente, ombra)
    └── img/
        ├── ble/             Screenshot del collaudo BLE con nRF Connect
        └── ei/              Screenshot di Edge Impulse (matrice di confusione, feature explorer, ...)
```

## Compilazione

- **Firmware:** Arduino IDE con core *Arduino Mbed OS Nano Boards* 1.1.6, librerie
  `Arduino_APDS9960`, `ArduinoBLE` e la libreria Edge Impulse
  `Smart-Laundry-Color-Classifier_inferencing`.
- **Relazione:** `latexmk -pdf main.tex` dalla cartella `report/`.
