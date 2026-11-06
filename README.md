# GuitarLink 0.4
Experimental guitar-to-MIDI VST3 and test synth for macOS 14 or newer, Intel and Apple Silicon. Built on GitHub; development tools are not needed on the player's Mac. Original code and interface; not affiliated with GuitarCatcher or Nudist Audio.

## Install and route in Ableton
1. Quit Live. Extract the release ZIP and replace GuitarLink.vst3 in your existing VST3 scan folder, commonly ~/Library/Audio/Plug-Ins/VST3/.
2. Restart Live and rescan VST3 plug-ins. Load GuitarLink on an audio track receiving a clean, mono guitar DI before effects.
3. Select a tracking mode at the top of the plug-in. Use the built-in synth to check input, then route a MIDI track's MIDI From to the GuitarLink audio track and its GuitarLink output; set monitoring to In and load your synth there.
4. Turn GuitarLink's Dry and Synth levels down when using an external synth. Use Mute / stop all notes before changing routes.

macOS may require approval for this ad-hoc signed test build. It is not notarized. Do not disable system security globally. Avoid loading duplicate copies of the same VST3 from multiple scan folders.

## Tracking modes
- Fast Lead: single-note live playing with the shortest analysis path.
- Stable Lead: single-note playing with more smoothing.
- Chords (experimental): older general-purpose chord prototype.
- DAEAC#E / measured: new strum-oriented accuracy mode calibrated from six open-string recordings in D–A–E–A–C♯–E tuning. See CALIBRATED-MODE.md.

The measured mode is specific to the recorded guitar and its actual string pitches. It identifies supported fundamentals and suppresses harmonics instead of guessing all six sounding strings. Octave duplicates may be omitted. New tones need an attack; legato, hammer-ons and bends are not reliable. Fast Lead remains preferable for latency-sensitive single notes.

## Latency and limits
Zero latency is not achievable. The measured mode uses a roughly 85–93 ms analysis window plus confirmation and asynchronous processing; interface buffers and the receiving synth add further delay. Lowering Ableton's buffer cannot remove the detector's own delay. Try 128 samples, then 64 if your system plays without dropouts. Measured-mode accuracy and CPU have limited recorded-guitar coverage and are not established for arbitrary chords or guitars.

The plug-in includes a small six-voice test synth, ADSR, dry/synth mix, gate, lead pitch bend, keyboard and pitch display. No sampler, MPE or effects rack. It is a prototype, not equivalent to a mature commercial polyphonic guitar tracker. No original guitar recordings are stored in this repository.

## Build and verification
The GitHub workflow builds JUCE 8.0.10 with CMake/Xcode on its Mac runner, runs core/string/chord/worker tests and full processor MIDI/synth checks, renders the actual editor, and checks signing and macOS 14 compatibility. The calibration workflow also builds a standalone local AIFF replay tool, including a paced worker mode. Passing tests does not imply Ableton host validation. Review JUCE's license before distribution.
