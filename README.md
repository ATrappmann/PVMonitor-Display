# PVMonitor-Display

**PVMonitor-Display** ist ein *ESP32-Projekt* zur Visualisierung der Leistung eines *Balkonkraftwerks* mit Wechselrichter und Akkuspeicher,
welches über den [**PVMonitor**](https://github.com/ATrappmann/PVMonitor) gesteuert wird.

Im Gegensatz zum *Balkonkraftwerk*, welches sich im Keller befindet, kann das **PVMonitor-Display** an einer beliebigen Stelle
im Haus installiert werden. Es kommuniziert über die JSON-Schnittstelle mit dem **PVMonitor** und zeigt die PV-Leistung, den Hausverbrauch, 
den Netzbezug und den Ladezustand des Akkuspeichers an.

![PVMonitor-Display](/docs/PVMonitor-Display.png)

## Einführung

Installiert man ein **PVMonitor-Display** beispielsweise in der Küche, kann man anhand der 
angezeigten Werte erkennen, ob es gerade ein guter Zeitpunkt ist, mit der selbst erzeugten Energie die Spülmaschine zu betreiben. 
Andernfalls kann man mit der Zeitvorwahltaste den Start des Spülprogramms auf einen späteren Zeitpunkt verlegen, an dem die 
Sonne stärker scheinen soll.

Startet man die Spülmaschine und die Waschmaschine nacheinander und nicht gleichzeitig, erreicht man ebenfalls eine bessere Ausnutzung
der selbst erzeugten PV-Energie und reduziert den Netzbezug erheblich.

## Verwendete Komponenten

Voraussetzung für den Einsatz der **PVMonitor-Display** ist der **PVMonitor** aus dem Projekt https://github.com/ATrappmann/PVMonitor.

Ansonsten benötigt man folgende Komponenten:

* ESP32 Development Board
* TFT display, 1,3", 240x240, ICST7789VW
* 3D-Druck Gehäuse

Über gelötete Kabel werden folgende Verbindungen hergestellt:

![Connections](/docs/Connections.png)

## Die Software
Für den **PVMonitor-Display** werden folgende externe Software-Bibliotheken benötigt:
* https://github.com/bblanchon/ArduinoJson
* https://github.com/adafruit/Adafruit-ST7735-Library

## Copyright
**PVMonitor-Display** is written by Andreas Trappmann.
It is published under the MIT license, check LICENSE for more information.
All text above must be included in any redistribution.

## Release Notes

Version 1.0 - 26.06.2025

  * Initial publication.
