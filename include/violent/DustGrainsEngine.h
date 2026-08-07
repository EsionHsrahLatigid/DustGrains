#pragma once

#include "violent/ViolentDspPrimitives.h"

#include <array>
#include <cstdint>

namespace violent
{

struct DustGrainsParameters
{
    float densityHz = 80.0f;
    float durationSeconds = 0.018f;
    float positionJitter = 0.75f;
    float rate = 1.0f;
    float rateScatter = 0.6f;
    float stereoSpread = 0.8f;
    float outputGain = 0.55f;
};

class DustGrainsEngine
{
public:
    static constexpr int maximumGrains = 32;

    DustGrainsEngine();

    void prepare (double sampleRate) noexcept;
    void reset (std::uint32_t seed) noexcept;
    void setParameters (const DustGrainsParameters& newParameters) noexcept;
    void noteOn (int noteNumber, float velocity) noexcept;
    void noteOff (int noteNumber) noexcept;
    [[nodiscard]] StereoFrame processSample() noexcept;
    void process (float* left, float* right, int numSamples) noexcept;
    [[nodiscard]] int getActiveGrainCount() const noexcept;
    [[nodiscard]] std::uint64_t getTriggeredGrainCount() const noexcept { return triggeredGrains; }

private:
    static constexpr int sourceSize = 4096;
    static constexpr int sourceMask = sourceSize - 1;

    struct Grain
    {
        float position = 0.0f;
        float increment = 1.0f;
        float pan = 0.0f;
        int age = 0;
        int length = 1;
        bool active = false;
    };

    void rebuildSource() noexcept;
    void spawnGrain() noexcept;
    [[nodiscard]] float readSource (float position) const noexcept;

    DustGrainsParameters parameters;
    double sampleRate = 44100.0;
    std::uint32_t baseSeed = 1u;
    DeterministicNoise noise;
    TriggerEnvelope envelope;
    DcBlocker dcLeft;
    DcBlocker dcRight;
    std::array<float, sourceSize> source {};
    std::array<Grain, maximumGrains> grains {};
    std::uint64_t triggeredGrains = 0u;
    int currentNote = -1;
    bool gate = false;
};

} // namespace violent
