# POG Sensor

<img src="assets/brand/icon.png" alt="POG Sensor icon" width="96">

[![CI & Release](https://github.com/POG-Projects/pog-os-sensor/actions/workflows/ci-release.yml/badge.svg)](https://github.com/POG-Projects/pog-os-sensor/actions/workflows/ci-release.yml)
[![Latest release](https://img.shields.io/github/v/release/POG-Projects/pog-os-sensor)](https://github.com/POG-Projects/pog-os-sensor/releases/latest)

Firmware ESP32 dédié aux capteurs de POG Home. Il détecte automatiquement les
modules compatibles sur un même bus I²C :

- **BME280 / BMP280 / BME680** pour la température et la pression, avec
  humidité sur les BME et résistance de gaz brute sur le BME680 ;
- **SHT30 / SHT31 / SHT35** et **AHT10 / AHT20** pour la température et
  l’humidité ;
- **SCD40 / SCD41** pour le CO₂, la température et l’humidité ;
- **VEML7700** pour la luminosité ambiante en lux ;
- **SGP40** pour l’indice adaptatif de composés organiques volatils (VOC) ;
- **HLK-LD2410B** pour la présence immobile et le mouvement ;
- **HLK-LD2450** pour suivre jusqu’à trois cibles en coordonnées X/Y ;
- disponibilité du capteur et puissance du signal Wi‑Fi en diagnostic ;
- jusqu’à quatre LED RGB WS2812B optionnelles pour le statut, la présence et
  l’éclairage pilotable.

Plusieurs modules peuvent fonctionner ensemble. Par exemple, un BMP280 et un
SHT31 donnent pression + température + humidité, tandis qu’un SCD41 et un
VEML7700 ajoutent le CO₂ et la luminosité. POG Home ne reçoit qu’une entité
stable par grandeur, quelle que soit la combinaison physique.

Les mesures restent locales : l’ESP32 découvre POG Home par mDNS, est adopté
avec le protocole `pogdev`, puis publie uniquement dans son espace MQTT protégé.
Il n’utilise aucun cloud.

Documentation complémentaire :

- [schémas de câblage ESP32-C3 et harnais](docs/WIRING.md) ;
- [datasheet des couleurs et codes d’erreur LED](docs/STATUS_LED.md).

## Matériel

Le firmware sonde les adresses I²C usuelles sans configuration manuelle :

| Famille | Mesures | Adresses |
|---|---|---|
| BME280 / BMP280 / BME680 | température, pression, humidité et gaz selon modèle | `0x76`, `0x77` |
| SHT3x | température, humidité | `0x44`, `0x45` |
| AHT10 / AHT20 | température, humidité | `0x38` |
| SCD40 / SCD41 | CO₂, température, humidité | `0x62` |
| VEML7700 | luminosité | `0x10` |
| SGP40 | indice VOC | `0x59` |

| Carte | SDA par défaut | SCL par défaut | Cible PlatformIO |
|---|---:|---:|---|
| ESP32-C3 SuperMini | GPIO 6 | GPIO 5 | `esp32c3` |
| ESP32-S3 DevKit | GPIO 8 | GPIO 9 | `esp32s3` |
| ESP32 DevKit classique | GPIO 21 | GPIO 22 | `esp32dev` |

Le [guide de câblage](docs/WIRING.md) détaille chaque module pris en charge,
les deux radars UART, la lampe WS2812B et le harnais à masse commune.

Branchement commun des modules I²C :

| Module | ESP32 |
|---|---|
| VIN / VCC | 3,3 V |
| GND | GND |
| SDA | SDA de la carte |
| SCL | SCL de la carte |

Utiliser 3,3 V sauf si le breakout indique explicitement qu’il intègre un
régulateur et une adaptation de niveaux. Les broches peuvent être changées dans
le portail sans recompiler.

### Radars UART sur ESP32-C3 SuperMini

Les deux radars peuvent fonctionner simultanément grâce aux deux UART
matériels. Ils utilisent 256000 bauds par défaut et sont reconnus d’après leurs
trames, indépendamment du connecteur choisi.

| Signal | Module | ESP32-C3 |
|---|---|---|
| Radar A TX | TX | GPIO 3 (RX ESP) |
| Radar A RX | RX | GPIO 4 (TX ESP) |
| Radar B TX | TX | GPIO 20 (RX ESP) |
| Radar B RX | RX | GPIO 21 (TX ESP) |
| Alimentation | VCC | 5 V |
| Masse | GND | GND commune |

Ne jamais relier ensemble les sorties TX des deux radars. Les rails 3,3 V et
5 V du harnais restent séparés ; seule la masse est commune. Les modules radar
sont alimentés en 5 V, mais leurs signaux UART sont en logique 3,3 V. Les quatre
GPIO radar sont modifiables dans les réglages avancés du portail.

### LED RGB de statut

Un segment de quatre LED découpé d’un bandeau **WS2812B 5 V** se branche avec
une seule ligne de données : `DIN` sur GPIO 7 pour le C3 SuperMini, `5V` et
masse commune. Une résistance série de 330 Ω et un condensateur de 100 µF près
du segment sont recommandés. La luminosité du firmware est limitée à 38 % de
la puissance nominale. POG Home expose la rangée comme une lumière RGB et peut
configurer l’allumage automatique déclenché par les radars.

La signification complète des couleurs, animations et erreurs est décrite dans
la [datasheet de la LED de statut](docs/STATUS_LED.md). L’option **Lampe
adressable installée** doit être activée dans les réglages avancés. Elle est
désactivée par défaut : un appareil sans segment LED ne publie aucune entité
lumière dans POG Home et ne pilote pas GPIO 7.

## Compiler et flasher

```bash
pio run -e esp32c3
pio run -e esp32c3 -t upload
pio device monitor -b 115200
```

Remplacer `esp32c3` par `esp32s3` ou `esp32dev` selon la carte.

## Première mise en route

1. Flasher l’ESP32 puis rejoindre le Wi‑Fi `POG-Sensor-Setup`.
2. Créer le mot de passe administrateur demandé par le portail.
3. Le portail captif s’ouvre automatiquement sur iOS, macOS, Android et
   Windows. Ouvrir `http://192.168.4.1` seulement s’il ne s’affiche pas.
4. Choisir un réseau dans le scanner Wi‑Fi, puis saisir son mot de passe et le
   nom du capteur. Les GPIO et les modules optionnels se configurent dans les
   réglages avancés.
5. Après redémarrage, ouvrir POG Home et adopter l’appareil
   `ESP-SENSOR-<MAC>`.
6. Les entités apparaissent automatiquement sous la catégorie **Climat**.

Si POG Home ne peut pas être découvert en mDNS, son IP peut être saisie comme
adresse de secours dans le portail. Une configuration incorrecte du Wi‑Fi
réactive automatiquement le point d’accès après 20 secondes.

## Réglages du web panel

Le dashboard local regroupe la configuration sans nécessiter de recompilation :

| Section | Réglages disponibles |
|---|---|
| Réseau | scanner Wi‑Fi, SSID et mot de passe |
| POG Home | nom de l’appareil, adresse de secours et port |
| Bus environnement | GPIO SDA/SCL, intervalle de mesure et correction de température |
| Radars | RX/TX des deux ports UART ; LD2410B et LD2450 sont auto-détectés |
| Lampe témoin | présence du module, couleur, luminosité, automatisme radar et maintien de 0 à 300 s |
| Maintenance | état OTA, recherche de release, installation automatique ou fichier `.bin`, redémarrage |
| Sécurité | changement du mot de passe et verrouillage immédiat du dashboard |

La broche et le nombre de pixels de la lampe sont affichés selon la carte. Si
la lampe est déclarée absente, le bloc de contrôle reste masqué, son GPIO n’est
pas piloté et POG Home ne reçoit aucune entité lumière.

## Sécurité du dashboard

Le dashboard reprend le modèle de sécurité de POG AirPlay :

- mot de passe administrateur obligatoire à la première ouverture ;
- seule son empreinte SHA‑256 est conservée dans la NVS ;
- jetons de session aléatoires de 128 bits, conservés en mémoire et valables
  24 heures ;
- comparaison des secrets en temps constant ;
- verrouillage de 30 secondes après cinq mots de passe incorrects ;
- protection du statut, du scanner Wi‑Fi, de la configuration, de l’OTA et du
  redémarrage par l’en-tête `X-Auth-Token` ;
- changement du mot de passe depuis le panel avec fermeture des autres
  sessions ;
- jetons invalidés à chaque redémarrage et bouton de verrouillage immédiat.

La page HTML et les endpoints d’authentification restent accessibles afin de
présenter l’écran de connexion. En cas d’oubli du mot de passe, effacer la NVS
en reflashant complètement la carte réinitialise l’accès administrateur.

## Comportement des mesures

La lecture a lieu toutes les 30 secondes par défaut. Une mise à jour MQTT est
envoyée dès qu’une mesure varie d’au moins :

- `0,2 °C` ;
- `1 %` d’humidité ;
- `1 hPa` ;
- `25 ppm` de CO₂ ;
- `10 lx` ;
- `5 points` d’indice VOC ;
- `2 kΩ` de résistance de gaz ;
- `5 dBm` pour le signal Wi‑Fi.

Un état complet est aussi republié toutes les cinq minutes. Le SCD4x mesure en
continu avec son intervalle natif de cinq secondes ; le firmware prélève sa
dernière valeur au cycle suivant. Le mode de mesure forcé du BME/BMP280 réduit
son auto-échauffement entre deux lectures. Une correction de température de
`−10 à +10 °C` est configurable depuis le portail.

Le SGP40 est interrogé chaque seconde, comme l’exige l’algorithme officiel
Sensirion. La température et l’humidité disponibles sur le bus compensent sa
mesure ; à défaut, le firmware utilise 25 °C et 50 %. La valeur publiée est un
indice VOC de 1 à 500, dont 100 représente la composition moyenne apprise sur
24 heures. Ce composant ne mesure pas directement le CO₂ et aucune valeur eCO₂
artificielle n’est publiée.

Le LD2450 envoie dix trames par seconde. POG Sensor publie la présence, le
mouvement, le nombre de cibles et, pour chacune des trois cibles possibles, sa
position, sa vitesse radiale et sa résolution. POG Home dessine ces positions
et conserve une courte trace visuelle pour matérialiser les trajectoires.

## Mises à jour OTA

Une fois connecté au Wi-Fi, POG Sensor vérifie automatiquement la dernière
release officielle après quelques secondes, puis toutes les six heures.
L’installation reste confirmée depuis l’interface `http://pogsensor.local` :

1. le firmware télécharge `manifest.json` en HTTPS ;
2. il sélectionne uniquement l’image correspondant à sa carte ;
3. il refuse un nom d’asset ou une empreinte SHA-256 invalide ;
4. il écrit l’image dans le slot OTA inactif ;
5. il vérifie l’empreinte complète avant activation et redémarrage.

Le portail permet aussi de relancer la vérification, consulter les notes de
release et installer manuellement un fichier `.bin`.

Les ESP32-C3 et ESP32 classiques utilisent désormais `min_spiffs.csv`, avec
deux slots applicatifs d’environ 1,9 Mo. Une carte flashée avec l’ancienne table
de partitions doit recevoir une dernière mise à jour par USB avec l’image
`merged-<carte>.bin`; les mises à jour suivantes pourront être faites en OTA.
Les données NVS (Wi-Fi, réglages et identité POG Home) gardent les mêmes offsets
et sont préservées pendant cette migration.

## Intégration POG Home

Le firmware implémente le protocole `pogdev` v1 :

- découverte `_poghome._tcp` et adresse manuelle de secours ;
- annonce et adoption sans secret partagé ;
- stockage NVS avec relecture des identifiants MQTT ;
- `hello`, `state`, `status` et Last Will retenus ;
- nouvelle adoption automatique si le compte MQTT a été supprimé ;
- identités d’entités stables : `temperature`, `humidity`, `pressure`, `co2`,
  `illuminance`, `voc_index`, `gas_resistance`, `presence`, `motion`,
  `radar_tracking`, `sensor_status` et `wifi_signal` ; seules les entités prises
  en charge par les composants détectés sont annoncées.

Les mesures et diagnostics sont en lecture seule. Lorsque la lampe est
installée, ses trois entités acceptent les commandes de puissance, couleur,
luminosité, automatisme et temporisation. Le champ `local_rules` est réservé
dans le descripteur pour rester compatible avec les futures versions du
protocole.

## Test hôte

La politique de détection des variations et les décodeurs des deux radars sont
testables sans ESP32 :

```bash
c++ -std=c++17 -Wall -Wextra -Werror -I include \
  test_host/test_sampling_policy.cpp -o /tmp/pog-sensor-test
/tmp/pog-sensor-test
c++ -std=c++17 -Wall -Wextra -Werror -I include \
  test_host/test_radar_protocol.cpp -o /tmp/pog-radar-test
/tmp/pog-radar-test
```

## CI et releases

Le workflow GitHub Actions suit le même contrat de publication que PogLight et
POG AirPlay :

- une pull request teste les politiques, les protocoles radar et le JavaScript
  embarqué, puis compile les trois cartes sans créer de release ;
- un push de code sur `main`, ou un lancement manuel, publie une release
  GitHub ;
- une modification limitée à la documentation ne déclenche pas la matrice.

Chaque release contient :

- `firmware-<carte>.bin`, l’image applicative destinée aux futures mises à jour
  OTA et utilisée par le système de mise à jour intégré ;
- `merged-<carte>.bin`, l’image complète pour le premier flash USB ;
- `manifest.json`, le catalogue des cartes avec tailles et empreintes SHA-256 ;
- `SHA256SUMS`, les empreintes de tous les binaires et du manifeste.

La version initiale est définie dans `version.txt` puis injectée dans chaque
binaire pendant la compilation. Si la release existe déjà, la CI incrémente
automatiquement le patch, met à jour `version.txt`, crée le tag `vX.Y.Z` et
publie la nouvelle version comme release courante.
