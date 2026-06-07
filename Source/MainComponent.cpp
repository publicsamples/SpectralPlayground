#include "MainComponent.h"

namespace
{
class BufferPreviewSource final : public juce::PositionableAudioSource
{
public:
    void setBuffer(juce::AudioBuffer<float> newBuffer)
    {
        buffer = std::move(newBuffer);
        position = 0;
    }

    void prepareToPlay(int, double) override {}
    void releaseResources() override {}

    void getNextAudioBlock(const juce::AudioSourceChannelInfo& info) override
    {
        info.clearActiveBufferRegion();

        if (buffer.getNumSamples() <= 0)
            return;

        const auto remaining = buffer.getNumSamples() - (int) position;
        if (remaining <= 0)
            return;

        const auto numToCopy = juce::jmin(info.numSamples, remaining);

        for (int ch = 0; ch < info.buffer->getNumChannels(); ++ch)
        {
            const auto sourceChannel = juce::jmin(ch, buffer.getNumChannels() - 1);
            info.buffer->copyFrom(ch, info.startSample, buffer, sourceChannel, (int) position, numToCopy);
        }

        position += numToCopy;
    }

    void setNextReadPosition(juce::int64 newPosition) override
    {
        position = juce::jlimit<juce::int64>(0, getTotalLength(), newPosition);
    }

    juce::int64 getNextReadPosition() const override { return position; }
    juce::int64 getTotalLength() const override { return buffer.getNumSamples(); }
    bool isLooping() const override { return false; }

private:
    juce::AudioBuffer<float> buffer;
    juce::int64 position = 0;
};
}

MainComponent::MainComponent()
{
    for (auto* b : { &loadButton, &analyzeButton, &saveButton, &auditionSourceButton, &auditionAnalyzedButton })
    {
        addAndMakeVisible(*b);
        b->addListener(this);
    }

    for (auto* l : { &sourceLabel, &statusLabel, &diagnosticsLabel, &analysisSectionLabel, &playbackSectionLabel,
                     &presetLabel, &fftSizeLabel, &hopSizeLabel, &thresholdLabel, &maxPartialsLabel,
                     &toleranceLabel, &rootHzLabel, &noiseLevelLabel })
        addAndMakeVisible(*l);

    addAndMakeVisible(waveformPanel);
    addAndMakeVisible(presetBox);
    addAndMakeVisible(fftSizeBox);
    addAndMakeVisible(hopSizeSlider);
    addAndMakeVisible(thresholdSlider);
    addAndMakeVisible(maxPartialsSlider);
    addAndMakeVisible(toleranceSlider);
    addAndMakeVisible(rootHzSlider);
    addAndMakeVisible(noiseLevelSlider);

    sourceLabel.setText("No file loaded", juce::dontSendNotification);
    statusLabel.setText("Idle", juce::dontSendNotification);
    analysisSectionLabel.setText("Analysis", juce::dontSendNotification);
    analysisSectionLabel.setFont(juce::FontOptions(16.0f, juce::Font::bold));
    playbackSectionLabel.setText("Playback", juce::dontSendNotification);
    playbackSectionLabel.setFont(juce::FontOptions(16.0f, juce::Font::bold));
    diagnosticsLabel.setJustificationType(juce::Justification::topLeft);
    diagnosticsLabel.setColour(juce::Label::backgroundColourId, juce::Colours::black.withAlpha(0.25f));
    diagnosticsLabel.setColour(juce::Label::outlineColourId, juce::Colours::darkgrey);
    diagnosticsLabel.setBorderSize(juce::BorderSize<int>(8));
    diagnosticsLabel.setText("Diagnostics will appear after analysis.", juce::dontSendNotification);

    presetLabel.setText("Preset", juce::dontSendNotification);
    fftSizeLabel.setText("FFT Size", juce::dontSendNotification);
    hopSizeLabel.setText("Hop Size", juce::dontSendNotification);
    thresholdLabel.setText("Peak Threshold", juce::dontSendNotification);
    maxPartialsLabel.setText("Max Partials", juce::dontSendNotification);
    toleranceLabel.setText("Track Tolerance", juce::dontSendNotification);
    rootHzLabel.setText("Root Hz", juce::dontSendNotification);
    noiseLevelLabel.setText("Noise Level", juce::dontSendNotification);

    presetBox.addItem("Default", 1);
    presetBox.addItem("Drums / Percussive", 2);
    presetBox.addItem("Monophonic Melodic", 3);
    presetBox.addItem("Sustained Harmonic", 4);
    presetBox.onChange = [this] { applyAnalysisPreset(presetBox.getSelectedId()); };

    fftSizeBox.addItem("1024", 1);
    fftSizeBox.addItem("2048", 2);
    fftSizeBox.addItem("4096", 3);

    auto initSlider = [](juce::Slider& s, double min, double max, double value, double step)
    {
        s.setSliderStyle(juce::Slider::LinearHorizontal);
        s.setTextBoxStyle(juce::Slider::TextBoxRight, false, 80, 20);
        s.setRange(min, max, step);
        s.setValue(value);
    };

    initSlider(hopSizeSlider, 64, 2048, 512, 1);
    initSlider(thresholdSlider, -120.0, 0.0, -60.0, 0.1);
    initSlider(maxPartialsSlider, 8, 2048, 128, 1);
    initSlider(toleranceSlider, 1.0, 200.0, 25.0, 0.1);
    initSlider(rootHzSlider, 20.0, 2000.0, 220.0, 0.01);
    initSlider(noiseLevelSlider, 0.0, 1.0, 0.25, 0.001);

    audioFormatManager.registerBasicFormats();
    audioDeviceManager.initialise(0, 2, nullptr, true);
    audioDeviceManager.addAudioCallback(&audioSourcePlayer);
    analyzedPreviewSource = std::make_unique<BufferPreviewSource>();
    presetBox.setSelectedId(1, juce::sendNotificationSync);
}

MainComponent::~MainComponent()
{
    stopPlayback();
    audioSourcePlayer.setSource(nullptr);
    audioDeviceManager.removeAudioCallback(&audioSourcePlayer);

    for (auto* b : { &loadButton, &analyzeButton, &saveButton, &auditionSourceButton, &auditionAnalyzedButton })
        b->removeListener(this);
}

void MainComponent::resized()
{
    auto area = getLocalBounds().reduced(12);
    auto top = area.removeFromTop(34);
    loadButton.setBounds(top.removeFromLeft(120));
    analyzeButton.setBounds(top.removeFromLeft(100));
    saveButton.setBounds(top.removeFromLeft(100));
    auditionSourceButton.setBounds(top.removeFromLeft(130));
    auditionAnalyzedButton.setBounds(top.removeFromLeft(140));
    sourceLabel.setBounds(top);

    area.removeFromTop(8);
    auto right = area.removeFromRight(320);
    auto statusArea = area.removeFromBottom(24);
    auto diagnosticsArea = area.removeFromBottom(120);
    diagnosticsArea.removeFromTop(8);
    waveformPanel.setBounds(area);
    diagnosticsLabel.setBounds(diagnosticsArea);
    statusLabel.setBounds(statusArea);

    auto section = [&](juce::Label& label)
    {
        auto r = right.removeFromTop(26);
        label.setBounds(r);
        right.removeFromTop(6);
    };

    auto row = [&](juce::Label& label, juce::Component& comp)
    {
        auto r = right.removeFromTop(40);
        label.setBounds(r.removeFromLeft(120));
        comp.setBounds(r);
        right.removeFromTop(6);
    };

    section(analysisSectionLabel);
    row(presetLabel, presetBox);
    row(fftSizeLabel, fftSizeBox);
    row(hopSizeLabel, hopSizeSlider);
    row(thresholdLabel, thresholdSlider);
    row(maxPartialsLabel, maxPartialsSlider);
    row(toleranceLabel, toleranceSlider);
    row(rootHzLabel, rootHzSlider);
    right.removeFromTop(10);
    section(playbackSectionLabel);
    row(noiseLevelLabel, noiseLevelSlider);
}

void MainComponent::buttonClicked(juce::Button* button)
{
    if (button == &loadButton) loadFile();
    if (button == &analyzeButton) analyzeSelection();
    if (button == &saveButton) saveAsset();
    if (button == &auditionSourceButton) auditionSource();
    if (button == &auditionAnalyzedButton) auditionAnalyzed();
}

void MainComponent::loadFile()
{
    activeFileChooser = std::make_unique<juce::FileChooser>("Load audio file", juce::File(), "*.wav;*.aif;*.aiff");
    activeFileChooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                                   [this] (const juce::FileChooser& chooser)
                                   {
                                       const auto chosenFile = chooser.getResult();
                                       activeFileChooser.reset();
                                       if (!chosenFile.existsAsFile())
                                           return;

                                       currentFile = chosenFile;
                                       currentAsset = {};
                                       stopPlayback();
                                       waveformPanel.setFile(currentFile);
                                       sourceLabel.setText(currentFile.getFullPathName(), juce::dontSendNotification);
                                       statusLabel.setText("Loaded file", juce::dontSendNotification);
                                       diagnosticsLabel.setText("Selection: " + juce::String(waveformPanel.getSelectionRangeSeconds().getLength(), 3)
                                                                    + " s\nAnalyze to populate diagnostics.",
                                                                juce::dontSendNotification);
                                   });
}

void MainComponent::analyzeSelection()
{
    if (!currentFile.existsAsFile())
    {
        statusLabel.setText("Load a file first", juce::dontSendNotification);
        return;
    }

    statusLabel.setText("Analyzing...", juce::dontSendNotification);
    currentAsset = analysisEngine.analyzeFile(currentFile,
                                              getAnalysisSettings(),
                                              waveformPanel.getSelectionRangeSeconds());
    diagnosticsLabel.setText(buildDiagnosticsText(), juce::dontSendNotification);
    statusLabel.setText("Analysis complete: " + juce::String(currentAsset.frameCount) + " frames",
                        juce::dontSendNotification);
}

void MainComponent::saveAsset()
{
    if (currentAsset.frames.empty())
    {
        statusLabel.setText("Nothing to save yet", juce::dontSendNotification);
        return;
    }

    auto spectralDir = juce::File(SPECTRAL_PLAYGROUND_ASSET_DIR);
    spectralDir.createDirectory();

    auto out = spectralDir.getChildFile(currentFile.getFileNameWithoutExtension() + ".spectral.json");
    if (SpectralAssetWriter::writeJson(currentAsset, out))
        statusLabel.setText("Saved: " + out.getFileName(), juce::dontSendNotification);
    else
        statusLabel.setText("Failed to save asset", juce::dontSendNotification);
}

void MainComponent::auditionSource()
{
    if (!currentFile.existsAsFile())
    {
        statusLabel.setText("Load a file first", juce::dontSendNotification);
        return;
    }

    stopPlayback();
    auto sourceBuffer = readSourceSelectionBuffer();
    if (sourceBuffer.getNumSamples() <= 0)
    {
        statusLabel.setText("Failed to read source selection", juce::dontSendNotification);
        return;
    }

    if (auto* preview = dynamic_cast<BufferPreviewSource*>(analyzedPreviewSource.get()))
    {
        preview->setBuffer(std::move(sourceBuffer));
        preview->setNextReadPosition(0);
        activePlaybackSource = preview;
        audioSourcePlayer.setSource(activePlaybackSource);
    }

    statusLabel.setText("Auditioning source", juce::dontSendNotification);
}

void MainComponent::auditionAnalyzed()
{
    if (currentAsset.frames.empty())
    {
        statusLabel.setText("Analyze a file first", juce::dontSendNotification);
        return;
    }

    stopPlayback();

    auto previewBuffer = renderAnalyzedPreviewBuffer();
    if (previewBuffer.getNumSamples() <= 0)
    {
        statusLabel.setText("Preview render failed", juce::dontSendNotification);
        return;
    }

    if (auto* preview = dynamic_cast<BufferPreviewSource*>(analyzedPreviewSource.get()))
    {
        preview->setBuffer(std::move(previewBuffer));
        preview->setNextReadPosition(0);
        activePlaybackSource = preview;
        audioSourcePlayer.setSource(activePlaybackSource);
        statusLabel.setText("Auditioning analyzed preview", juce::dontSendNotification);
    }
}

void MainComponent::stopPlayback()
{
    fileTransportSource.stop();
    fileTransportSource.setSource(nullptr);
    fileReaderSource.reset();
    activePlaybackSource = nullptr;
    audioSourcePlayer.setSource(nullptr);
}

juce::AudioBuffer<float> MainComponent::renderAnalyzedPreviewBuffer() const
{
    if (currentAsset.frames.empty() || currentAsset.sampleRate <= 0.0)
        return {};

    const auto totalSamples = juce::jmax(1, (int) std::ceil(currentAsset.durationSeconds * currentAsset.sampleRate));
    juce::AudioBuffer<float> buffer(2, totalSamples);
    buffer.clear();

    const auto sampleRate = currentAsset.sampleRate;
    const auto twoPi = juce::MathConstants<double>::twoPi;
    juce::HashMap<int, double> phaseById;
    juce::Random random(0x53484e4f);
    const auto noiseLevel = noiseLevelSlider.getValue();
    double lastNoise = 0.0;
    double averageFrameEnergy = 0.0;

    for (const auto& frame : currentAsset.frames)
    {
        double frameEnergy = 0.0;
        for (const auto& partial : frame.partials)
            frameEnergy += (double) partial.amplitude;
        averageFrameEnergy += frameEnergy;
    }

    averageFrameEnergy /= juce::jmax(1, currentAsset.frameCount);

    for (int frameIndex = 0; frameIndex < currentAsset.frameCount; ++frameIndex)
    {
        const auto& frame = currentAsset.frames[(size_t) frameIndex];
        const auto frameStart = juce::jlimit(0, totalSamples, (int) std::round(frame.timeSeconds * sampleRate));

        auto nextTime = frame.timeSeconds + currentAsset.hopTime;
        if (frameIndex + 1 < currentAsset.frameCount)
            nextTime = currentAsset.frames[(size_t) frameIndex + 1].timeSeconds;

        const auto frameEnd = juce::jlimit(frameStart, totalSamples, (int) std::round(nextTime * sampleRate));
        if (frameEnd <= frameStart)
            continue;

        const auto partialCount = juce::jmin<int>((int) frame.partials.size(), 128);
        double transientEnv = 0.0;
        double frameNoiseSum = 0.0;
        double frameAmpSum = 0.0;

        for (int partialIndex = 0; partialIndex < partialCount; ++partialIndex)
        {
            const auto& partial = frame.partials[(size_t) partialIndex];
            frameNoiseSum += (double) partial.amplitude * (double) partial.bandwidth;
            frameAmpSum += (double) partial.amplitude;
        }

        const double frameNoise = frameAmpSum > 1.0e-9 ? frameNoiseSum / frameAmpSum : 0.0;
        const double activityNorm = juce::jlimit(0.0, 1.0, frameAmpSum / juce::jmax(1.0e-6, averageFrameEnergy * 0.85));
        const double energyComp = frameAmpSum > 1.0e-9
            ? juce::jlimit(1.0, 2.25, std::sqrt(averageFrameEnergy / frameAmpSum))
            : 1.0;

        for (int sample = frameStart; sample < frameEnd; ++sample)
        {
            double value = 0.0;

            for (int partialIndex = 0; partialIndex < partialCount; ++partialIndex)
            {
                const auto& partial = frame.partials[(size_t) partialIndex];
                auto phase = phaseById[partial.id];
                value += (double) partial.amplitude * std::sin(phase);
                phase += twoPi * (double) partial.frequency / sampleRate;

                if (phase >= twoPi)
                    phase = std::fmod(phase, twoPi);

                phaseById.set(partial.id, phase);
            }

            transientEnv = juce::jmax(transientEnv, (double) frame.transient);
            transientEnv *= 0.995;

            const double white = random.nextDouble() * 2.0 - 1.0;
            const double hpNoise = white - lastNoise;
            lastNoise = white;

            value *= energyComp;
            value *= 1.0 + (transientEnv * activityNorm) * 0.35;
            value += hpNoise * frameNoise * noiseLevel * activityNorm * (0.015 + transientEnv * 0.12);

            const auto out = (float) (value * 0.28);
            buffer.setSample(0, sample, out);
            buffer.setSample(1, sample, out);
        }
    }

    auto maxMagnitude = 0.0f;
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        maxMagnitude = juce::jmax(maxMagnitude, buffer.getMagnitude(ch, 0, buffer.getNumSamples()));

    if (maxMagnitude > 0.0001f)
        buffer.applyGain(0.8f / maxMagnitude);

    return buffer;
}

AnalysisSettings MainComponent::getAnalysisSettings() const
{
    AnalysisSettings s;
    s.fftSize = fftSizeBox.getText().getIntValue();
    s.hopSize = (int) hopSizeSlider.getValue();
    s.peakThresholdDb = (float) thresholdSlider.getValue();
    s.maxPartials = (int) maxPartialsSlider.getValue();
    s.trackToleranceHz = (float) toleranceSlider.getValue();
    s.rootHz = rootHzSlider.getValue();
    return s;
}

void MainComponent::applyAnalysisPreset(int presetId)
{
    switch (presetId)
    {
        case 2:
            fftSizeBox.setSelectedId(1, juce::dontSendNotification);
            hopSizeSlider.setValue(128, juce::dontSendNotification);
            thresholdSlider.setValue(-48.0, juce::dontSendNotification);
            maxPartialsSlider.setValue(96, juce::dontSendNotification);
            toleranceSlider.setValue(40.0, juce::dontSendNotification);
            rootHzSlider.setValue(220.0, juce::dontSendNotification);
            break;

        case 3:
            fftSizeBox.setSelectedId(2, juce::dontSendNotification);
            hopSizeSlider.setValue(256, juce::dontSendNotification);
            thresholdSlider.setValue(-66.0, juce::dontSendNotification);
            maxPartialsSlider.setValue(128, juce::dontSendNotification);
            toleranceSlider.setValue(14.0, juce::dontSendNotification);
            rootHzSlider.setValue(220.0, juce::dontSendNotification);
            break;

        case 4:
            fftSizeBox.setSelectedId(3, juce::dontSendNotification);
            hopSizeSlider.setValue(512, juce::dontSendNotification);
            thresholdSlider.setValue(-72.0, juce::dontSendNotification);
            maxPartialsSlider.setValue(192, juce::dontSendNotification);
            toleranceSlider.setValue(10.0, juce::dontSendNotification);
            rootHzSlider.setValue(220.0, juce::dontSendNotification);
            break;

        case 1:
        default:
            fftSizeBox.setSelectedId(2, juce::dontSendNotification);
            hopSizeSlider.setValue(512, juce::dontSendNotification);
            thresholdSlider.setValue(-60.0, juce::dontSendNotification);
            maxPartialsSlider.setValue(128, juce::dontSendNotification);
            toleranceSlider.setValue(25.0, juce::dontSendNotification);
            rootHzSlider.setValue(220.0, juce::dontSendNotification);
            break;
    }
}

juce::String MainComponent::buildDiagnosticsText() const
{
    if (currentFile == juce::File())
        return "No file loaded.";

    const auto selection = waveformPanel.getSelectionRangeSeconds();
    const auto selectionDuration = selection.getLength();
    juce::String text;
    text << "Selection start: " << juce::String(selection.getStart(), 3) << " s\n"
         << "Selection duration: " << juce::String(selectionDuration, 3) << " s\n"
         << "Asset start offset: " << juce::String(currentAsset.analysisStartSeconds, 3) << " s\n"
         << "FFT / hop: " << juce::String(currentAsset.fftSize) << " / "
         << juce::String(currentAsset.hopSizeSamples);

    if (currentAsset.frames.empty())
        return text + "\nNo analysis data.";

    const auto expectedFrames = currentAsset.hopSizeSamples > 0
        ? (int) std::ceil((selectionDuration * currentAsset.sampleRate) / (double) currentAsset.hopSizeSamples)
        : 0;

    int nonEmptyFrames = 0;
    int maxPartialsInFrame = 0;
    double averagePartials = 0.0;
    double averageTransient = 0.0;

    for (const auto& frame : currentAsset.frames)
    {
        const auto partialsInFrame = (int) frame.partials.size();
        if (partialsInFrame > 0)
            ++nonEmptyFrames;
        maxPartialsInFrame = juce::jmax(maxPartialsInFrame, partialsInFrame);
        averagePartials += partialsInFrame;
        averageTransient += frame.transient;
    }

    averagePartials /= juce::jmax(1, currentAsset.frameCount);
    averageTransient /= juce::jmax(1, currentAsset.frameCount);

    const auto firstFrameTime = currentAsset.frames.front().timeSeconds;
    const auto lastFrameTime = currentAsset.frames.back().timeSeconds;
    const auto coverageEnd = lastFrameTime + currentAsset.hopTime;

    text << "\nFrames actual / expected: " << juce::String(currentAsset.frameCount) << " / " << juce::String(expectedFrames)
         << "\nFrame time range: " << juce::String(firstFrameTime, 3) << " to " << juce::String(lastFrameTime, 3) << " s"
         << "\nEstimated coverage end: " << juce::String(coverageEnd, 3) << " s"
         << "\nNon-empty frames: " << juce::String(nonEmptyFrames)
         << "\nAvg / max partials: " << juce::String(averagePartials, 1) << " / " << juce::String(maxPartialsInFrame)
         << "\nMean transient: " << juce::String(averageTransient, 3)
         << "\nTracked partial IDs: " << juce::String(currentAsset.partialCount);

    return text;
}

juce::AudioBuffer<float> MainComponent::readSourceSelectionBuffer() const
{
    if (!currentFile.existsAsFile())
        return {};

    juce::AudioFormatManager sourceFormatManager;
    sourceFormatManager.registerBasicFormats();

    std::unique_ptr<juce::AudioFormatReader> reader(sourceFormatManager.createReaderFor(currentFile));
    if (reader == nullptr)
        return {};

    const auto selection = waveformPanel.getSelectionRangeSeconds();
    const auto totalDuration = (double) reader->lengthInSamples / reader->sampleRate;
    const auto start = juce::jlimit(0.0, totalDuration, selection.getStart());
    auto end = juce::jlimit(0.0, totalDuration, selection.getEnd());

    if (end <= start)
        end = totalDuration;

    const auto startSample = (juce::int64) std::floor(start * reader->sampleRate);
    const auto endSample = (juce::int64) std::ceil(end * reader->sampleRate);
    const auto numSamples = juce::jmax<juce::int64>(0, endSample - startSample);

    if (numSamples <= 0)
        return {};

    juce::AudioBuffer<float> buffer(juce::jmax(1, (int) reader->numChannels), (int) numSamples);
    buffer.clear();
    reader->read(&buffer, 0, (int) numSamples, startSample, true, true);
    return buffer;
}
