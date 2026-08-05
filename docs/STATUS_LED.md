# Datasheet — LED RGB de statut POG Sensor

## Périmètre

Le firmware pilote un **segment de quatre pixels RGB adressables** découpé d’un
bandeau. Le composant de référence est le **WS2812B 5 V, protocole 800 kHz,
ordre GRB**. Des pixels SK6812 RGB compatibles 800 kHz conviennent également.
Les modèles RGBW ne sont pas pris en charge par ce profil. Les quatre pixels
sont commandés comme une seule lumière et n’utilisent qu’une ligne DATA.

Le segment est optionnel et désactivé par défaut. Cocher **Lampe adressable
installée** dans les réglages avancés du dashboard l’active au redémarrage. Si
l’option est désactivée, GPIO 7 n’est pas piloté et aucune entité lumière n’est
publiée dans POG Home.

> Un bandeau WS2818 alimenté en 12 V n’est pas un WS2812B : ne jamais envoyer
> son alimentation 12 V vers le rail 5 V ou l’ESP32-C3.

## Caractéristiques électriques

| Paramètre | Valeur retenue |
|---|---:|
| Alimentation du pixel | 5 V |
| Protocole | NRZ 800 kHz |
| Ordre des composantes | GRB |
| Nombre de pixels | 4 |
| GPIO DATA sur ESP32-C3 SuperMini | GPIO 7 |
| GPIO DATA sur ESP32-S3 | GPIO 7 |
| GPIO DATA sur ESP32 classique | GPIO 27 |
| Résistance DATA recommandée | 330 Ω, entre 220 et 470 Ω accepté |
| Découplage recommandé | 100 µF entre 5 V et GND près du pixel |
| Limite logicielle de luminosité | 96/255, soit 38 % |
| Courant maximal estimé avec cette limite | environ 92 mA au blanc pour 4 pixels |

Le signal 3,3 V du C3 fonctionne généralement avec le premier pixel placé près de la
carte. Pour un câble DATA long, un environnement bruité ou une alimentation 5 V
élevée, utiliser un adaptateur de niveau **74AHCT125** ou **74HCT14** alimenté
en 5 V.

## Branchement sur ESP32-C3 SuperMini

| Segment WS2812B | Connexion |
|---|---|
| `5V` / `VDD` | rail 5 V du harnais |
| `GND` / `VSS` | masse commune |
| `DIN` / flèche entrante | résistance 330 Ω puis GPIO 7 |
| `DOUT` du quatrième pixel | non connecté |

Respecter le sens des flèches imprimées sur le bandeau : le C3 commande
**DIN**, jamais DOUT. Couper le bandeau hors tension et étamer les pastilles
sans créer de pont entre 5 V, DATA et GND.

## États normaux

Les couleurs indiquées sont les teintes nominales avant la limitation de
luminosité et la correction gamma.

| Code | Couleur | Animation | Signification | Fin de l’état |
|---|---|---|---|---|
| `S00` | vert POG `#34C759` | respiration douce, cycle 3,2 s | firmware opérationnel, capteur présent et POG Home connecté | changement de condition |
| `S10` | blanc `#FFFFFF` | respiration, cycle 1,2 s | démarrage et initialisation du firmware | fin de `setup()` |
| `S20` | ambre `#FF8A00` | deux éclats de 160 ms, cycle 1,6 s | portail captif actif, configuration requise | redémarrage avec Wi‑Fi configuré |
| `S30` | cyan `#00C7FF` | respiration rapide, cycle 1,1 s | connexion au Wi‑Fi en cours | Wi‑Fi connecté ou portail captif lancé |
| `S40` | violet `#8B5CF6` | respiration lente, cycle 1,8 s | recherche, adoption ou connexion à POG Home | liaison MQTT POG Home établie |
| `S60` | couleur choisie | fixe | éclairage manuel ou présence radar active | extinction manuelle ou fin de présence et du délai |

## Lumière de présence pilotable

POG Home expose trois commandes sous l’appareil POG Sensor :

| Entité | Contrôles | Comportement |
|---|---|---|
| `Lumière de présence` | marche, arrêt, luminosité, teinte, saturation et température de blanc | contrôle les quatre pixels comme une seule lumière |
| `Allumage sur présence` | activé/désactivé | autorise les radars LD2410B ou LD2450 à allumer automatiquement la lumière |
| `Maintien après présence` | 0 à 300 secondes | évite une extinction brusque entre deux détections ; valeur initiale 8 s |

Les mêmes réglages sont accessibles dans le dashboard local, avec un aperçu de
la teinte. La couleur initiale est un blanc doré `#FFD28A` et la luminosité
relative est 55 %. La commande « Allumer » force la lumière même sans présence. La commande
« Éteindre » coupe immédiatement la lumière ; si une personne est encore
détectée, l’automatisme attend que la zone redevienne libre avant de se réarmer.
La couleur, la luminosité, l’automatisme et le délai sont conservés après un
redémarrage. L’allumage forcé est volontairement temporaire.

## Mise à jour OTA

| Code | Couleur | Animation | Signification |
|---|---|---|---|
| `S50` | bleu `#0A84FF` | alternance toutes les 120 ms | vérification de la dernière release |
| `S51` | bleu glacier `#64D2FF` | double éclat, cycle 2,4 s | mise à jour disponible, en attente de confirmation |
| `S52` | bleu profond `#007AFF` | respiration rapide, cycle 650 ms | téléchargement et écriture du firmware |
| `S53` | blanc `#FFFFFF` | alternance toutes les 100 ms | contrôle SHA-256 et activation de l’image |

Ne pas couper l’alimentation pendant `S52` ou `S53`. Après une vérification
réussie, l’ESP32 redémarre automatiquement.

## Codes d’erreur

Un code rouge ou rose est formé d’un nombre d’éclats de 140 ms, espacés de
180 ms, puis d’une pause. Le message correspondant est également écrit sur le
port série à 115200 bauds.

| Code | Signal lumineux | Détection | Cause probable | Action recommandée |
|---|---|---|---|---|
| `E10` | rouge, 1 éclat | aucun capteur I²C et aucun radar UART détecté | alimentation, masse, SDA/SCL ou RX/TX absent | vérifier le harnais et les rails 3,3/5 V |
| `E11` | rouge, 2 éclats | un module environnemental est détecté mais sa lecture échoue | bus I²C perturbé, adresse en conflit ou alimentation instable | contrôler SDA GPIO 6, SCL GPIO 5 et les résistances de tirage |
| `E12` | rouge, 3 éclats | un radar déjà reconnu ne produit plus de trame valide pendant 3 s | radar débranché, TX coupé, débit incorrect ou chute du 5 V | vérifier TX du radar vers RX ESP et la masse commune |
| `E30` | violet, 3 éclats | appareil adopté mais MQTT POG Home déconnecté depuis au moins 60 s | POG Home arrêté, réseau isolé ou identifiants invalides | vérifier POG Home et le réseau local |
| `E40` | rose, 4 éclats | contrôle ou installation OTA en échec | GitHub inaccessible, manifeste invalide, espace insuffisant ou SHA-256 refusé | consulter le portail et le journal série ; retenter la vérification |

`E10`, `E11`, `E12` et `E30` disparaissent automatiquement dès que la condition
revient à la normale. `E40` est affiché pendant 12 secondes pour ne pas masquer
indéfiniment un appareil par ailleurs fonctionnel ; le détail reste disponible
dans la page OTA.

## Priorité d’affichage

Une seule LED ne peut montrer qu’un état à la fois. Le firmware applique la
priorité suivante, de la plus forte à la plus faible :

1. téléchargement ou vérification OTA ;
2. erreur OTA temporaire ;
3. portail de configuration ou connexion Wi‑Fi ;
4. panne de capteur ou de radar ;
5. lumière manuelle ou présence détectée ;
6. perte de POG Home ;
7. mise à jour disponible ;
8. fonctionnement normal.

## Diagnostic logiciel

Le changement d’état apparaît sur le port série sous la forme :

```text
[LED] S00 opérationnel
[LED] E12 liaison radar
```

L’endpoint local authentifié `GET /api/status` expose aussi le champ
`status_led` lorsque le segment est déclaré installé, ce qui permet au portail
et aux outils de diagnostic de retrouver le code affiché.

## Personnalisation à la compilation

La broche et le nombre de pixels sont définis par `POGSENSOR_STATUS_LED_PIN` et
`POGSENSOR_STATUS_LED_COUNT` dans `platformio.ini`. Pour une carte personnalisée,
remplacer les valeurs dans l’environnement PlatformIO concerné. La broche
choisie doit rester distincte de SDA, SCL et des quatre GPIO UART des radars.
