#include "ota_update.h"

#include <ArduinoJson.h>
#include <Update.h>
#include <WiFi.h>
#include <esp_crt_bundle.h>
#include <esp_http_client.h>
#include <mbedtls/sha256.h>

#ifndef POGSENSOR_FW_VERSION
#define POGSENSOR_FW_VERSION "dev"
#endif

namespace {

constexpr char kManifestUrl[] =
    "https://github.com/POG-Projects/pog-os-sensor/releases/latest/download/manifest.json";
constexpr char kReleaseAssetPrefix[] =
    "https://github.com/POG-Projects/pog-os-sensor/releases/download/v";
constexpr uint32_t kFirstCheckDelayMs = 8000;
constexpr uint32_t kCheckPeriodMs = 6UL * 60UL * 60UL * 1000UL;
constexpr size_t kMaxManifestBytes = 8192;

enum class Phase : uint8_t {
  Idle,
  Checking,
  Current,
  Available,
  Downloading,
  Verifying,
  Error,
};

struct State {
  Phase phase = Phase::Idle;
  String latestVersion;
  String assetName;
  String assetUrl;
  String expectedSha256;
  String error;
  uint8_t progress = 0;
  bool checked = false;
  bool updateAvailable = false;
};

State state;
SemaphoreHandle_t stateMutex = nullptr;
TaskHandle_t updaterTaskHandle = nullptr;
volatile bool checkRequested = false;
volatile bool installRequested = false;

const char *boardEnvironment() {
#if CONFIG_IDF_TARGET_ESP32C3
  return "esp32c3";
#elif CONFIG_IDF_TARGET_ESP32S3
  return "esp32s3";
#else
  return "esp32dev";
#endif
}

const char *phaseName(Phase phase) {
  switch (phase) {
    case Phase::Checking:
      return "checking";
    case Phase::Current:
      return "current";
    case Phase::Available:
      return "available";
    case Phase::Downloading:
      return "downloading";
    case Phase::Verifying:
      return "verifying";
    case Phase::Error:
      return "error";
    default:
      return "idle";
  }
}

void setPhase(Phase phase, const String &error = "") {
  xSemaphoreTake(stateMutex, portMAX_DELAY);
  state.phase = phase;
  state.error = error;
  xSemaphoreGive(stateMutex);
}

bool parseSemver(const String &raw, int &major, int &minor, int &patch) {
  String version = raw;
  if (version.startsWith("v")) version.remove(0, 1);
  int first = version.indexOf('.');
  int second = version.indexOf('.', first + 1);
  if (first <= 0 || second <= first + 1 ||
      version.indexOf('.', second + 1) >= 0) {
    return false;
  }
  String a = version.substring(0, first);
  String b = version.substring(first + 1, second);
  String c = version.substring(second + 1);
  if (!a.length() || !b.length() || !c.length()) return false;
  for (char ch : a)
    if (!isDigit(ch)) return false;
  for (char ch : b)
    if (!isDigit(ch)) return false;
  for (char ch : c)
    if (!isDigit(ch)) return false;
  major = a.toInt();
  minor = b.toInt();
  patch = c.toInt();
  return true;
}

bool isNewerVersion(const String &latest, const String &current) {
  int lMajor, lMinor, lPatch, cMajor, cMinor, cPatch;
  if (!parseSemver(latest, lMajor, lMinor, lPatch) ||
      !parseSemver(current, cMajor, cMinor, cPatch)) {
    return false;
  }
  if (lMajor != cMajor) return lMajor > cMajor;
  if (lMinor != cMinor) return lMinor > cMinor;
  return lPatch > cPatch;
}

bool safeAssetName(const String &name) {
  if (!name.length() || name.length() > 80 || name.indexOf("..") >= 0)
    return false;
  for (char ch : name) {
    if (!(isAlphaNumeric(ch) || ch == '-' || ch == '_' || ch == '.'))
      return false;
  }
  return true;
}

bool validSha256(const String &hash) {
  if (hash.length() != 64) return false;
  for (char ch : hash)
    if (!isHexadecimalDigit(ch)) return false;
  return true;
}

String digestToHex(const uint8_t digest[32]) {
  char out[65];
  for (size_t i = 0; i < 32; ++i) {
    snprintf(out + i * 2, 3, "%02x", digest[i]);
  }
  out[64] = '\0';
  return String(out);
}

struct HttpContext {
  String *text = nullptr;
  mbedtls_sha256_context *sha = nullptr;
  size_t received = 0;
  int64_t total = 0;
  bool tooLarge = false;
  bool writeFailed = false;
  uint8_t lastProgress = 0;
};

esp_err_t httpEvent(esp_http_client_event_t *event) {
  HttpContext *context = static_cast<HttpContext *>(event->user_data);
  if (!context) return ESP_OK;

  if (event->event_id == HTTP_EVENT_ON_HEADER && event->header_key &&
      event->header_value &&
      strcasecmp(event->header_key, "Content-Length") == 0) {
    context->total = strtoll(event->header_value, nullptr, 10);
  }

  if (event->event_id != HTTP_EVENT_ON_DATA || !event->data_len ||
      esp_http_client_get_status_code(event->client) != 200) {
    return ESP_OK;
  }

  if (context->text) {
    if (context->text->length() + event->data_len > kMaxManifestBytes) {
      context->tooLarge = true;
      return ESP_FAIL;
    }
    context->text->concat(static_cast<const char *>(event->data),
                          event->data_len);
    context->received += event->data_len;
    return ESP_OK;
  }

  if (context->sha) {
    const uint8_t *data = static_cast<const uint8_t *>(event->data);
    if (Update.write(const_cast<uint8_t *>(data), event->data_len) !=
        static_cast<size_t>(event->data_len)) {
      context->writeFailed = true;
      return ESP_FAIL;
    }
    mbedtls_sha256_update(context->sha, data, event->data_len);
    context->received += event->data_len;
    if (context->total > 0) {
      uint8_t progress =
          min(99, static_cast<int>(context->received * 100ULL /
                                   static_cast<uint64_t>(context->total)));
      if (progress != context->lastProgress) {
        context->lastProgress = progress;
        xSemaphoreTake(stateMutex, portMAX_DELAY);
        state.progress = progress;
        xSemaphoreGive(stateMutex);
      }
    }
  }
  return ESP_OK;
}

esp_http_client_handle_t createHttpClient(const char *url,
                                          HttpContext *context) {
  esp_http_client_config_t config = {};
  config.url = url;
  config.user_agent = "POGSensor/" POGSENSOR_FW_VERSION;
  config.timeout_ms = 20000;
  config.disable_auto_redirect = false;
  config.max_redirection_count = 6;
  config.event_handler = httpEvent;
  config.user_data = context;
  config.crt_bundle_attach = esp_crt_bundle_attach;
  config.buffer_size = 4096;
  // Les assets GitHub redirigent vers une URL signée plus longue que le
  // tampon d'émission ESP-IDF par défaut.
  config.buffer_size_tx = 4096;
  return esp_http_client_init(&config);
}

bool fetchManifest(String &body, String &error) {
  HttpContext context;
  context.text = &body;
  esp_http_client_handle_t client = createHttpClient(kManifestUrl, &context);
  if (!client) {
    error = "initialisation HTTPS impossible";
    return false;
  }
  esp_http_client_set_header(client, "Accept", "application/json");
  esp_err_t result = esp_http_client_perform(client);
  int status = esp_http_client_get_status_code(client);
  esp_http_client_cleanup(client);

  if (result != ESP_OK) {
    error = "GitHub est injoignable (" + String(esp_err_to_name(result)) + ")";
    return false;
  }
  if (status != 200) {
    error = "GitHub répond HTTP " + String(status);
    return false;
  }
  if (context.tooLarge || !body.length()) {
    error =
        context.tooLarge ? "manifeste trop volumineux" : "manifeste vide";
    return false;
  }
  return true;
}

bool checkLatestRelease() {
  setPhase(Phase::Checking);
  String body;
  String error;
  if (!fetchManifest(body, error)) {
    setPhase(Phase::Error, error);
    return false;
  }

  JsonDocument manifest;
  if (deserializeJson(manifest, body)) {
    setPhase(Phase::Error, "manifest.json est invalide");
    return false;
  }
  String version = manifest["version"].as<String>();
  int major, minor, patch;
  if (!parseSemver(version, major, minor, patch)) {
    setPhase(Phase::Error, "version de release invalide");
    return false;
  }

  JsonObjectConst selected;
  for (JsonObjectConst board : manifest["boards"].as<JsonArrayConst>()) {
    if (board["env"].as<String>() == boardEnvironment()) {
      selected = board;
      break;
    }
  }
  if (selected.isNull()) {
    setPhase(Phase::Error, "aucun firmware pour cette carte");
    return false;
  }

  String asset = selected["app"].as<String>();
  String hash = selected["appSha256"].as<String>();
  hash.toLowerCase();
  if (!safeAssetName(asset) || !validSha256(hash)) {
    setPhase(Phase::Error, "asset de release invalide");
    return false;
  }

  bool available = isNewerVersion(version, POGSENSOR_FW_VERSION);
  xSemaphoreTake(stateMutex, portMAX_DELAY);
  state.latestVersion = version;
  state.assetName = asset;
  state.assetUrl = String(kReleaseAssetPrefix) + version + "/" + asset;
  state.expectedSha256 = hash;
  state.checked = true;
  state.updateAvailable = available;
  state.progress = 0;
  state.error = "";
  state.phase = available ? Phase::Available : Phase::Current;
  xSemaphoreGive(stateMutex);
  Serial.printf("[OTA] version locale %s, release %s%s\n",
                POGSENSOR_FW_VERSION, version.c_str(),
                available ? " disponible" : " à jour");
  return true;
}

bool installLatestRelease() {
  String url;
  String expectedHash;
  String targetVersion;
  xSemaphoreTake(stateMutex, portMAX_DELAY);
  if (!state.updateAvailable || state.assetUrl.isEmpty()) {
    xSemaphoreGive(stateMutex);
    return false;
  }
  url = state.assetUrl;
  expectedHash = state.expectedSha256;
  targetVersion = state.latestVersion;
  state.phase = Phase::Downloading;
  state.progress = 0;
  state.error = "";
  xSemaphoreGive(stateMutex);

  if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH)) {
    setPhase(Phase::Error, "espace OTA insuffisant");
    return false;
  }

  mbedtls_sha256_context sha;
  mbedtls_sha256_init(&sha);
  mbedtls_sha256_starts(&sha, 0);
  HttpContext context;
  context.sha = &sha;
  esp_http_client_handle_t client = createHttpClient(url.c_str(), &context);
  if (!client) {
    mbedtls_sha256_free(&sha);
    Update.abort();
    setPhase(Phase::Error,
             "initialisation du téléchargement impossible");
    return false;
  }
  esp_http_client_set_header(client, "Accept",
                             "application/octet-stream");
  esp_err_t result = esp_http_client_perform(client);
  int status = esp_http_client_get_status_code(client);
  esp_http_client_cleanup(client);

  if (result != ESP_OK || status != 200 || context.writeFailed ||
      !context.received) {
    mbedtls_sha256_free(&sha);
    Update.abort();
    String reason = status != 200 ? "téléchargement HTTP " + String(status)
                                  : "téléchargement interrompu";
    setPhase(Phase::Error, reason);
    return false;
  }

  setPhase(Phase::Verifying);
  uint8_t digest[32];
  mbedtls_sha256_finish(&sha, digest);
  mbedtls_sha256_free(&sha);
  String actualHash = digestToHex(digest);
  if (actualHash != expectedHash) {
    Update.abort();
    setPhase(Phase::Error, "empreinte SHA-256 incorrecte");
    Serial.printf("[OTA] SHA-256 refusé: %s au lieu de %s\n",
                  actualHash.c_str(), expectedHash.c_str());
    return false;
  }

  if (!Update.end(true)) {
    setPhase(Phase::Error, "activation du firmware impossible");
    return false;
  }

  xSemaphoreTake(stateMutex, portMAX_DELAY);
  state.progress = 100;
  state.phase = Phase::Verifying;
  xSemaphoreGive(stateMutex);
  Serial.printf("[OTA] version %s vérifiée, redémarrage\n",
                targetVersion.c_str());
  delay(800);
  ESP.restart();
  return true;
}

void updaterTask(void *) {
  uint32_t nextAutomaticCheck = millis() + kFirstCheckDelayMs;
  for (;;) {
    uint32_t now = millis();
    if (WiFi.status() == WL_CONNECTED) {
      if (checkRequested ||
          (int32_t)(now - nextAutomaticCheck) >= 0) {
        checkRequested = false;
        checkLatestRelease();
        nextAutomaticCheck = millis() + kCheckPeriodMs;
      }
      if (installRequested) {
        installRequested = false;
        installLatestRelease();
      }
    }
    vTaskDelay(pdMS_TO_TICKS(250));
  }
}

}  // namespace

void otaUpdateBegin() {
  if (updaterTaskHandle) return;
  if (!stateMutex) stateMutex = xSemaphoreCreateMutex();
  xTaskCreate(updaterTask, "ota-update", 12288, nullptr, 1,
              &updaterTaskHandle);
}

void otaUpdateRequestCheck() {
  if (!updaterTaskHandle) otaUpdateBegin();
  checkRequested = true;
}

bool otaUpdateRequestInstall() {
  if (!updaterTaskHandle) otaUpdateBegin();
  xSemaphoreTake(stateMutex, portMAX_DELAY);
  bool allowed = state.updateAvailable &&
                 state.phase != Phase::Downloading &&
                 state.phase != Phase::Verifying;
  xSemaphoreGive(stateMutex);
  if (allowed) installRequested = true;
  return allowed;
}

void otaUpdateFillJson(JsonObject out) {
  if (!stateMutex) otaUpdateBegin();
  xSemaphoreTake(stateMutex, portMAX_DELAY);
  out["phase"] = phaseName(state.phase);
  out["currentVersion"] = POGSENSOR_FW_VERSION;
  out["latestVersion"] = state.latestVersion;
  out["board"] = boardEnvironment();
  out["checked"] = state.checked;
  out["updateAvailable"] = state.updateAvailable;
  out["progress"] = state.progress;
  if (state.assetName.length()) out["asset"] = state.assetName;
  if (state.error.length()) out["error"] = state.error;
  if (state.latestVersion.length()) {
    out["releaseUrl"] =
        String("https://github.com/POG-Projects/pog-os-sensor/releases/tag/v") +
        state.latestVersion;
  }
  xSemaphoreGive(stateMutex);
}

OtaVisualState otaUpdateVisualState() {
  if (!stateMutex) return OtaVisualState::Idle;
  xSemaphoreTake(stateMutex, portMAX_DELAY);
  Phase phase = state.phase;
  xSemaphoreGive(stateMutex);
  switch (phase) {
    case Phase::Checking:
      return OtaVisualState::Checking;
    case Phase::Available:
      return OtaVisualState::Available;
    case Phase::Downloading:
      return OtaVisualState::Downloading;
    case Phase::Verifying:
      return OtaVisualState::Verifying;
    case Phase::Error:
      return OtaVisualState::Error;
    default:
      return OtaVisualState::Idle;
  }
}
