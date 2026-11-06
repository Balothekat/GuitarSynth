# GuitarLink 0.1 — experimental live guitar to MIDI
Original JUCE/C++ prototype inspired by the requested guitar-controller workflow.
Not affiliated with Nudist Audio. No GuitarCatcher code, assets, or algorithms used.

STATUS: source only. Not compiled, not validated in Ableton, no measured latency.
No finished VST3 is included. Generic parameter editor, not the pictured UI.
Monophonic only; no chord recognition, sampler, effects rack, or MPE.

## What is implemented
Streaming pitch estimation; amplitude-derived MIDI velocity; optional +/-2-semitone
channel pitch bend; built-in sine/harmonic test synth; dry mix; gate; note-off after
several missing estimates; mute/panic toggle; mono/stereo input support.
The tracker uses fixed-size storage and performs no heap allocation.
JUCE MIDI event insertion may allocate; hard real-time suitability is unverified.

## Latency
Zero latency is not achievable. A low E period is about 12.1 ms.
Detection uses past samples and typically needs several periods. The exact onset delay,
CPU cost, false-note rate, audio interface delay and synth response are UNMEASURED.
There is no explicit audio lookahead or delayed dry-audio buffer.
Start with 48 kHz and a 64/128 sample interface buffer and test for dropouts.
These are starting settings, not a latency guarantee.

## Build without installing development tools on your Mac
Upload the CONTENTS of this project to a GitHub repository, including .github.
The workflow compiles on GitHub's Mac runner and runs synthetic checks.
Download the GuitarLink-Mac artifact and extract the ZIP inside it.
You still need working GitHub repository access; this workflow has not been run.
CMake and Apple tools are required on the build runner, not on your Mac.

## After a successful build
Copy GuitarLink.vst3 into ~/Library/Audio/Plug-Ins/VST3/ and rescan Live.
Load on an audio track receiving your clean guitar DI, before distortion/reverb.
Use a mono input selector for Scarlett's guitar channel. Begin with the built-in synth.
For external instruments, route the plugin's MIDI output to a MIDI track if supported
by your Live version/plugin configuration. That routing has not been verified here.
Set receiver pitch bend range to +/-2 semitones. Turn pitch bend OFF for discrete
note articulation: with bend ON, movements under two semitones retain the base note.
If tracking jitters, raise the gate and check the DI level. Avoid clipping.
Mute / stop all notes silences audio and emits note-offs while enabled.
Release the toggle to resume. Disable before changing MIDI routing.
Disabling/removing the device while notes are held can still require your synth's panic.

## Known limitations / next engineering work
Picking the same pitch again during a sustained note does not retrigger MIDI.
Smooth bends and adjacent newly picked notes are ambiguous in bend mode.
No polyphony; overlapping strings and distorted signals can yield octave errors.
Averaging downsampling is a prototype filter, not a high-quality antialiasing stage.
Only synthetic steady-tone/silence tests are included. Recorded DI, note transitions,
onset/offset timing, bend trajectories, MIDI lifecycle and CPU need validation.
A custom keyboard/tracking display and sampler UI remain future work.
JUCE is a third-party dependency; review its license before distribution.
