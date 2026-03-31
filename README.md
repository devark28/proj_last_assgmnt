# Assessment Projects — Run & Test Instructions

---

## Project 1 — Smart Traffic Light Controller (Tinkercad)

### Components

| Component     | Quantity        |
|---------------|-----------------|
| Arduino Uno   | 1               |
| Red LED       | 2               |
| Yellow LED    | 2               |
| Green LED     | 2               |
| 220Ω resistor | 6 (one per LED) |
| Push button   | 2               |
| Breadboard    | 1               |
| Jumper wires  | as needed       |

> No resistors needed for buttons — the code uses `INPUT_PULLUP` (internal pull-up), so buttons connect directly between the pin and GND.

### Wiring — Intersection 0 (North-South)

| Component                  | Arduino Pin |
|----------------------------|-------------|
| Red LED (+ through 220Ω)   | Pin 2       |
| Yellow LED (+ through 220Ω)| Pin 3       |
| Green LED (+ through 220Ω) | Pin 4       |
| Button (one leg)           | Pin 5       |
| Button (other leg)         | GND         |

### Wiring — Intersection 1 (East-West)

| Component                  | Arduino Pin |
|----------------------------|-------------|
| Red LED (+ through 220Ω)   | Pin 6       |
| Yellow LED (+ through 220Ω)| Pin 7       |
| Green LED (+ through 220Ω) | Pin 8       |
| Button (one leg)           | Pin 9       |
| Button (other leg)         | GND         |

All LED cathodes (short leg / flat side) connect to GND on the breadboard.
Power: Arduino **5V** -> breadboard positive rail, Arduino **GND** -> breadboard negative rail.

### Steps in Tinkercad

1. Go to tinkercad.com -> Circuits -> **Create new Circuit**
2. Drag in: 1x Arduino Uno, 1x Breadboard, 6x LED, 6x 220Ω Resistor, 2x Pushbutton
3. Wire everything according to the tables above
4. Click **Code** (top right) -> switch the dropdown from **Blocks** to **Text**
5. Delete the placeholder code, paste the entire contents of `traffic_light.ino`
6. Click **Start Simulation**
7. Open the **Serial Monitor** (bottom of screen) — type `?` and hit Send to see the menu
8. Click the buttons to simulate vehicles arriving — watch the green time extend
9. Type `s` to see live status, `1`/`2`/`3`/`4` for manual overrides

### Arduino / Tinkercad compile fix (for reference)

Tinkercad's compiler auto-inserts function prototypes after the last `#include`. If those
prototypes reference a custom struct type that isn't defined yet, compilation fails.

**The fix that works:** declare manual prototypes for **every** function in the sketch
(not just the ones that use custom types). When every function already has a prototype,
the auto-generator produces an empty block and the problem disappears.

---

## Project 2 — Linux Server Health Monitor

```bash
cd project2_server_monitor
chmod +x server_monitor.sh
./server_monitor.sh
```

### Testing checklist

- Option **1** — verify CPU / memory / disk numbers look real
- Option **2** — set CPU threshold to `1` (guaranteed to trigger), then run option 1 again — confirm warning line and log entry appear
- Option **3** — confirm log entries appeared with timestamps
- Option **5** — start background monitor; check option 3 after ~10 seconds
- Option **6** — stop background monitor
- Option **4** — clear logs
- Option **7** — exits cleanly

---

## Project 3 — Academic Records Analyzer

```bash
cd project3_academic_records
make
./academic_records
```

### Testing checklist

1. **Add** 3-4 students (option 1) with different courses and grades
2. **Display all** (option 2) — confirm table looks correct
3. **Sort by GPA** (option 7 -> 1) — highest GPA should appear first
4. **Sort by name** (option 7 -> 2) — check alphabetical order
5. **Search by name** (option 6 -> 2) — type a partial name, confirm match found
6. **Analytics -> Top N** (option 8 -> 3) — enter `2`, confirm top 2 appear
7. **Analytics -> Best per course** (option 8 -> 4)
8. **Save** (option 9) — creates `students.dat`
9. **Exit** (option 0) — auto-saves
10. Re-run `./academic_records` -> **Load** (option 10) -> confirm records came back
11. **Delete** a student (option 5), confirm they're gone, save again

---

## Project 4 — Data Analysis Toolkit

```bash
cd project4_data_toolkit
make
./data_toolkit
```

### Testing checklist

1. **Option 1** (manual input) — enter values: `5`, `3`, `9`, `1`, `7`, then a letter to stop
2. **Option 3** — display dataset, confirm values are there
3. **Option 4 -> Full statistics** (op 9) — check sum, mean, min, max, std dev
4. **Option 4 -> Sort ascending** (op 4) — display again, should be in order
5. **Option 4 -> Filter above** (op 7) — enter threshold `4`, confirm only values >4 shown
6. **Option 4 -> Transform -> Scale** (op 8 -> 1) — enter factor `2`, confirm all values doubled
7. **Option 5** — save to `results.txt`, verify with `cat results.txt`
8. **Option 6** — reset dataset, option 3 should show empty
9. **Option 2** — load back from `results.txt`, display to confirm

---

## Project 5 — Multi-threaded Web Scraper

```bash
cd project5_web_scraper
make
./scraper                                              # uses 5 built-in URLs
./scraper https://example.com https://httpbin.org/get  # custom URLs
```

### Testing checklist

- After running, check the summary table — all rows should show `OK`
- Check output files were created:
  ```bash
  ls scraped_pages/
  cat scraped_pages/page_01.txt   # should show the header + HTML content
  ```
- Test error handling with a bad URL:
  ```bash
  ./scraper https://example.com https://this-does-not-exist-xyz.fake
  ```
  The valid URL should show `OK`, the bad one `FAIL` with an error message
- Confirm parallel execution by watching the `[Thread XX] Starting` lines —
  they should appear out of order since threads race each other
