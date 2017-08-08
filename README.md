# NeoPixelClock

Eine Uhr mit 60 Neopixel auf Basis ESP-01
Beinhaltet WiFiManager zur Konfiguration des lokalen WLAN.
 
Da kein Pin für die Anwendung eines einfachen LDR verfügbar ist,
wird Sonnenauf- und -untergang für die Helligkeitssteuerung
verwendet (TimeLord.h). Nachdem die Neopixel über Pin 3 (Rx) gesteuert werden,
sind Pins 0 und 2 wieder verfügbar. Es könnten weitere Interfaces
über I2C angebunden werden (hier nicht umgesetzt).
Sommer-/Winterzeit wird über TimeLib.h berechnet, die Zeit selbst liefert
NTP (de.pool.ntp.org)
 
Für mehr Pins kann ESP-12 verwendet werden. dazu
"#define ESP-12 1" unkommentieren.

17-08-07:
Der Status der Synchronisation wird jetzt durch die Farbe der 5-Minuten-Marker angezeigt.

17-08-08:
Der Kontrast zwischen Markern und Zeigern vor einem NTP-Sync wurde verbessert.
