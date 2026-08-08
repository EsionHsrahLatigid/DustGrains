#include "violent/plugins/DustGrainsPlugin.h"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cmath>
#include <iostream>

namespace
{

float peakAbs (const yup::AudioBuffer<float>& buffer)
{
    float peak = 0.0f;
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        const auto* samples = buffer.getReadPointer (channel);
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            peak = std::max (peak, std::fabs (samples[sample]));
    }
    return peak;
}

float processBlock (violent::plugin::DustGrainsPlugin& plugin, yup::MidiBuffer& midi, int samples = 256)
{
    yup::AudioBuffer<float> block (2, samples);
    yup::ParameterChangeBuffer parameters;
    parameters.reserve (16);
    yup::AudioProcessContext<float> context { block, midi, parameters, nullptr, {}, {} };
    plugin.processBlock (context);
    return peakAbs (block);
}

void preparePlugin (violent::plugin::DustGrainsPlugin& plugin)
{
    plugin.prepareToPlay (yup::AudioSpec (48000.0f, 256, 2));
}

void testStandaloneTriggerProducesWaveform()
{
    violent::plugin::DustGrainsPlugin plugin;
    preparePlugin (plugin);
    plugin.setStandaloneTriggerGate (true);

    yup::AudioBuffer<float> audio (2, 4096);
    yup::MidiBuffer midi;
    yup::ParameterChangeBuffer parameters;
    parameters.reserve (16);

    for (int offset = 0; offset < audio.getNumSamples(); offset += 256)
    {
        yup::AudioBuffer<float> block (2, 256);
        yup::AudioProcessContext<float> context { block, midi, parameters, nullptr, {}, {} };
        plugin.processBlock (context);
        for (int channel = 0; channel < block.getNumChannels(); ++channel)
            std::copy (block.getReadPointer (channel),
                       block.getReadPointer (channel) + block.getNumSamples(),
                       audio.getWritePointer (channel) + offset);
    }

    const auto renderedPeak = peakAbs (audio);
    assert (renderedPeak > 1.0e-5f);
    assert (plugin.getOutputPeak() > 1.0e-5f);

    plugin.setStandaloneTriggerGate (false);
    yup::AudioBuffer<float> releaseBlock (2, 256);
    yup::AudioProcessContext<float> releaseContext { releaseBlock, midi, parameters, nullptr, {}, {} };
    plugin.processBlock (releaseContext);
    assert (! plugin.getStandaloneTriggerGate());
}

void testRapidPulseBeforeProcessStillTriggers()
{
    violent::plugin::DustGrainsPlugin plugin;
    preparePlugin (plugin);
    yup::MidiBuffer midi;

    plugin.setStandaloneTriggerGate (true);
    plugin.setStandaloneTriggerGate (false);

    const auto peak = processBlock (plugin, midi, 512);
    assert (peak > 1.0e-5f);
    assert (plugin.getOutputPeak() > 1.0e-5f);
    assert (plugin.getTriggeredGrainCountForTesting() > 0u);
    assert (! plugin.getStandaloneTriggerGate());
}

void testMidiOverlapReturnsToHeldStandaloneGate()
{
    violent::plugin::DustGrainsPlugin plugin;
    preparePlugin (plugin);
    yup::MidiBuffer midi;

    plugin.setStandaloneTriggerGate (true);
    (void) processBlock (plugin, midi);
    const auto standaloneTriggerCount = plugin.getTriggeredGrainCountForTesting();
    assert (standaloneTriggerCount > 0u);

    midi.addEvent (yup::MidiMessage::noteOn (1, 72, 1.0f), 0);
    (void) processBlock (plugin, midi);
    const auto midiTriggerCount = plugin.getTriggeredGrainCountForTesting();
    assert (midiTriggerCount > standaloneTriggerCount);

    midi.addEvent (yup::MidiMessage::noteOff (1, 72), 0);
    (void) processBlock (plugin, midi);
    assert (plugin.getStandaloneTriggerGate());
    assert (plugin.getTriggeredGrainCountForTesting() > midiTriggerCount);
}

void testResetDoesNotSuppressHeldGateAndReleaseStillWorks()
{
    violent::plugin::DustGrainsPlugin plugin;
    preparePlugin (plugin);
    yup::MidiBuffer midi;

    plugin.setStandaloneTriggerGate (true);
    (void) processBlock (plugin, midi);
    assert (plugin.getTriggeredGrainCountForTesting() > 0u);

    plugin.flush();
    assert (plugin.getStandaloneTriggerGate());
    const auto postFlushPeak = processBlock (plugin, midi, 512);
    assert (postFlushPeak > 1.0e-5f);
    assert (plugin.getTriggeredGrainCountForTesting() > 0u);

    plugin.setStandaloneTriggerGate (false);
    (void) processBlock (plugin, midi);
    assert (! plugin.getStandaloneTriggerGate());
}

} // namespace

int main()
{
    testStandaloneTriggerProducesWaveform();
    testRapidPulseBeforeProcessStillTriggers();
    testMidiOverlapReturnsToHeldStandaloneGate();
    testResetDoesNotSuppressHeldGateAndReleaseStillWorks();
    std::cout << "DustGrainsPluginBridgeTests passed\n";
    return 0;
}
