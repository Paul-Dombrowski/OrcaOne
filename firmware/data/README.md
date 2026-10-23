# data/ — Dateisystem-Abbild für den ESP32 (LittleFS)

Was hier liegt, wird mit **„Upload Filesystem Image"** in den Flash des ESP32
geladen und vom Webserver ausgeliefert.

**Nicht direkt hier bearbeiten.** Die Weboberfläche wird in
[`../../web/`](../../web/) entwickelt und von dort hierher **kopiert**, bevor man
das Filesystem-Image hochlädt. So bleibt die eine Quelle (`web/`) maßgeblich.

Kopier-Befehl (aus dem Projekt-Stammverzeichnis, PowerShell):

```powershell
Copy-Item -Recurse -Force web\* firmware\data\
```
