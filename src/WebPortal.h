#pragma once

// Portal web de configuración: Access Point + servidor HTTP con vista en vivo
// de la energía por gate y endpoints para ajustar la sensibilidad.
namespace WebPortal {

// Levanta el Access Point y el servidor HTTP. Llamar una vez en setup(),
// después de RadarProtocol::begin().
void begin();

// Procesa peticiones HTTP pendientes. Llamar una vez por iteración de loop().
void handleClient();

} // namespace WebPortal
