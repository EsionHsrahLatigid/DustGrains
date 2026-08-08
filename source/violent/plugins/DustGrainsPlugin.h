#pragma once

#include "violent/DustGrainsEngine.h"

#include <yup_audio_processors/yup_audio_processors.h>

#include <array>
#include <atomic>
#include <cstdint>

namespace violent::plugin
{

class DustGrainsPlugin final : public yup::AudioProcessor
{
public:
    DustGrainsPlugin();

    void prepareToPlay (const yup::AudioSpec& spec) override;
    void releaseResources() override;
    void processBlock (yup::AudioProcessContext<float>& context) override;
    void flush() override;

    int getNumVoices() const override;
    int getCurrentPreset() const noexcept override;
    void setCurrentPreset (int index) noexcept override;
    int getNumPresets() const override;
    yup::String getPresetName (int index) const override;
    void setPresetName (int index, yup::StringRef newName) override;

    yup::Result loadStateFromMemory (const yup::MemoryBlock& data) override;
    yup::Result saveStateIntoMemory (yup::MemoryBlock& data) override;

    bool hasEditor() const override;
    yup::AudioProcessorEditor* createEditor() override;

    void setStandaloneTriggerGate (bool shouldBeOpen) noexcept;
    [[nodiscard]] bool getStandaloneTriggerGate() const noexcept;
    [[nodiscard]] float getOutputPeak() const noexcept;
#ifdef DUSTGRAINS_HEADLESS_PLUGIN_TEST
    [[nodiscard]] std::uint64_t getTriggeredGrainCountForTesting() const noexcept;
#endif

private:
    enum ParameterIndex
    {
        density,
        duration,
        positionJitter,
        rate,
        rateScatter,
        stereoSpread,
        output,
        parameterCount
    };

    void advanceParameters (int samplePosition) noexcept;
    void applyEngineParameters() noexcept;
    void consumeStandaloneTriggerEdges() noexcept;
    void startStandaloneTrigger() noexcept;
    void releaseStandaloneTrigger() noexcept;
    void publishOutputPeak (float peak) noexcept;

    std::array<yup::AudioParameter::Ptr, parameterCount> parameters;
    std::array<yup::AudioParameterHandle, parameterCount> handles;
    std::array<float, parameterCount> smoothedValues {};
    violent::DustGrainsEngine engine;
    int activeMidiNote = -1;
    bool standaloneGateHeld = false;
    bool standaloneNoteActive = false;
    int controlCountdown = 0;
    std::atomic<int> currentPreset { 0 };
    std::atomic<int> desiredStandaloneGate { 0 };
    std::atomic<std::uint32_t> standalonePressEdges { 0u };
    std::atomic<std::uint32_t> standaloneReleaseEdges { 0u };
    std::atomic<std::uint32_t> outputPeakBits { 0u };
    std::uint32_t consumedStandalonePressEdges = 0u;
    std::uint32_t consumedStandaloneReleaseEdges = 0u;
    std::array<yup::String, 4> presetNames {
        "Dust Furnace",
        "Needle Weather",
        "Broken Surface",
        "Particle Siren"
    };
};

} // namespace violent::plugin
