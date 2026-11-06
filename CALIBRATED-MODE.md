# Measured D–A–E–A–C♯–E mode (experimental)

This accuracy mode uses numeric string profiles extracted from six clean open-string recordings. It fits strong partials, looks for corroborated fundamentals, maps them to feasible string/fret positions, and suppresses harmonics. Its live analysis runs on a worker thread; the audio callback never waits for the analysis. This is strum-oriented: new tones need an attack. It is not a general six-note guitar transcription system.

## Recorded regression
A separate F♯ minor 7 example used frets 4–4–0–0–0–0 in the confirmed tuning. Sequential native replay found MIDI 42 (F♯2), 49 (C♯3), 52 (E3) and 57 (A3), once each, with no other note-ons. First onsets occurred at 0.244 seconds for 42/52/57 and 0.279 seconds for 49, measured from the start of the recording. All four remained held to the mute. The upper C♯4 and E4 duplicates were omitted: they overlap lower strings' harmonics and are not guessed.

All six isolated open strings were recognized once each without extra note-ons. Synthetic measured-profile strings and a four-tone chord mixture pass at 44.1 and 48 kHz. These are limited examples, not a broad accuracy benchmark.

## Latency and CPU
The 2048-sample downsampled window spans about 93 ms at 44.1 kHz (85 ms at 48 kHz). Analysis runs every 768 downsampled samples and needs temporal confirmation. Scheduling, the interface and the receiving synth add delay. It does not achieve zero or near-zero latency. Fast Lead is preferable for latency-sensitive single-note playing.

The paced worker test feeds actual recordings in 128-sample blocks using a clock. macOS's audio-feeder priority is approximated with user-interactive QoS and the worker uses user-initiated QoS; neither test thread is a real-time audio callback. A queue overflow clears stale analysis instead of blocking the audio thread and can cause notes to cut off or retrigger. High system load can still break live tracking. Test the VST in Ableton with a light session; host performance remains to be validated by the player.

## Profile pitches
The measured open frequencies are approximately 71.160, 108.615, 163.260, 216.397, 274.651 and 327.449 Hz. These reflect the guitar's recorded pitches, including tuning offsets; they are not exact A440 equal-temperament D–A–E–A–C♯–E. Retuning can invalidate the profile. This version does not yet offer automatic recalibration.

Per-string bending, MPE, legato tracking, arbitrary-voicing generalization, repeated same-note articulation, and full six-note transcription remain unresolved. No original recordings are stored in the repository.
