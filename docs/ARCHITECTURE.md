# KindPath Q Architecture

## Modules
- `core/analysis`: FFT spectrum, RMS/loudness, crest factor, correlation, transient density, and feel tags.
- `core/dsp`: Small DSP utilities (mono collapse).
- `core/education`: Education card data loader and model.
- `ui`: Shared JUCE components (transport bar, waveform, analysis panel, education panel, main layout).
- `apps/standalone`: JUCE standalone app using the shared UI and analysis engine.
- `plugins/kindpath-q`: JUCE plugin (AU/VST3) using the same analysis engine and UI shell.
- `scripts`: Bootstrap and build helpers.
- `assets`: Visual and audio assets (placeholders now).

## Data Flow
- Audio comes from the standalone transport source or plugin input bus.
- `AnalysisEngine` runs per block and stores a snapshot (spectrum, RMS, crest, correlation, transient density).
- UI polls snapshots on a timer and renders meters + spectrum bars.
- Education cards are loaded from JSON and rotated on user interaction.

## File Import / Sharing

- Accepted formats: WAV, AIFF, MP3, FLAC, OGG, CAF, MP4/M4A (whatever `registerBasicFormats()` plus platform decoders support).
- **Standalone**: "Load Audio" button opens a file picker; files can also be dragged from Finder directly onto the waveform area.
- **Plugin**: "Load Audio" button and drag-and-drop are both enabled for a *reference waveform* — the file is displayed visually only. Live audio analysis continues from the DAW input bus via `processBlock`.
- `WaveformView` implements `juce::FileDragAndDropTarget` and exposes an `onFileDropped` callback; `KindPathMainComponent` exposes `setOnFileDrop` to wire callers in.


## Shared Boundaries
- `AnalysisEngine` is shared across app and plugin.
- UI is shared and wired to analysis + education data in each target.
