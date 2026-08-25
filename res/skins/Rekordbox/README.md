<!-- CUSTOM MOD (rekordbox-skin): Rekordbox-style layout for local Mixxx build -->

# Rekordbox skin (v1) for Mixxx 2.7 — local build

A Rekordbox-style legacy skin: dark grey `#1a1a1a` background, white labels,
blue accent `#00a0e9`. Layout patterns were adapted from
`res/skins/LateNight` (same project, GPL); all files here are fresh.

## File tree

```
res/skins/Rekordbox/
├── skin.xml            root <Skin>, manifest, 5 rows + library
├── fxunit.xml          ROW 1 template: compact Beat FX unit panel (FxUnit var)
├── cfx.xml             ROW 2 template: per-deck CFX mini panel (ChanNum var)
├── deckpod.xml         ROW 3 template: deck pod (ChanNum var)
├── mixer.xml           ROW 4 template: channel strip (ChanNum var)
├── padfx.xml           ROW 5 template: Pad FX panel (EffectUnit4)
├── library.xml         search box + sidebar + track table
├── qss/Rekordbox.qss   minimal dark stylesheet
├── README.md
└── graphics/
    ├── btn.svg                     authored text SVG, button idle face
    ├── btn_active.svg              authored text SVG, blue active/pressed face
    ├── knobs/*.svg                 copied from LateNight/palemoon/knobs (6)
    └── sliders/*.svg               copied from LateNight/palemoon/sliders (4)
```

## What works in v1

- **ROW 1 – Beat FX racks**: compact panels for `[EffectRack1_EffectUnit1]`
  and `[EffectRack1_EffectUnit2]`: per-slot enable toggle + meta knob +
  effect selector combo (`WEffectSelector`), plus dry/wet (`mix`), super
  (`super1`) and chain-preset button (`WEffectChainPresetButton`) per unit.
  Same mechanism LateNight uses (no `<Window>` element exists in the legacy
  format; the main layout simply fills the window like LateNight).
- **ROW 2 – CFX row** (one panel per deck 1–4): DEFAULT / SPACE / DUB ECHO /
  FLANGER / CRUSH buttons that load into
  `[QuickEffectRack1_[ChannelN]_EffectSlot1]`, a macro knob and an FX ON gate.
- **ROW 3 – Deck pods x4**: `WOverview` waveform bound to `[ChannelN]`,
  PLAY/CUE/SYNC transport, horizontal rate fader with reset, hotcue pads
  1–4 (single row; a full 3x4 grid is deferred).
- **ROW 4 – Center mixer**: HI/MID/LOW EQ knobs
  (`[EqualizerRack1_[ChannelN]_Effect1],parameter3/2/1`), FILTER knob
  (`[QuickEffectRack1_[ChannelN]],super1`), volume fader, headphone CUE
  (`pfl`), master volume and crossfader in the middle.
- **ROW 5 – Pad FX**: `[EffectRack1_EffectUnit4_Effect1/2/3],enabled`
  toggles (SPACE/REVERB/FLANGER), routing buttons
  `[EffectRack1_EffectUnit4],group_[Channel1/2]_enable`, and EDIT toggle on
  `[Controls],PadFxEditMode`.
- **Library**: search box + sidebar + track table (LateNight's split, trimmed).

## CFX button mechanism (chosen approach + limitation)

`EffectSlot` exposes a 1-indexed loader CO:

```cpp
// src/effects/effectslot.cpp
m_pControlLoadedEffect = std::make_unique<ControlObject>(
        ConfigKey(m_group, "loaded_effect"));
m_pControlLoadedEffect->connectValueChangeRequest(
        &EffectSlot::slotLoadedEffectRequest);
...
void EffectSlot::slotLoadedEffectRequest(double value) {
    // ControlObjects are 1-indexed
    int index = static_cast<int>(value) - 1;
    ...
    loadEffectWithDefaults(m_pVisibleEffects->at(index));
}
```

A legacy `PushButton` only emits 1 on press / 0 on release, so each CFX
button routes its press through a connection value transformer
(`<Transform><Add>k-1</Add></Transform>`, parsed in
`legacyskinparser.cpp setupConnections()` → `ValueTransformer`) making the
press emit exactly `k` into `loaded_effect`. DEFAULT uses the slot's
`clear` pushbutton CO instead. The macro knob is bound to the rack-level
`[QuickEffectRack1_[ChannelN]],super1` (the slot has no `super1`; slots
expose `meta`, racks expose `super1` — same function, proven binding from
LateNight's quick_effect_knob templates).

**Limitation:** `loaded_effect` indexes the *visible effects list*, whose
order depends on the user's effects settings. On a stock fresh config all
built-in effects are visible and the list is built by prepending the
lexicographically sorted manifests (`EffectsBackendManager::
getManifestsForBackend()` sorts by display name, then
`VisibleEffectsList::readEffectsXml()` *prepends* unknown ones), i.e.
reverse-alphabetical. The hardcoded indices assume exactly that stock order
(including Pitch Shift present):

SPACE→5 (Reverb), DUB ECHO→19 (Echo), FLANGER→17 (Flanger),
CRUSH→21 (Bitcrusher).

If you hide/show effects or install plugin backends, adjust the `<Add>`
values in `cfx.xml` to match your visible order (Preferences → Effects shows
the list; index = position from the top). A name-based loader would require
a controller script or C++ change — deferred.

## Asset notes

- No binary images generated or copied. Button faces are two small
  hand-authored SVGs (`graphics/btn.svg`, `btn_active.svg`).
- Knob/slider artwork is copied verbatim from `LateNight/palemoon`
  (10 small SVGs listed above) because `KnobComposed`/`WSliderComposed` need
  indicator/handle pixmaps; they are referenced as local
  `skins:Rekordbox/graphics/...` paths.

## Deferred (not in v1)

- Full 3x4 hotcue/loop pad grid (single row of 4 hotcues shipped).
- Jog wheels / spinnies, vinyl controls, beatjump/loop controls.
- Sampler and mic/aux panels.
- Momentary (hold-to-FX) behavior and LED-style latching visuals for CFX
  buttons; per-effect parameter macros beyond super1.
- Name-based (index-independent) effect loading.
- Skin settings pane, maximized-library view, preview decks.
- `[Controls],PadFxEditMode` does nothing until the parallel C++ work lands;
  binding a missing CO is skipped harmlessly at load time.
