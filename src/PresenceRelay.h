#pragma once

#include <Arduino.h>

// Controla el relé a partir del target state reportado por el radar, con
// debounce: exige presencia continua durante CONFIRM_MS antes de encender, y
// ausencia continua durante HOLD_MS antes de apagar.
namespace PresenceRelay {

// Configura el pin del relé. Llamar una vez en setup().
void begin();

// targetState: 0 = sin objetivo, cualquier otro valor = objetivo detectado.
// Llamar una vez por iteración de loop() con el último estado conocido.
void update(uint8_t targetState);

bool isOn();

} // namespace PresenceRelay
