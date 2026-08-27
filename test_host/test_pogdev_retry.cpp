#include <cassert>
#include <cstdint>
#include <cstdio>

#include "pogdev_retry.h"

using pogdev::AuthGate;
using pogdev::Backoff;
using pogdev::OfflineWatch;

int main() {
  // -- La reprise double puis se plafonne, et ne cesse jamais. --
  Backoff b;
  assert(pogdev::backoffNextDelayMs(b) == 5000);
  assert(pogdev::backoffNextDelayMs(b) == 10000);
  assert(pogdev::backoffNextDelayMs(b) == 20000);
  assert(pogdev::backoffNextDelayMs(b) == 40000);
  assert(pogdev::backoffNextDelayMs(b) == 60000);
  // Un courtier absent des heures : le pas reste au plafond, jamais nul,
  // jamais un abandon — c'est la panne du 27 août qu'on rejoue ici.
  for (int i = 0; i < 10000; ++i) {
    assert(pogdev::backoffNextDelayMs(b) == 60000);
  }
  // Le retour au pas de base n'a lieu qu'au CONNACK accepté.
  pogdev::backoffConnected(b);
  assert(pogdev::backoffNextDelayMs(b) == 5000);

  // Près de la borne de l'entier, le doublement ne déborde pas.
  Backoff wide;
  wide.baseMs = 0x90000000u;
  wide.capMs = 0xF0000000u;
  wide.nextMs = wide.baseMs;
  assert(pogdev::backoffNextDelayMs(wide) == 0x90000000u);
  assert(pogdev::backoffNextDelayMs(wide) == 0xF0000000u);
  assert(pogdev::backoffNextDelayMs(wide) == 0xF0000000u);

  // -- Des refus isolés ou rapprochés ne déclenchent rien : un courtier qui
  // redémarre refuse pendant que ses comptes se reprovisionnent. --
  AuthGate g;
  assert(!pogdev::authGateRejected(g, 1000));
  assert(!pogdev::authGateRejected(g, 6000));
  assert(!pogdev::authGateRejected(g, 11000));  // 3 refus, mais 10 s d'étalement
  // Les refus persistent au-delà de la fenêtre : la relève devient due.
  assert(pogdev::authGateRejected(g, 91000));
  // ...et reste polie ensuite (POG Home limite les annonces par IP).
  assert(!pogdev::authGateRejected(g, 151000));
  assert(pogdev::authGateRejected(g, 211000));
  // Un CONNACK accepté remet tout à zéro : plus rien avant une vraie récidive.
  pogdev::authGateConnected(g);
  assert(!pogdev::authGateRejected(g, 500000));
  assert(!pogdev::authGateRejected(g, 505000));

  // -- Le filet ne se déclenche qu'après 30 minutes CONTINUES hors ligne. --
  OfflineWatch w;
  assert(!pogdev::offlineWatchTick(w, true, 0));
  assert(!pogdev::offlineWatchTick(w, true, 29u * 60u * 1000u));
  assert(pogdev::offlineWatchTick(w, true, 30u * 60u * 1000u));
  // Un retour du courtier, même bref, désarme le compteur.
  OfflineWatch w2;
  assert(!pogdev::offlineWatchTick(w2, true, 0));
  assert(!pogdev::offlineWatchTick(w2, false, 20u * 60u * 1000u));
  assert(!pogdev::offlineWatchTick(w2, true, 21u * 60u * 1000u));
  assert(!pogdev::offlineWatchTick(w2, true, 50u * 60u * 1000u));
  assert(pogdev::offlineWatchTick(w2, true, 51u * 60u * 1000u + 1u));
  // Le débordement de millis() (49,7 jours) ne fausse pas la mesure : un
  // appareil encastré atteint cette échéance en vrai.
  OfflineWatch w3;
  uint32_t nearWrap = 0xFFFFFFFFu - 1000u;
  assert(!pogdev::offlineWatchTick(w3, true, nearWrap));
  assert(!pogdev::offlineWatchTick(w3, true, nearWrap + 29u * 60u * 1000u));
  assert(pogdev::offlineWatchTick(w3, true, nearWrap + 30u * 60u * 1000u));

  puts("politique de reprise MQTT : OK");
  return 0;
}
