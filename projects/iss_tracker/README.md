# ISS Tracker

A live 3D globe on ESP-Mosaico that follows the International Space Station:
where it is, where it has been, and where it is going next.

![ISS Tracker](media/cover.png)

## Features

- **Live position:** the ISS floats above the globe and casts a shadow.
- **Orbit track:** the last 45 minutes and the next 90 minutes.
- **Day and night:** the night side is shaded from the real sun position.
- **Live readout:** country or ocean below, latitude, longitude, altitude,
  speed and UTC time.
- **Touch to spin:** drag the globe; it returns to the ISS after a few seconds.

## Install

From the workspace root, with the board connected by USB:

```sh
# Option A: prebuilt firmware from the repo's release/ folder
python mosaico.py iris system-update --bundle iss_tracker-system-update.irisfw

# Option B: build from source
python mosaico.py iris system-update --project projects/iss_tracker
```

A new or blank board needs `python mosaico.py recover` once first.

Preview on the PC without a board:

```sh
python mosaico.py project sim --project projects/iss_tracker --interactive
```

## Web monitor and logs

```sh
python mosaico.py iris run --project projects/iss_tracker    # prints a web workbench URL
python mosaico.py iris logs --project projects/iss_tracker   # live device logs
```

Open the printed URL in your browser to watch the device. Press Ctrl+C to stop.

## Requirements

- Wi-Fi saved in Vibe Mode. The app reuses it.
- Internet access for the clock and orbit data. The ISS appears once the
  clock is set.

## How it works

- Orbit data (TLE) comes from [CelesTrak](https://celestrak.org/) every
  6 hours and is saved on the device.
- Positions are computed on the device with SGP4 (`main/sgp4.c`).
- Accuracy slowly drops as the saved orbit data gets older.

## Credits

- Map: [world-atlas](https://github.com/topojson/world-atlas) (ISC), from
  [Natural Earth](https://www.naturalearthdata.com/) (public domain).
  Regenerate with `tools/gen_world_map.py`.
- Fonts: DejaVu, see `ui/fonts/LICENSE`.
