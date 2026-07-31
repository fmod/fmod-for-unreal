# FMOD Sequencer Waveforms and Timeline Playback

<p align="center">
  <a href="https://youtu.be/P3qcxBJaG64">
    <img
      src="https://img.youtube.com/vi/P3qcxBJaG64/maxresdefault.jpg"
      alt="Watch the FMOD Sequencer Waveforms and Timeline Playback demo"
      width="900">
  </a>
</p>

<p align="center">
  <a href="https://youtu.be/P3qcxBJaG64"><strong>Watch the video demonstration on YouTube</strong></a>
</p>

This fork extends the official FMOD for Unreal integration with waveform visualization and timeline-aware audio playback in Unreal Engine Sequencer.



# FMOD Sequencer Waveforms and Timeline Playback

This fork extends the official FMOD for Unreal integration with waveform visualization and timeline-aware audio playback in Unreal Engine Sequencer.

It is intended for cinematics, dialogue, sound effects, music cues, and other workflows where FMOD audio needs to be positioned and edited directly against the Sequencer timeline.

Tested with:

- Unreal Engine 5.7.4
- FMOD Studio 2.03.11

## Features

### Real FMOD waveform previews

FMOD Event audio is captured and displayed directly inside its playback range in Sequencer.

The waveform is generated from the actual FMOD Event audio rather than from placeholder or procedurally generated data.

### Automatic waveform refresh

Waveforms are automatically discovered and generated when FMOD tracks or keys are added to an already open Sequencer.

There is no need to close and reopen Sequencer after adding or changing audio tracks.

### Loop visualization

Looping FMOD Events display a repeated waveform across their active playback range.

Each loop cycle is marked with a visual separator, making repeated audio easier to read, align, and trim.

The visible loop range ends at the first applicable boundary:

- a `Stop` command;
- the next `Play` command;
- the end of the Sequencer playback range.

### Playback from any timeline position

Starting Sequencer playback from the middle of the timeline starts the corresponding FMOD Event at the correct internal timeline offset.

Audio no longer restarts from the beginning every time Sequencer playback begins.

Supported behavior includes:

- timeline offsets for one-shot Events;
- modulo offsets for simple looping Events;
- independent playback for multiple FMOD Audio Components;
- normal native playback when starting exactly on a `Play` key;
- automatic cleanup when Sequencer is stopped or closed.

Scrubbing, jumping, stepping, and reverse transport do not inject audio playback.

### Clearer track names

The original integration used the word `Event` for both sound selection and playback control, which made the Sequencer setup difficult to understand.

The tracks are now displayed as:

- `Sound Track` — selects the FMOD Event asset;
- `Playback Track` — contains `Play`, `Stop`, and `Pause` commands.

The underlying `Event` property name, asset references, bindings, and serialized Sequence data remain unchanged.

## Sequencer setup

1. Add or bind an actor containing an `FMODAudioComponent`.
2. Expand the `FMODAudioComponent` in Sequencer.
3. Add a `Sound Track`.
4. Add a `Playback Track`.
5. Add a key to the Sound Track and select the required FMOD Event.
6. Add a `Play` command to the Playback Track at the frame where the sound should begin.

A `Stop` command is optional.

Use it when:

- the sound must end before its natural duration;
- a looping Event must stop at a specific frame;
- the visible waveform range needs an explicit ending.

Several sounds can be placed sequentially on the same FMOD Audio Component. Each selected sound should have its own corresponding `Play` command.

## Example timeline

```text
FMODAudioComponent
├── Playback Track    Play ---------------- Stop
└── Sound Track       Event A
```

For sequential sounds:

```text
Playback Track        Play A        Play B
Sound Track           Event A       Event B
```

Each sound selection must be paired with its own `Play` command.

## How waveform capture works

Waveform generation is editor-only.

The extension uses a separate persistent FMOD Studio `NOSOUND_NRT` system to capture peak data without playing the Event through the normal game audio output.

The capture service:

- loads the required FMOD banks;
- creates an isolated Event instance;
- captures audio through a DSP callback;
- converts the result into millisecond waveform peaks;
- caches completed waveforms for use by Sequencer.

This system is separate from normal runtime and auditioning playback.

## Timeline-aware playback

When explicit forward playback starts away from a `Play` key, the extension:

1. Resolves the exact bound `FMODAudioComponent`.
2. Finds the most recent `Play` command before the cursor.
3. Resolves the sound selected at that Play position.
4. Calculates the required FMOD timeline offset.
5. Starts the component-owned FMOD Event instance at that offset.

One-shot Events are not started after their natural duration.

Simple loop Events wrap the offset using the Event timeline length.

Ambiguous or invalid track configurations fail safely instead of playing an unrelated sound.

## Compatibility and scope

The extension is editor-focused and does not rename FMOD asset classes or serialized property identifiers.

Existing FMOD Event assets, Sequence bindings, and normal runtime playback remain compatible.

Timeline-aware seeking currently targets:

- standard one-shot Events;
- simple looping Events.

Complex loop logic, programmer sounds, and reverse audio playback are outside the current scope.
