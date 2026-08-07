#include "violent/DustGrainsEngine.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace violent
{

DustGrainsEngine::DustGrainsEngine()
{
    prepare (44100.0);
}

void DustGrainsEngine::prepare (double newSampleRate) noexcept
{
    sampleRate = std::isfinite (newSampleRate) && newSampleRate > 1.0 ? newSampleRate : 44100.0;
    envelope.prepare (sampleRate);
    dcLeft.prepare (sampleRate);
    dcRight.prepare (sampleRate);
    reset (baseSeed);
}

void DustGrainsEngine::reset (std::uint32_t seed) noexcept
{
    baseSeed = seed != 0u ? seed : 1u;
    noise.reset (baseSeed);
    envelope.reset();
    dcLeft.reset();
    dcRight.reset();
    for (auto& grain : grains)
        grain = {};
    triggeredGrains = 0u;
    currentNote = -1;
    gate = false;
    rebuildSource();
}

void DustGrainsEngine::setParameters (const DustGrainsParameters& newParameters) noexcept
{
    parameters.densityHz = clampFinite (newParameters.densityHz, 0.1f, 1200.0f, 80.0f);
    parameters.durationSeconds = clampFinite (newParameters.durationSeconds, 0.001f, 0.25f, 0.018f);
    parameters.positionJitter = clampFinite (newParameters.positionJitter, 0.0f, 1.0f, 0.75f);
    parameters.rate = clampFinite (newParameters.rate, 0.125f, 8.0f, 1.0f);
    parameters.rateScatter = clampFinite (newParameters.rateScatter, 0.0f, 1.0f, 0.6f);
    parameters.stereoSpread = clampFinite (newParameters.stereoSpread, 0.0f, 1.0f, 0.8f);
    parameters.outputGain = clampFinite (newParameters.outputGain, 0.0f, 2.0f, 0.55f);
    envelope.setTimes (0.001f, 2.5f);
}

void DustGrainsEngine::noteOn (int noteNumber, float velocity) noexcept
{
    const auto incomingNote = std::clamp (noteNumber, 0, 127);
    const auto incomingVelocity = clampFinite (velocity, 0.0f, 1.0f, 1.0f);
    if (incomingVelocity <= 0.0f)
    {
        noteOff (incomingNote);
        return;
    }

    currentNote = incomingNote;
    gate = true;
    noise.reset (baseSeed ^ (static_cast<std::uint32_t> (currentNote + 1) * 0x9e3779b9u));
    envelope.trigger (incomingVelocity);
    spawnGrain();
}

void DustGrainsEngine::noteOff (int noteNumber) noexcept
{
    if (std::clamp (noteNumber, 0, 127) == currentNote)
    {
        gate = false;
        envelope.release();
    }
}

StereoFrame DustGrainsEngine::processSample() noexcept
{
    const auto densityProbability = parameters.densityHz / static_cast<float> (sampleRate);
    const auto draw = 0.5f * (noise.nextFloat() + 1.0f);
    if (gate && draw < densityProbability)
        spawnGrain();

    float left = 0.0f;
    float right = 0.0f;
    int active = 0;

    for (auto& grain : grains)
    {
        if (! grain.active)
            continue;

        const auto phase = static_cast<float> (grain.age) / static_cast<float> (std::max (1, grain.length));
        if (phase >= 1.0f)
        {
            grain.active = false;
            continue;
        }

        const auto window = std::sin (std::numbers::pi_v<float> * phase);
        const auto asymmetricWindow = window * (0.35f + 0.65f * (1.0f - phase));
        const auto sample = readSource (grain.position) * asymmetricWindow;
        const auto pan = std::clamp (grain.pan * parameters.stereoSpread, -1.0f, 1.0f);
        left += sample * std::sqrt (0.5f * (1.0f - pan));
        right += sample * std::sqrt (0.5f * (1.0f + pan));

        grain.position += grain.increment;
        if (grain.position >= static_cast<float> (sourceSize))
            grain.position -= static_cast<float> (sourceSize);
        else if (grain.position < 0.0f)
            grain.position += static_cast<float> (sourceSize);
        ++grain.age;
        ++active;
    }

    const auto normalization = 1.0f / std::sqrt (static_cast<float> (std::max (1, active)));
    const auto amplitude = envelope.process() * parameters.outputGain * normalization;
    constexpr float ceiling = 0.95f;
    return { std::clamp (boundedDrive (dcLeft.process (left * amplitude), 1.6f), -ceiling, ceiling),
             std::clamp (boundedDrive (dcRight.process (right * amplitude), 1.6f), -ceiling, ceiling) };
}

void DustGrainsEngine::process (float* left, float* right, int numSamples) noexcept
{
    if (left == nullptr || right == nullptr || numSamples <= 0)
        return;

    for (int i = 0; i < numSamples; ++i)
    {
        const auto frame = processSample();
        left[i] = frame.left;
        right[i] = frame.right;
    }
}

int DustGrainsEngine::getActiveGrainCount() const noexcept
{
    return static_cast<int> (std::count_if (grains.begin(), grains.end(), [] (const auto& grain)
    {
        return grain.active;
    }));
}

void DustGrainsEngine::rebuildSource() noexcept
{
    float low = 0.0f;
    float previous = 0.0f;
    for (auto& sample : source)
    {
        const auto white = noise.nextFloat();
        low = 0.985f * low + 0.015f * white;
        const auto velvet = std::fabs (white) > 0.985f ? (white > 0.0f ? 1.0f : -1.0f) : 0.0f;
        sample = std::clamp ((white - previous) * 0.32f + low * 0.7f + velvet * 0.45f, -1.0f, 1.0f);
        previous = white;
    }
}

void DustGrainsEngine::spawnGrain() noexcept
{
    auto* selected = &grains[0];
    for (auto& grain : grains)
    {
        if (! grain.active)
        {
            selected = &grain;
            break;
        }
        if (grain.age > selected->age)
            selected = &grain;
    }

    const auto randomPosition = 0.5f * (noise.nextFloat() + 1.0f);
    const auto noteOffset = static_cast<float> ((currentNote + 1) * 97 & sourceMask);
    const auto jitterSamples = parameters.positionJitter * static_cast<float> (sourceSize - 1);
    selected->position = std::fmod (noteOffset + randomPosition * jitterSamples, static_cast<float> (sourceSize));

    const auto scatter = noise.nextFloat() * parameters.rateScatter;
    selected->increment = std::clamp (parameters.rate * std::exp2 (scatter * 2.0f), 0.125f, 8.0f);
    selected->pan = noise.nextFloat();
    selected->age = 0;
    selected->length = std::max (8, static_cast<int> (parameters.durationSeconds * static_cast<float> (sampleRate)));
    selected->active = true;
    ++triggeredGrains;
}

float DustGrainsEngine::readSource (float position) const noexcept
{
    const auto base = static_cast<int> (position) & sourceMask;
    const auto next = (base + 1) & sourceMask;
    const auto fraction = position - std::floor (position);
    return source[static_cast<std::size_t> (base)]
         + fraction * (source[static_cast<std::size_t> (next)] - source[static_cast<std::size_t> (base)]);
}

} // namespace violent
