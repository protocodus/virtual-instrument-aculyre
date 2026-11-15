#!/bin/sh
# Renders the prototype's listening set into <output-directory>:
#
#   Tools/render_demo_set.sh <build-directory> <output-directory>
#
# The phrases come from Tools/write_preset_midi.py. Nothing here is committed:
# the renders are listening material for the feasibility review, with
# variants (no sympathy, the nylon glissando, the shared phrase on every
# preset, the dry reference A4) the README demos do not carry; those come
# from RenderAculyrePresetDemos.
set -eu
build=$1
out=$2
here=$(cd "$(dirname "$0")" && pwd)
render="$build/RenderAculyreDemo"
mkdir -p "$out/more"
python3 "$here/write_preset_midi.py" "$out/midi" > /dev/null
midi="$out/midi"

"$render" 0 "$midi/01-lyre-harp.mid" "$out/01-lyre-harp.wav" --tail 1.2
"$render" 1 "$midi/02-nylon-lyre-harp.mid" "$out/02-nylon-lyre-harp.wav" --tail 1.2
"$render" 2 "$midi/03-gut-lyre.mid" "$out/03-gut-lyre.wav" --tail 1.2

"$render" 0 "$midi/more/01-single-notes.mid" "$out/more/01-single-notes-lyre-harp.wav" --tail 1.2
"$render" 0 "$midi/more/01-single-notes.mid" "$out/more/01b-single-notes-lyre-harp-no-sympathy.wav" --tail 1.2 --no-sympathy
"$render" 0 "$midi/more/02-glissando.mid" "$out/more/02-glissando-lyre-harp.wav" --tail 1.2
"$render" 1 "$midi/more/02-glissando.mid" "$out/more/02b-glissando-nylon-lyre-harp.wav" --tail 1.2
"$render" 0 "$midi/more/03-dynamics.mid" "$out/more/03-dynamics-lyre-harp.wav" --tail 1.2
"$render" 0 "$midi/comparison/shared-phrase.mid" "$out/more/04a-shared-phrase-lyre-harp.wav" --tail 1.2
"$render" 1 "$midi/comparison/shared-phrase.mid" "$out/more/04b-shared-phrase-nylon-lyre-harp.wav" --tail 1.2
"$render" 2 "$midi/comparison/shared-phrase.mid" "$out/more/04c-shared-phrase-gut-lyre.wav" --tail 1.2
"$render" 0 "$midi/comparison/reference-a4.mid" "$out/more/05-reference-a4-lyre-harp-dry.wav" --tail 1.2 --dry
