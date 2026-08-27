#pragma once

#include <stdint.h>

// Politique de survie au courtier MQTT absent — pure, sans dépendance Arduino,
// compilée et exécutée sur l'hôte par la CI (même montage que sampling_policy,
// même leçon que hub_retry.c de pog-os-vox).
//
// Panne du 27 août 2026 : timsrv tombe des heures, POG Home redémarre
// plusieurs fois, et chaque appareil du foyer reste ensuite muet — zéro client
// MQTT — jusqu'à un débranchage physique. Un appareil mural ou encastré doit
// retrouver un courtier qui revient des heures plus tard : les tentatives ne
// cessent JAMAIS, seul l'intervalle grandit. Et l'adoption ne s'efface plus
// sur la foi d'un CONNACK refusé : un courtier qui redémarre refuse aussi
// pendant que ses comptes se reprovisionnent, alors que la ré-adoption d'un
// appareil effacé exige un geste humain côté POG Home — l'effacement est
// irréversible, le refus ne l'est pas.

namespace pogdev {

// ---- La reprise : 5 s, doublées à chaque échec, plafonnées à 60 s. ----

constexpr uint32_t kRetryBaseMs = 5000;
constexpr uint32_t kRetryCapMs = 60000;

struct Backoff {
  uint32_t baseMs = kRetryBaseMs;
  uint32_t capMs = kRetryCapMs;
  uint32_t nextMs = kRetryBaseMs;
};

// Rend l'attente avant la prochaine tentative, puis double le palier suivant —
// sans dépasser le plafond ni déborder l'entier près de sa borne.
inline uint32_t backoffNextDelayMs(Backoff &b) {
  uint32_t delay = b.nextMs;
  b.nextMs = b.nextMs <= b.capMs / 2 ? b.nextMs * 2 : b.capMs;
  return delay;
}

// Au CONNACK accepté seulement, jamais au simple TCP établi : un courtier qui
// accepte puis refuse en boucle garderait sinon le rythme le plus agressif.
inline void backoffConnected(Backoff &b) { b.nextMs = b.baseMs; }

// ---- Les refus d'authentification : demander avant d'effacer. ----
//
// L'ancien comportement effaçait l'adoption au premier CONNACK « identifiants
// refusés ». Or la relève (`GET /api/v1/pogdev/announce/<hw>?secret=`) existe
// et le serveur y répond de manière décisive : 404 = appareil vraiment oublié,
// « pending » = demande de ré-adoption posée et visible dans l'inventaire,
// « adopted » = identifiants neufs. C'est donc elle qui tranche, jamais le
// CONNACK. La garde attend que les refus persistent (un redémarrage du
// courtier passe en dessous), puis espace les relèves (POG Home limite les
// annonces par IP).

constexpr uint8_t kAuthStrikesBeforeProbe = 3;
constexpr uint32_t kAuthStrikeSpanMs = 90000;
constexpr uint32_t kAuthProbeSpacingMs = 120000;

struct AuthGate {
  uint8_t strikes = 0;
  bool counting = false;
  uint32_t firstStrikeAtMs = 0;
  bool probed = false;
  uint32_t lastProbeAtMs = 0;
};

// À appeler à chaque CONNACK refusé pour cause d'identifiants. Rend true
// quand une relève auprès de POG Home est due.
inline bool authGateRejected(AuthGate &g, uint32_t nowMs) {
  if (!g.counting) {
    g.counting = true;
    g.firstStrikeAtMs = nowMs;
  }
  if (g.strikes < 255) ++g.strikes;
  if (g.strikes < kAuthStrikesBeforeProbe) return false;
  if (nowMs - g.firstStrikeAtMs < kAuthStrikeSpanMs) return false;
  if (g.probed && nowMs - g.lastProbeAtMs < kAuthProbeSpacingMs) return false;
  g.probed = true;
  g.lastProbeAtMs = nowMs;
  return true;
}

inline void authGateConnected(AuthGate &g) { g = AuthGate{}; }

// ---- Le filet : le débranchage automatisé. ----
//
// Le remède constaté le 27 août était de débrancher puis rebrancher chaque
// appareil : ce que le code ne sait pas énumérer — sockets épuisées, pile
// réseau figée — un redémarrage le guérit. Trente minutes continues avec le
// Wi-Fi debout, une adoption valide et aucun courtier joignable ne relèvent
// plus du transitoire : on refait le geste, automatiquement. Le compteur ne
// court ni sans adoption (un appareil autonome ne doit pas clignoter toutes
// les demi-heures) ni sans Wi-Fi (ce serait mesurer une autre panne).

constexpr uint32_t kOfflineRebootMs = 30u * 60u * 1000u;

struct OfflineWatch {
  bool armed = false;
  uint32_t sinceMs = 0;
};

// À appeler à chaque passage de la boucle ; `offline` = adopté, Wi-Fi debout,
// courtier absent. Rend true quand le redémarrage est dû.
inline bool offlineWatchTick(OfflineWatch &w, bool offline, uint32_t nowMs) {
  if (!offline) {
    w.armed = false;
    return false;
  }
  if (!w.armed) {
    w.armed = true;
    w.sinceMs = nowMs;
    return false;
  }
  return nowMs - w.sinceMs >= kOfflineRebootMs;
}

}  // namespace pogdev
