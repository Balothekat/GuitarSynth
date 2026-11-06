# GuitarLink 0.4 — measured D-A-E-A-C#-E profile
A separate measured-tuning chord mode, alongside unchanged Fast Lead, Stable Lead,
and the original experimental chord mode. The profile was extracted from six
user-provided open-string notes. This is specific to that instrument and tuning.

The new tracker fits/removes strong partials before looking for quieter notes,
maps measured frequencies to string/fret candidates, requires harmonic support
and temporal stability, and runs analysis on a worker thread in live operation.
It does not invent missing octave duplicates. A chord can yield four distinct
fundamentals rather than all six sounding string notes.

Native replay of the provided F# minor 7 recording recovered MIDI 42/49/52/57
with no extra note-ons in the initial test. Final regression results are saved
with the local analysis report. The C#4/E4 octave duplicates are still omitted.
Trigger times and CPU figures must be reported from the final compiled replay.
This accuracy mode is slower than Fast Lead; it does not achieve zero latency.

Known limits: one chord and six isolated string recordings are limited coverage.
Retuning can invalidate the measured profile. Per-string bending, MPE, real-guitar
voicing-generalization, and full six-note transcription are not solved.
No original audio recordings are stored in the repository.
