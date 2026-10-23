# docs/design/display/ — Design-Dateien fürs OLED-Display

Hier liegen **alle gestalterischen Vorlagen für das 1,3" OLED**: Menü-Layouts,
Screen-Mockups, Icons und Bitmap-Quellen. Dies ist der feste Pfad dafür (siehe
Platzierungsregel in [`../../ARBEITSWEISE.md`](../../ARBEITSWEISE.md) §2).

## Was hierher gehört
- **Layout-Skizzen / Mockups** einzelner Bildschirme (Startbildschirm, Hauptmenü,
  Statusanzeigen) — als Bild (PNG) oder Skizze.
- **Icons / Bitmaps** als Quelle (z. B. PNG), aus denen später das Display-Format
  erzeugt wird.
- Kurze **Beschreibung je Screen**: was wird angezeigt, wann.

## Was NICHT hierher gehört
- **Code.** Das OLED wird vom Firmware-Modul gezeichnet
  ([`../../../firmware/src/modules/display/`](../../../firmware/src/modules/display/)).
  Aus einem Bitmap erzeugter C-Code (z. B. XBM-Header) gehört **ins Display-Modul**,
  nicht hierher. Hier liegt nur die *Quelle/Gestaltung*.

## Technische Eckdaten (für maßhaltige Designs)
- Auflösung: **128 × 64 Pixel**, **monochrom** (1 Bit pro Pixel — nur an/aus).
- Entwürfe bitte in dieser Größe/Farbtiefe anlegen, sonst passen sie nicht.

## Namenskonvention
`<screen>.png`, z. B. `hauptmenue.png`, `start.png`, `nfc-gelesen.png`.

> Pendant für die Handy-Oberfläche: [`../web/`](../web/).
