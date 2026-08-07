#include "violent/plugins/DustGrainsPlugin.h"

#include "violent/ParameterGridEditor.h"
#include "violent/ProductState.h"

#include <algorithm>
#include <array>

namespace violent::plugin
{
namespace
{
constexpr std::array<char, 4> stateMagic {{ 'D', 'G', 'R', '1' }};
constexpr int stateVersion = 1;
constexpr int controlPeriod = 16;

yup::NormalisableRange<float> makeDensityRange()
{
    auto range = yup::NormalisableRange<float> (0.1f, 1200.0f);
    range.setSkewForCentre (80.0f);
    return range;
}

yup::NormalisableRange<float> makeDurationRange()
{
    auto range = yup::NormalisableRange<float> (0.001f, 0.25f);
    range.setSkewForCentre (0.025f);
    return range;
}

yup::NormalisableRange<float> makeRateRange()
{
    auto range = yup::NormalisableRange<float> (0.125f, 8.0f);
    range.setSkewForCentre (1.0f);
    return range;
}

constexpr std::array<std::array<float, 7>, 4> presetValues {{
    {{ 80.0f, 0.018f, 0.75f, 1.0f, 0.60f, 0.80f, -7.0f }},
    {{ 420.0f, 0.004f, 1.00f, 2.8f, 0.92f, 1.00f, -11.0f }},
    {{ 22.0f, 0.090f, 0.38f, 0.55f, 0.28f, 0.45f, -5.0f }},
    {{ 760.0f, 0.012f, 0.86f, 4.5f, 0.78f, 0.92f, -14.0f }}
}};
} // namespace

DustGrainsPlugin::DustGrainsPlugin()
    : yup::AudioProcessor ("DustGrains",
                           yup::AudioBusLayout ({ yup::AudioBus ("midi", yup::AudioBus::Midi, yup::AudioBus::Input, 1) },
                                                { yup::AudioBus ("main", yup::AudioBus::Audio, yup::AudioBus::Output, 2) }))
{
    parameters[density] = yup::AudioParameterBuilder()
                              .withID ("density")
                              .withName ("Density")
                              .withHostID (density)
                              .withRange (makeDensityRange())
                              .withDefault (presetValues[0][density])
                              .withSmoothing (20.0f)
                              .withModulatable (true)
                              .withUnit (yup::AudioParameter::ParameterUnit::Hertz)
                              .build();
    parameters[duration] = yup::AudioParameterBuilder()
                               .withID ("duration")
                               .withName ("Duration")
                               .withHostID (duration)
                               .withRange (makeDurationRange())
                               .withDefault (presetValues[0][duration])
                               .withSmoothing (20.0f)
                               .withModulatable (true)
                               .withUnit (yup::AudioParameter::ParameterUnit::Seconds)
                               .build();
    parameters[positionJitter] = yup::AudioParameterBuilder()
                                     .withID ("position_jitter")
                                     .withName ("Position Jitter")
                                     .withHostID (positionJitter)
                                     .withRange (0.0f, 1.0f)
                                     .withDefault (presetValues[0][positionJitter])
                                     .withSmoothing (15.0f)
                                     .withModulatable (true)
                                     .build();
    parameters[rate] = yup::AudioParameterBuilder()
                           .withID ("rate")
                           .withName ("Rate")
                           .withHostID (rate)
                           .withRange (makeRateRange())
                           .withDefault (presetValues[0][rate])
                           .withSmoothing (20.0f)
                           .withModulatable (true)
                           .withUnit (yup::AudioParameter::ParameterUnit::Ratio)
                           .build();
    parameters[rateScatter] = yup::AudioParameterBuilder()
                                  .withID ("rate_scatter")
                                  .withName ("Rate Scatter")
                                  .withHostID (rateScatter)
                                  .withRange (0.0f, 1.0f)
                                  .withDefault (presetValues[0][rateScatter])
                                  .withSmoothing (15.0f)
                                  .withModulatable (true)
                                  .build();
    parameters[stereoSpread] = yup::AudioParameterBuilder()
                                   .withID ("stereo_spread")
                                   .withName ("Stereo Spread")
                                   .withHostID (stereoSpread)
                                   .withRange (0.0f, 1.0f)
                                   .withDefault (presetValues[0][stereoSpread])
                                   .withSmoothing (20.0f)
                                   .withModulatable (true)
                                   .build();
    parameters[output] = yup::AudioParameterBuilder()
                             .withID ("output")
                             .withName ("Output")
                             .withHostID (output)
                             .withRange (-48.0f, 6.0f)
                             .withDefault (presetValues[0][output])
                             .withSmoothing (30.0f)
                             .withModulatable (true)
                             .withUnit (yup::AudioParameter::ParameterUnit::Decibels)
                             .build();

    for (const auto& parameter : parameters)
        addParameter (parameter);
}

void DustGrainsPlugin::prepareToPlay (const yup::AudioSpec& spec)
{
    engine.prepare (spec.sampleRate);
    for (std::size_t i = 0; i < handles.size(); ++i)
    {
        handles[i] = yup::AudioParameterHandle (*parameters[i], spec.sampleRate);
        smoothedValues[i] = parameters[i]->getValue();
    }
    applyEngineParameters();
    activeNote = -1;
    controlCountdown = 0;
}

void DustGrainsPlugin::releaseResources()
{
}

void DustGrainsPlugin::processBlock (yup::AudioProcessContext<float>& context)
{
    auto& audio = context.audio;
    for (std::size_t i = 0; i < handles.size(); ++i)
        handles[i].prepareBlock (context.params, parameters[i]->getIndexInContainer());

    auto midi = context.midi.begin();
    const auto midiEnd = context.midi.end();
    const auto numSamples = audio.getNumSamples();
    const auto numChannels = audio.getNumChannels();
    auto* left = numChannels > 0 ? audio.getWritePointer (0) : nullptr;
    auto* right = numChannels > 1 ? audio.getWritePointer (1) : nullptr;

    for (int sample = 0; sample < numSamples; ++sample)
    {
        while (midi != midiEnd && (*midi).samplePosition <= sample)
        {
            const auto& message = (*midi).getMessage();
            if (message.isNoteOn())
            {
                activeNote = std::clamp (message.getNoteNumber(), 0, 127);
                engine.noteOn (activeNote, std::clamp (message.getFloatVelocity(), 0.0f, 1.0f));
            }
            else if (message.isNoteOff())
            {
                const auto note = std::clamp (message.getNoteNumber(), 0, 127);
                if (note == activeNote)
                {
                    engine.noteOff (note);
                    activeNote = -1;
                }
            }
            ++midi;
        }

        advanceParameters (sample);
        if (controlCountdown <= 0)
        {
            applyEngineParameters();
            controlCountdown = controlPeriod;
        }
        --controlCountdown;

        const auto frame = engine.processSample();
        if (left != nullptr)
            left[sample] = frame.left;
        if (right != nullptr)
            right[sample] = frame.right;
        for (int channel = 2; channel < numChannels; ++channel)
            audio.getWritePointer (channel)[sample] = 0.0f;
    }

    context.midi.clear();
}

void DustGrainsPlugin::flush()
{
    engine.reset (1u);
    activeNote = -1;
    controlCountdown = 0;
}

int DustGrainsPlugin::getNumVoices() const
{
    return 1;
}

int DustGrainsPlugin::getCurrentPreset() const noexcept
{
    return currentPreset.load (std::memory_order_relaxed);
}

void DustGrainsPlugin::setCurrentPreset (int index) noexcept
{
    if (! yup::isPositiveAndBelow (index, static_cast<int> (presetValues.size())))
        return;
    currentPreset.store (index, std::memory_order_relaxed);
    for (std::size_t i = 0; i < parameters.size(); ++i)
        parameters[i]->setValue (presetValues[static_cast<std::size_t> (index)][i]);
}

int DustGrainsPlugin::getNumPresets() const
{
    return static_cast<int> (presetNames.size());
}

yup::String DustGrainsPlugin::getPresetName (int index) const
{
    return yup::isPositiveAndBelow (index, static_cast<int> (presetNames.size()))
        ? presetNames[static_cast<std::size_t> (index)]
        : yup::String ("Invalid Preset");
}

void DustGrainsPlugin::setPresetName (int index, yup::StringRef newName)
{
    if (yup::isPositiveAndBelow (index, static_cast<int> (presetNames.size())))
        presetNames[static_cast<std::size_t> (index)] = newName;
}

yup::Result DustGrainsPlugin::loadStateFromMemory (const yup::MemoryBlock& data)
{
    auto preset = currentPreset.load (std::memory_order_relaxed);
    const auto result = loadProductState (*this, data, stateMagic, stateVersion, getNumPresets(), preset);
    if (result.wasOk())
        currentPreset.store (preset, std::memory_order_relaxed);
    return result;
}

yup::Result DustGrainsPlugin::saveStateIntoMemory (yup::MemoryBlock& data)
{
    return saveProductState (*this, data, stateMagic, stateVersion, currentPreset.load (std::memory_order_relaxed));
}

bool DustGrainsPlugin::hasEditor() const
{
    return true;
}

yup::AudioProcessorEditor* DustGrainsPlugin::createEditor()
{
    return new ParameterGridEditor (*this,
                                    "DustGrains",
                                    "Dense granular bursts can become loud. Start with monitoring low and raise deliberately.",
                                    0xffe4cc33u);
}

void DustGrainsPlugin::advanceParameters (int samplePosition) noexcept
{
    for (std::size_t i = 0; i < handles.size(); ++i)
    {
        handles[i].advanceToSample (samplePosition);
        smoothedValues[i] = handles[i].getNextValue();
    }
}

void DustGrainsPlugin::applyEngineParameters() noexcept
{
    violent::DustGrainsParameters values;
    values.densityHz = smoothedValues[density];
    values.durationSeconds = smoothedValues[duration];
    values.positionJitter = smoothedValues[positionJitter];
    values.rate = smoothedValues[rate];
    values.rateScatter = smoothedValues[rateScatter];
    values.stereoSpread = smoothedValues[stereoSpread];
    values.outputGain = violent::decibelsToGain (smoothedValues[output]);
    engine.setParameters (values);
}

} // namespace violent::plugin

extern "C" yup::AudioProcessor* createPluginProcessor()
{
    return new violent::plugin::DustGrainsPlugin();
}
