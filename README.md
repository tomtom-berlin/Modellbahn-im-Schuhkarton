# Modellbahn im Schuhkarton:
Ein kleines Rangierpuzzle (Inglenook siding) im 5-3-3 oder 3-2-2-Modus, je nach Fahrzeuglaenge (z.B. Hornby Era 2-4 kurze vent cars oder plank waggons + J50 steam shunter, oder G10/G1 + Köf, BR55 oder BR89).
Größe: 2 Teile a 36 cm x 22 cm x 19 cm (B x T x H)
Das DCC-Signal wird mit einem Seeeduino XIAO RP2040 erzeugt, als H-Bridge ist ein DRV8871 eingesetzt. Strommessung mit MAX471. Eingangsspannung 12V =.
Die Dekoder: - für die Beleuchtung: ATmega328P als "raw iron"
             - für die Weiche: ATtiny85 (Sparkfun), TTfiligran Weichenantrieb mit Schalter zur Herzstückpolarisierung, Servo MG996.
             - für die Drehscheibe auf dem ungestalteten Teil: Seeeduino XIAO SAMD21, Steppermotor GS12-15BY 1:100
Gebäude, Drehscheibe und "Schuhkarton": div. 3D-Drucke, Güterschuppen von Auhagen
