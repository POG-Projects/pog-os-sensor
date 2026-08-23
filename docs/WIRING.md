# Câblage — ESP32-C3 SuperMini et modules POG Sensor

Ce guide décrit le profil matériel par défaut `esp32c3`. Les GPIO peuvent être
modifiés depuis les réglages avancés du dashboard, sauf la sortie de la lampe
qui est définie par la cible PlatformIO.

## Vue d’ensemble du harnais

```text
ESP32-C3 SuperMini
├── rail 3,3 V ── capteurs I²C
├── rail 5 V   ── radars LD2410B / LD2450 et WS2812B
├── GND commun ── tous les modules
├── GPIO 6 SDA ── tous les capteurs I²C en parallèle
├── GPIO 5 SCL ── tous les capteurs I²C en parallèle
├── UART A ────── GPIO 3 RX / GPIO 4 TX
├── UART B ────── GPIO 20 RX / GPIO 21 TX
└── GPIO 7 ────── DIN de la lampe WS2812B
```

Le harnais distribue l’alimentation en parallèle : il ne faut pas faire passer
le courant d’un module au suivant à travers sa carte. Les rails 3,3 V et 5 V
restent séparés et toutes les masses sont communes. Couper l’alimentation avant
d’ajouter ou de retirer un module.

## Capteurs I²C

Tous les capteurs ci-dessous partagent les quatre mêmes connexions :

| Broche du module | ESP32-C3 | Rôle |
|---|---:|---|
| `VCC` / `VIN` | 3,3 V | alimentation |
| `GND` | GND | masse commune |
| `SDA` | GPIO 6 | données I²C |
| `SCL` | GPIO 5 | horloge I²C |

| Module | Adresse(s) | Valeurs publiées | Remarque |
|---|---|---|---|
| BME280 | `0x76`, `0x77` | température, humidité, pression | changer l’adresse par la broche SDO du breakout si nécessaire |
| BMP280 | `0x76`, `0x77` | température, pression | aucune mesure d’humidité |
| BME680 | `0x76`, `0x77` | température, humidité, pression, résistance de gaz | ne peut pas partager l’adresse d’un BME/BMP sans modification de SDO |
| SHT30 / SHT31 / SHT35 | `0x44`, `0x45` | température, humidité | compatible en complément d’un BMP280 |
| AHT10 / AHT20 | `0x38` | température, humidité | adresse fixe |
| SCD40 / SCD41 | `0x62` | CO₂ réel, température, humidité | laisser les ouvertures du boîtier respirer |
| VEML7700 | `0x10` | luminosité ambiante | orienter le capteur vers la zone à mesurer |
| SGP40 | `0x59` | indice VOC adaptatif | ne mesure ni CO₂ ni eCO₂ |

Le composant SGP40 nu accepte uniquement une alimentation basse tension. Le
profil POG utilise donc 3,3 V ; ne brancher un breakout en 5 V que si sa propre
documentation confirme la présence d’un régulateur et d’une adaptation I²C.

## Radar A — LD2410B

| LD2410B | ESP32-C3 | Sens du signal |
|---|---:|---|
| `VCC` | 5 V | alimentation du radar |
| `GND` | GND | masse commune |
| `TX` | GPIO 3 (`RX` ESP) | radar vers ESP32 |
| `RX` | GPIO 4 (`TX` ESP) | ESP32 vers radar |

Le LD2410B publie le mouvement, la présence immobile et les distances. Son
Bluetooth éventuel n’est pas utilisé par le firmware : la liaison principale
reste l’UART à 256000 bauds.

## Radar B — LD2450

| LD2450 | ESP32-C3 | Sens du signal |
|---|---:|---|
| `VCC` | 5 V | alimentation du radar |
| `GND` | GND | masse commune |
| `TX` | GPIO 20 (`RX` ESP) | radar vers ESP32 |
| `RX` | GPIO 21 (`TX` ESP) | ESP32 vers radar |

Le LD2450 transmet jusqu’à trois cibles avec position X/Y, vitesse et
résolution. Les sorties TX des deux radars ne doivent jamais être reliées entre
elles : chaque radar possède son propre RX côté ESP32.

Les connecteurs A et B ne sont pas réservés à un modèle. Le firmware reconnaît
automatiquement LD2410B ou LD2450 d’après les trames reçues.

Au démarrage, le firmware écoute d’abord le RX attendu, puis la broche TX sans
jamais la piloter pendant le test. Une trame LD2410/LD2450 valide sur la seconde
broche produit le diagnostic `RX/TX inversés` et la réception est compensée en
logiciel. L’absence de trame sur les deux broches reste indéterminée : vérifier
alors le 5 V, la masse commune, le câblage et le débit du radar.

## Lampe témoin — quatre WS2812B

| Segment WS2812B | ESP32-C3 |
|---|---:|
| `5V` | rail 5 V |
| `GND` | GND commun |
| `DIN` | GPIO 7 via une résistance de 330 Ω |
| `DOUT` du dernier pixel | non connecté |

Ajouter un condensateur d’environ 100 µF entre 5 V et GND près du segment.
Respecter la flèche `DIN → DOUT`. Pour un câble DATA long ou bruité, utiliser
un adaptateur de niveau 74AHCT125 ou 74HCT14.

La lampe est désactivée par défaut. Après câblage, ouvrir **Réglages avancés →
Lampe témoin**, cocher **Lampe adressable installée**, choisir la couleur, la
luminosité et le délai de présence, puis enregistrer. Le C3 redémarre pour
réserver GPIO 7 et publier les contrôles dans POG Home.

## Vérification avant mise sous tension

- aucun fil 5 V ne rejoint le rail 3,3 V ;
- tous les modules partagent la même masse ;
- chaque `TX` radar rejoint un `RX` ESP distinct ;
- SDA et SCL ne sont pas inversés ;
- la lampe est raccordée sur `DIN`, jamais sur `DOUT` ;
- les GPIO 3, 4, 5, 6, 7, 20 et 21 ne sont pas réutilisés ailleurs avec le
  profil C3 par défaut.

Après démarrage, le dashboard affiche les modules détectés. Un module absent
indique généralement un problème d’alimentation, de masse, d’adresse I²C ou de
croisement TX/RX.
