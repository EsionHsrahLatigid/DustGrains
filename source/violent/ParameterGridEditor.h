#pragma once

#include <yup_audio_processors/yup_audio_processors.h>
#include <yup_gui/yup_gui.h>

#include <cstdint>
#include <memory>
#include <vector>

namespace violent::plugin
{

class ParameterGridEditor final
    : public yup::AudioProcessorEditor
    , private yup::Timer
{
public:
    ParameterGridEditor (yup::AudioProcessor& processor,
                         yup::StringRef title,
                         yup::StringRef warning,
                         std::uint32_t accentColor);
    ~ParameterGridEditor() override;

    bool isResizable() const override;
    bool shouldPreserveAspectRatio() const override;
    yup::Size<int> getPreferredSize() const override;
    void paint (yup::Graphics& graphics) override;
    void resized() override;
    void keyDown (const yup::KeyPress& key, const yup::Point<float>& position) override;
    void keyUp (const yup::KeyPress& key, const yup::Point<float>& position) override;
    void focusLost() override;

private:
    class OutputMeter;
    class EditorSlider;
    class TriggerButton;

    void timerCallback() override;
    void setMouseGateHeld (bool shouldBeHeld);
    void setSpaceGateHeld (bool shouldBeHeld);
    void publishTriggerGate();
    void updateTriggerButtonText();

    yup::String title;
    yup::String warning;
    std::uint32_t accentColor = 0xffe4cc33u;
    yup::AudioProcessor* processor = nullptr;
    bool mouseGateHeld = false;
    bool spaceGateActive = false;
    float displayedOutputPeak = 0.0f;
    std::unique_ptr<yup::Label> titleLabel;
    std::unique_ptr<yup::Label> warningLabel;
    std::unique_ptr<TriggerButton> triggerButton;
    std::unique_ptr<yup::Label> meterLabel;
    std::unique_ptr<OutputMeter> outputMeter;
    std::vector<yup::AudioParameter::Ptr> parameters;
    std::vector<std::unique_ptr<yup::Label>> labels;
    std::vector<std::unique_ptr<yup::Slider>> sliders;
    std::vector<std::unique_ptr<yup::Label>> valueLabels;
};

} // namespace violent::plugin
