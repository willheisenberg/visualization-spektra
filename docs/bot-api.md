# Spektra per JSON-RPC steuern

Kodis vorhandener JSON-RPC-Zugang und dessen Zugangsdaten werden benutzt. `params` ist stets ein
Array von `key=value`-Strings.

```json
{"jsonrpc":"2.0","id":1,"method":"Addons.ExecuteAddon","params":{"addonid":"visualization.spektra","params":["action=set","key=intensity","value=40"]}}
```

| Aktion | Argumente | Wirkung |
|---|---|---|
| Einschalten | `["action=on"]` | Spektra als Kodi-Visualisierung wählen, öffnen, wenn Musik läuft |
| Ausschalten | `["action=off"]` | Fenster schließen, kein automatisches Öffnen |
| Umschalten | `["action=toggle"]` | an/aus |
| Einstellung setzen | `["action=set","key=<schlüssel>","value=<wert>"]` | eine Einstellung live ändern |
| Zuordnung setzen | `["action=set","key=route.<effekt>","value=bass+high"]` | Bänder eines Effekts, getrennt durch `+` (ein Komma trennt Kodi als neues Argument ab); `value=` leer = Effekt aus |
| Zurücksetzen | `["action=reset"]` | alle Einstellungen auf Standard |
| Szene | `["action=scene","name=eclipse"]` | Szene wählen (derzeit nur `eclipse`) |
| Status | `["action=status"]` | Status senden |
| Menü | `["action=menu"]` oder keine Argumente | Menü am Fernseher öffnen |

## Schlüssel

| Schlüssel | Werte | Standard |
|---|---|---|
| `enabled`, `autoopen` | `true`/`false` | `true` |
| `scene` | `eclipse` | `eclipse` |
| `intensity` | 1–100 | 70 |
| `eclipse.flame`, `eclipse.waves`, `eclipse.particles`, `eclipse.corona` | 0–100 | 100 |
| `eclipse.pixel` | 0–8 = 0,5× · 0,75× · 1× · 1,5× · 2× · 3× · 4× · 6× · 8× | 2 (1×) |
| `eclipse.drop` | `true`/`false` | `true` |
| `eclipse.route.<effekt>.<band>` | `true`/`false` | siehe unten |
| `maxparticles` | 10000–60000 | 42000 |

Effekte: `breath` (Ring atmet), `flame` (Flammenhöhe), `corona` (Corona-Stärke), `free` (Staub löst
sich), `flow` (Strömung), `wave` (Welle), `burst` (Flammenstoß rundum), `jet`
(Stichflamme), `streamer` (Streamer + Bogen), `dust` (Staubausbruch). Bänder: `bass`, `low`, `mid`,
`high`. Standard: breath→bass, flame→low, corona→mid, free→high, flow→high, wave→bass, burst→mid,
jet→low, streamer→mid, dust→high.

## Status

Erfolg und Fehler kommen asynchron als `Other.spektra_status` über Kodis WebSocket, auch für
Befehle, die schon beim Prüfen der Argumente scheitern;
`ExecuteAddon` bestätigt nur die Übergabe. Das interne `Other.spektra_cmd` kann der Bot ignorieren.

| Feld | Inhalt |
|---|---|
| `state` | `on`, `off` oder `error` |
| `visible` | Visualisierungsfenster offen |
| `scene` | aktive Szene |
| `settings` | alle Einstellungen |
| `error` | `invalid_command`, `unknown_key`, `invalid_value` oder leer |
| `revision` | steigt mit jeder Änderung |

Abfrage auch per `XBMC.GetInfoLabels` auf `Window(Home).Property(spektra.<feld>)` (`settings` als
JSON-Text). `spektra.service=ready` bestätigt den laufenden Service.

## Automatisches Öffnen unterdrücken

Spektra öffnet die Visualisierung bei Musikstart nur, wenn es in Kodi als Visualisierung gewählt
ist. Spielt ein anderes Addon Ton ab, der keine Musik ist, setzt es solange
`Window(10000).setProperty("spektra.suppress", "<addon-id>")` und löscht sie danach.
`partyvideo.suppress` wird ebenfalls beachtet (Telegram-Anrufe).
