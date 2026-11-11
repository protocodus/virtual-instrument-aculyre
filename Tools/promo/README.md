# Three-dance promo

Builds Aculyre's Full HD promo, its three 9:16 shorts and their thumbnails,
after Respira's `Tools/promo/`: the Minuet in G on the Nylon Lyre Harp patch,
The Kesh on the Gut Lyre patch and The Butterfly on the Lyre Harp patch, each
from a late strain to its final chord. Rendered audio and video are review
deliverables and are never committed.

| File | Does |
| --- | --- |
| `repertoire.py` | writes the three performed pieces as MIDI, one lyre each, with `write_preset_midi.py`'s `Performance`; the tunes' sources are in its docstring |
| `SongRender.cpp` | plays one piece through `aculyre::Performer` set as a Rack factory patch is; writes the WAV and a 60 Hz trace of every string's energy and state (the engine's read-only `stringActivity`) |
| `promo_audio.py` | `promo` cuts the three excerpts, matches their loudness, joins and masters them to -16 LUFS; `short-minuet`, `short-kesh`, `short-butterfly` make one excerpt each, under 30 s; writes `out/<set>/timeline.json` |
| `promo.mjs` | draws the film from `timeline.json` and muxes the audio (H.264 CRF 18, AAC 384 kbps); `--portrait` draws a 1080×1920 short with every word in the project's TikTok safe box |
| `thumbs.mjs` | three 3840×2160 promo thumbnail concepts and the shorts' 9:16 covers |

```sh
# from the repository root, into a work directory <w> beside an assets directory <w>/../assets
python3 Tools/promo/repertoire.py <w>/out
g++ -std=c++17 -O3 -DNDEBUG Tools/promo/SongRender.cpp Source/DSP/AculyreEngine.cpp Source/DSP/AculyrePerformer.cpp -o <w>/SongRender
cd <w>
./SongRender "Nylon Lyre Harp" out/minuet/minuet.mid out/minuet/minuet.wav out/minuet/minuet.csv --oversampling 4
./SongRender "Gut Lyre" out/kesh/kesh.mid out/kesh/kesh.wav out/kesh/kesh.csv --oversampling 4
./SongRender "Lyre Harp" out/butterfly/butterfly.mid out/butterfly/butterfly.wav out/butterfly/butterfly.csv --oversampling 4
python3 <repo>/Tools/promo/promo_audio.py promo     # and short-minuet, short-kesh, short-butterfly
node promo.mjs out/promo/timeline.json out/promo/promo.wav out/promo/promo.mp4
node promo.mjs out/short-kesh/timeline.json out/short-kesh/short-kesh.wav out/short-kesh/short.mp4 --portrait
node promo.mjs out/promo/timeline.json out/promo/promo.wav still --still 12   # the frame thumbs.mjs uses
node thumbs.mjs thumbs still-12.png
```

The `.mjs` scripts run from the work directory (copy them there) and need
`@napi-rs/canvas`, Inter in `/usr/share/fonts/opentype/inter/`, and in
`../assets/` the Libre Baskerville the Rack panel uses
(`reason-rack-extensions/Examples/Aculyre/Design/Fonts/LibreBaskerville-wght.ttf`)
and the Protocodus logo as `logo.png` (`Examples/Aculyre/Design/Brand/`);
`thumbs.mjs` also needs the Codex hero art `landscape.png` and `portrait.png`
(project files `aculyre/promo-2026-11-11/thumbs/codex-art/`, with their
prompts). `promo_audio.py` needs NumPy, SciPy, soundfile and pyloudnorm.
