/*
# Spectra6 E-Ink Picture Frame

13.3-inch Spectra 6 E-Ink digital picture frame project.

Features:
- Displays images in raw bitmap format from an SD card on a 13.3-inch Spectra 6 (1600x1200) e-ink display.
- Image filenames are listed in `order.txt`; the current index is tracked via `index.txt` and updated after each display cycle.
- The ESP32 enters deep sleep for a duration specified by `SLEEP_TIME` (default: 24 hours).
- To conserve battery, a MOSFET disconnects the SD card reader entirely when the system is idle.

---

### Parts List:
- Waveshare 13.3" Spectra 6 E-Ink display (1600x1200) with driver HAT  WS‑29355
- DFRobot FireBeetle 2 ESP32-C6  DFR1075
- 1000 µF capacitor  
- 3.7V Li-polymer battery (with compatible connector for ESP32 board)  
- 3.3V micro SD card reader/writer module (note: must be 3.3V, not 5V, compare to the one in the picture as reference)  
- MOSFET trigger switch driver module  
- Micro switch  
-  it's recommended to use or custom-cut a mat board (passe-partout) with the following dimensions: Outer size: 40 × 30 cm (to fit the frame), Inner cutout: 26.7 × 20 cm (to match the visible area of the 13.3″ display)

---

### Build Instructions:
1. Connect components according to the wiring diagram and pin definitions in the source code.
2. Use the Arduino IDE to compile and upload the code to the ESP32 board.
3. Convert your images to raw format using the provided Python script (or your own). You may apply dithering and contrast adjustments to match the look of the original image as closely as possible.
4. Save the raw image files to the SD card, along with:
   - `order.txt`: list of image filenames, one per line.
   - `index.txt`: current image index (updated automatically by the firmware).
5. Insert the SD card into the reader, power on the device, and enjoy your custom E-Ink photo frame.

---

### Notes:
- `SLEEP_TIME` is set to 24 hours by default—modify this in the source code as needed.
- Make sure your SD card is formatted correctly with FAT32 and uses filenames compatible with naming conventions.
- A capacitor in parallel with the display power supply is necessary; without it, the ESP32 will frequently reset due to brownouts, or the display may show strange artifacts.

Enjoy your low-power digital picture frame!

*/
