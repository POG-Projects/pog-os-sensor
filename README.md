# POG Sensor

<img src="assets/brand/icon.png" alt="POG Sensor icon" width="96">

[![CI & Release](https://github.com/POG-Projects/pog-os-sensor/actions/workflows/ci-release.yml/badge.svg)](https://github.com/POG-Projects/pog-os-sensor/actions/workflows/ci-release.yml)
[![Latest release](https://img.shields.io/github/v/release/POG-Projects/pog-os-sensor)](https://github.com/POG-Projects/pog-os-sensor/releases/latest)

Firmware ESP32 dédié aux capteurs de POG Home. La première version prend en
charge automatiquement les **BME280** et **BMP280** sur bus I²C :

- température sur les deux modèles ;
- pression atmosphérique sur les deux modèles ;
- humidité lorsque le composant est un BME280 ;
- disponibilité du capteur et puissance du signal Wi‑Fi en diagnostic.

Les mesures restent locales : l’ESP32 découvre POG Home par mDNS, est adopté
avec le protocole `pogdev`, puis publie uniquement dans son espace MQTT protégé.
Il n’utilise aucun cloud.

## Matériel

Le firmware essaie automatiquement les adresses I²C `0x76`, puis `0x77`.

| Carte | SDA par défaut | SCL par défaut | Cible PlatformIO |
|---|---:|---:|---|
| ESP32-C3 SuperMini | GPIO 6 | GPIO 5 | `esp32c3` |
| ESP32-S3 DevKit | GPIO 8 | GPIO 9 | `esp32s3` |
| ESP32 DevKit classique | GPIO 21 | GPIO 22 | `esp32dev` |

Branchement du module BME/BMP280 :

| Module | ESP32 |
|---|---|
| VIN / VCC | 3,3 V |
| GND | GND |
| SDA | SDA de la carte |
| SCL | SCL de la carte |

Utiliser 3,3 V sauf si le breakout indique explicitement qu’il intègre un
régulateur et une adaptation de niveaux. Les broches peuvent être changées dans
le portail sans recompiler.

## Compiler et flasher

```bash
pio run -e esp32c3
pio run -e esp32c3 -t upload
pio device monitor -b 115200
```

Remplacer `esp32c3` par `esp32s3` ou `esp32dev` selon la carte.

## Première mise en route

1. Flasher l’ESP32 puis rejoindre le Wi‑Fi `POG-Sensor-Setup`.
2. Le portail captif s’ouvre automatiquement sur iOS, macOS, Android et
   Windows. Ouvrir `http://192.168.4.1` seulement s’il ne s’affiche pas.
3. Choisir un réseau dans le scanner Wi‑Fi, puis saisir son mot de passe et le
   nom du capteur. Les GPIO I²C restent disponibles dans les réglages avancés.
4. Après redémarrage, ouvrir POG Home et adopter l’appareil
   `ESP-SENSOR-<MAC>`.
5. Les entités apparaissent automatiquement sous la catégorie **Climat**.

Si POG Home ne peut pas être découvert en mDNS, son IP peut être saisie comme
adresse de secours dans le portail. Une configuration incorrecte du Wi‑Fi
réactive automatiquement le point d’accès après 20 secondes.

## Comportement des mesures

La lecture a lieu toutes les 30 secondes par défaut. Une mise à jour MQTT est
envoyée dès qu’une mesure varie d’au moins :

- `0,2 °C` ;
- `1 %` d’humidité ;
- `1 hPa` ;
- `5 dBm` pour le signal Wi‑Fi.

Un état complet est aussi republié toutes les cinq minutes. Le mode de mesure
forcé du BME/BMP280 réduit son auto-échauffement entre deux lectures. Une
correction de température de `−10 à +10 °C` est configurable depuis le portail.

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
- identités d’entités stables : `temperature`, `humidity`, `pressure`,
  `sensor_status` et `wifi_signal`.

Les entités sont en lecture seule. Le champ `local_rules` est réservé dans le
descripteur pour rester compatible avec les futures versions du protocole.

## Test hôte

La politique de détection des variations est testable sans ESP32 :

```bash
c++ -std=c++17 -Wall -Wextra -I include \
  test_host/test_sampling_policy.cpp -o /tmp/pog-sensor-test
/tmp/pog-sensor-test
```

## CI et releases

Le workflow GitHub Actions suit le même contrat de publication que PogLight et
POG AirPlay :

- une pull request compile et teste les trois cartes sans créer de release ;
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
