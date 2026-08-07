#include "violent/DustGrainsEngine.h"

#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
#include <vector>

using violent::DustGrainsEngine;
using violent::DustGrainsParameters;

namespace
{

std::vector<float> render (std::uint32_t seed, DustGrainsParameters parameters, int samples)
{
    DustGrainsEngine engine;
    engine.prepare (48000.0);
    engine.setParameters (parameters);
    engine.reset (seed);
    engine.noteOn (61, 0.8f);

    std::vector<float> result;
    result.reserve (static_cast<std::size_t> (samples));
    for (int i = 0; i < samples; ++i)
        result.push_back (engine.processSample().left);
    return result;
}

void testSilentBeforeTrigger()
{
    DustGrainsEngine engine;
    engine.prepare (48000.0);
    for (int i = 0; i < 512; ++i)
    {
        const auto frame = engine.processSample();
        assert (frame.left == 0.0f);
        assert (frame.right == 0.0f);
    }
}

void testDeterministicForSameSeed()
{
    DustGrainsParameters parameters;
    parameters.densityHz = 240.0f;
    assert (render (1234u, parameters, 8192) == render (1234u, parameters, 8192));
}

void testDensityControlsTriggerCount()
{
    DustGrainsEngine sparse;
    DustGrainsEngine dense;
    sparse.prepare (48000.0);
    dense.prepare (48000.0);

    DustGrainsParameters sparseParameters;
    sparseParameters.densityHz = 4.0f;
    DustGrainsParameters denseParameters = sparseParameters;
    denseParameters.densityHz = 700.0f;
    sparse.setParameters (sparseParameters);
    dense.setParameters (denseParameters);
    sparse.reset (99u);
    dense.reset (99u);
    sparse.noteOn (40, 1.0f);
    dense.noteOn (40, 1.0f);

    for (int i = 0; i < 48000; ++i)
    {
        (void) sparse.processSample();
        (void) dense.processSample();
        assert (sparse.getActiveGrainCount() <= DustGrainsEngine::maximumGrains);
        assert (dense.getActiveGrainCount() <= DustGrainsEngine::maximumGrains);
    }

    assert (dense.getTriggeredGrainCount() > sparse.getTriggeredGrainCount() * 20u);
}

void testReleaseEventuallySilences()
{
    DustGrainsEngine engine;
    engine.prepare (48000.0);
    DustGrainsParameters parameters;
    parameters.densityHz = 300.0f;
    parameters.durationSeconds = 0.01f;
    engine.setParameters (parameters);
    engine.noteOn (72, 1.0f);
    for (int i = 0; i < 4096; ++i)
        (void) engine.processSample();
    engine.noteOff (72);
    for (int i = 0; i < 240000; ++i)
        (void) engine.processSample();

    assert (engine.getActiveGrainCount() == 0);
    const auto frame = engine.processSample();
    assert (std::fabs (frame.left) < 1.0e-5f);
    assert (std::fabs (frame.right) < 1.0e-5f);
}

void testExtremeAndNonFiniteParametersStayBounded()
{
    DustGrainsEngine engine;
    engine.prepare (std::numeric_limits<double>::infinity());
    DustGrainsParameters parameters;
    parameters.densityHz = std::numeric_limits<float>::infinity();
    parameters.durationSeconds = -100.0f;
    parameters.positionJitter = std::numeric_limits<float>::quiet_NaN();
    parameters.rate = std::numeric_limits<float>::infinity();
    parameters.rateScatter = -10.0f;
    parameters.stereoSpread = 10.0f;
    parameters.outputGain = 100.0f;
    engine.setParameters (parameters);
    engine.reset (0u);
    engine.noteOn (127, 1.0f);

    for (int i = 0; i < 100000; ++i)
    {
        const auto frame = engine.processSample();
        assert (std::isfinite (frame.left));
        assert (std::isfinite (frame.right));
        assert (frame.left >= -0.9501f && frame.left <= 0.9501f);
        assert (frame.right >= -0.9501f && frame.right <= 0.9501f);
        assert (engine.getActiveGrainCount() <= DustGrainsEngine::maximumGrains);
    }
}

void testVelocityZeroDoesNotScheduleGrains()
{
    DustGrainsEngine engine;
    engine.prepare (48000.0);
    engine.reset (123u);
    engine.noteOn (61, 0.0f);
    for (int i = 0; i < 48000; ++i)
        (void) engine.processSample();
    assert (engine.getActiveGrainCount() == 0);
    assert (engine.getTriggeredGrainCount() == 0u);
}

void testBufferProcessMatchesSampleProcess()
{
    DustGrainsEngine block;
    DustGrainsEngine sample;
    block.prepare (48000.0);
    sample.prepare (48000.0);
    block.reset (99u);
    sample.reset (99u);
    block.noteOn (44, 0.8f);
    sample.noteOn (44, 0.8f);

    std::vector<float> left (2048);
    std::vector<float> right (2048);
    block.process (left.data(), right.data(), static_cast<int> (left.size()));
    for (std::size_t i = 0; i < left.size(); ++i)
    {
        const auto frame = sample.processSample();
        assert (left[i] == frame.left);
        assert (right[i] == frame.right);
    }
}

} // namespace

int main()
{
    testSilentBeforeTrigger();
    testDeterministicForSameSeed();
    testDensityControlsTriggerCount();
    testReleaseEventuallySilences();
    testExtremeAndNonFiniteParametersStayBounded();
    testVelocityZeroDoesNotScheduleGrains();
    testBufferProcessMatchesSampleProcess();
    std::cout << "DustGrainsEngineTests passed\n";
    return 0;
}
