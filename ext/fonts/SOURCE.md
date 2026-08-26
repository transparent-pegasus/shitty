# Noto Color Emoji test font

`NotoColorEmoji.ttf` and `LICENSE.NotoColorEmoji` come from the official
Google Fonts Noto Emoji repository at commit
`8998f5dd683424a73e2314a8c1f1e359c19e8742`.

Source: <https://github.com/googlefonts/noto-emoji>

SHA-256:

- `NotoColorEmoji.ttf`:
  `72a635cb3d2f3524c51620cdde406b217204e8a6a06c6a096ff8ed4b5fd6e27b`
- `LICENSE.NotoColorEmoji`:
  `500bb1ccf43df7bbb522112f9133a52b16e1c35e809632f5d8609b179152de5b`

The font is used only as a deterministic real-font rendering fixture. Its
license is the SIL Open Font License included alongside it.

# Amiri test font

`Amiri-Regular.ttf` and `LICENSE.Amiri` come from the Google Fonts
repository at commit `39d11bc313031c9f68e21a297ce5e4a15cc5365e`
(`ofl/amiri/`).

Source: <https://github.com/google/fonts>

SHA-256:

- `Amiri-Regular.ttf`:
  `ab391c4147d054c48976e98322ad0eefe1427aa0e0502a12a4c75d80a70cfcd7`
- `LICENSE.Amiri`:
  `72de68e5954f4fdd24702292ef5a32f003ca960ec9330dc86e5eefb5dffb9b22`

The font covers U+FDFD, the widest ligature glyph in Unicode: its ink
spans several cells while its terminal width stays one column. Tests use
it to pin how a span's ink overflows into the captured blanks behind it.
Its license is the SIL Open Font License included alongside it.

# Bitmap strike test fonts

`spleen-8x16.bdf` and `LICENSE.spleen` come from the Spleen 2.1.0
release tarball (BSD 2-Clause).

Source: <https://github.com/fcambus/spleen>

`cozette.bdf` and `LICENSE.cozette` come from the Cozette v.1.25.2
release (MIT).

Source: <https://github.com/slavfox/Cozette>

SHA-256:

- `spleen-8x16.bdf`:
  `b2b05484ba4380c9c6854cc57637235a9fb039c65b5ba38a65289540f8fab7dc`
- `LICENSE.spleen`:
  `3cb3f3f5a795547d329828df881c80d923bfc75f65065c44ffa703ad78c678bd`
- `cozette.bdf`:
  `43baab1829398c6b33d02329611ef8d3ee9496c531809d8f30feb212ecaf9d3f`
- `LICENSE.cozette`:
  `cef7a837b15237cf9bbc74d691e1c8c479f4292413ca2319c8e3b145d6fc980b`

`fixture-8x8.bdf` is our own three-glyph hand-written fixture with
bit-exact expectations in tst/test_bitmap_font_render.py.

The bitmap fonts exercise the non-scalable strike path of the FreeType
backend: fixed sizing, strike-metric baselines and monochrome drawing.
