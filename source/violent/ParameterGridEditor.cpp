#include "violent/ParameterGridEditor.h"

#include "violent/plugins/DustGrainsPlugin.h"

#include <algorithm>
#include <cmath>
#include <functional>

namespace violent::plugin
{
class ParameterGridEditor::EditorSlider final : public yup::Slider
{
public:
    EditorSlider (yup::Slider::SliderType sliderType, ParameterGridEditor& ownerEditor)
        : yup::Slider (sliderType)
        , owner (ownerEditor)
    {
    }

    void mouseDown (const yup::MouseEvent& event) override
    {
        yup::Slider::mouseDown (event);
        owner.takeKeyboardFocus();
    }

    void mouseUp (const yup::MouseEvent& event) override
    {
        yup::Slider::mouseUp (event);
        owner.takeKeyboardFocus();
    }

private:
    ParameterGridEditor& owner;
};

class ParameterGridEditor::TriggerButton final : public yup::TextButton
{
public:
    using yup::TextButton::TextButton;

    std::function<void (bool)> onMouseGateChanged;
    std::function<void (bool)> onSpaceGateChanged;

    void mouseDown (const yup::MouseEvent& event) override
    {
        yup::TextButton::mouseDown (event);
        setClickingGrabFocus (false);
        if (onMouseGateChanged)
            onMouseGateChanged (true);
    }

    void mouseUp (const yup::MouseEvent& event) override
    {
        yup::TextButton::mouseUp (event);
        if (onMouseGateChanged)
            onMouseGateChanged (false);
    }

    void keyDown (const yup::KeyPress& key, const yup::Point<float>& position) override
    {
        if (key.getKey() == yup::KeyPress::spaceKey)
        {
            if (! keyGateActive && onSpaceGateChanged)
            {
                keyGateActive = true;
                onSpaceGateChanged (true);
            }
            return;
        }

        yup::TextButton::keyDown (key, position);
    }

    void keyUp (const yup::KeyPress& key, const yup::Point<float>& position) override
    {
        if (key.getKey() == yup::KeyPress::spaceKey)
        {
            if (keyGateActive && onSpaceGateChanged)
            {
                keyGateActive = false;
                onSpaceGateChanged (false);
            }
            return;
        }

        yup::TextButton::keyUp (key, position);
    }

    void focusLost() override
    {
        keyGateActive = false;
        if (onSpaceGateChanged)
            onSpaceGateChanged (false);
        yup::TextButton::focusLost();
    }

private:
    bool keyGateActive = false;
};

class ParameterGridEditor::OutputMeter final : public yup::Component
{
public:
    void setLevel (float newLevel) noexcept
    {
        level = std::clamp (newLevel, 0.0f, 1.0f);
        repaint();
    }

    void paint (yup::Graphics& graphics) override
    {
        const auto bounds = getLocalBounds();
        graphics.setFillColor (0xff15191du);
        graphics.fillRoundedRect (bounds, 4.0f);

        graphics.setFillColor (0xff2c343cu);
        graphics.fillRect (bounds.getX() + 2.0f, bounds.getY() + 2.0f, bounds.getWidth() - 4.0f, bounds.getHeight() - 4.0f);

        graphics.setFillColor (0xffe4cc33u);
        graphics.fillRect (bounds.getX() + 2.0f,
                           bounds.getY() + 2.0f,
                           (bounds.getWidth() - 4.0f) * level,
                           bounds.getHeight() - 4.0f);
    }

private:
    float level = 0.0f;
};

ParameterGridEditor::ParameterGridEditor (yup::AudioProcessor& processor,
                                          yup::StringRef newTitle,
                                          yup::StringRef newWarning,
                                          std::uint32_t newAccentColor)
    : title (newTitle)
    , warning (newWarning)
    , accentColor (newAccentColor)
    , processor (&processor)
{
    const auto processorParameters = processor.getParameters();
    parameters.assign (processorParameters.begin(), processorParameters.end());

    setWantsKeyboardFocus (true);

    titleLabel = std::make_unique<yup::Label>();
    titleLabel->setText (title, yup::dontSendNotification);
    titleLabel->setJustification (yup::Justification::centerLeft);
    addAndMakeVisible (*titleLabel);

    warningLabel = std::make_unique<yup::Label>();
    warningLabel->setText (warning, yup::dontSendNotification);
    warningLabel->setJustification (yup::Justification::centerLeft);
    addAndMakeVisible (*warningLabel);

    if (dynamic_cast<DustGrainsPlugin*> (&processor) != nullptr)
    {
        triggerButton = std::make_unique<TriggerButton>();
        triggerButton->setButtonText ("Trigger");
        triggerButton->setClickingGrabFocus (false);
        triggerButton->onMouseGateChanged = [this] (bool shouldBeHeld) { setMouseGateHeld (shouldBeHeld); };
        triggerButton->onSpaceGateChanged = [this] (bool shouldBeHeld) { setSpaceGateHeld (shouldBeHeld); };
        addAndMakeVisible (*triggerButton);

        meterLabel = std::make_unique<yup::Label>();
        meterLabel->setText ("Output", yup::dontSendNotification);
        meterLabel->setJustification (yup::Justification::centerRight);
        addAndMakeVisible (*meterLabel);

        outputMeter = std::make_unique<OutputMeter>();
        addAndMakeVisible (*outputMeter);
    }

    labels.reserve (parameters.size());
    sliders.reserve (parameters.size());
    valueLabels.reserve (parameters.size());

    for (const auto& parameter : parameters)
    {
        auto label = std::make_unique<yup::Label>();
        label->setText (parameter->getName(), yup::dontSendNotification);
        label->setJustification (yup::Justification::center);
        addAndMakeVisible (*label);
        labels.push_back (std::move (label));

        auto slider = std::make_unique<EditorSlider> (yup::Slider::RotaryVerticalDrag, *this);
        slider->setClickingGrabFocus (false);
        slider->setRange (parameter->getMinimumValue(),
                          parameter->getMaximumValue(),
                          parameter->isStepped() ? 1.0 : 0.0);
        slider->setDefaultValue (parameter->getDefaultValue());
        slider->setValue (parameter->getValue(), yup::dontSendNotification);
        slider->setTextBoxStyle (yup::Slider::NoTextBox);
        slider->setPopupDisplayEnabled (false);
        slider->setMouseCursor (yup::MouseCursor::Hand);
        slider->onDragStart = [parameter] (const yup::MouseEvent&) { parameter->beginChangeGesture(); };
        slider->onValueChanged = [parameter] (double value)
        {
            parameter->setValueNotifyingHost (static_cast<float> (value));
        };
        slider->onDragEnd = [parameter] (const yup::MouseEvent&) { parameter->endChangeGesture(); };
        addAndMakeVisible (*slider);
        sliders.push_back (std::move (slider));

        auto valueLabel = std::make_unique<yup::Label>();
        valueLabel->setText (parameter->toString(), yup::dontSendNotification);
        valueLabel->setJustification (yup::Justification::center);
        addAndMakeVisible (*valueLabel);
        valueLabels.push_back (std::move (valueLabel));
    }

    setSize (getPreferredSize().to<float>());
    startTimerHz (30);
}

ParameterGridEditor::~ParameterGridEditor()
{
    mouseGateHeld = false;
    spaceGateActive = false;
    publishTriggerGate();
}

bool ParameterGridEditor::isResizable() const
{
    return true;
}

bool ParameterGridEditor::shouldPreserveAspectRatio() const
{
    return true;
}

yup::Size<int> ParameterGridEditor::getPreferredSize() const
{
    return { 940, 520 };
}

void ParameterGridEditor::paint (yup::Graphics& graphics)
{
    graphics.setFillColor (0xff0a0b0du);
    graphics.fillAll();

    graphics.setFillColor (accentColor);
    graphics.fillRect (0.0f, 0.0f, getWidth(), 5.0f);
}

void ParameterGridEditor::resized()
{
    constexpr int columns = 5;
    constexpr float margin = 20.0f;
    constexpr float top = 118.0f;
    constexpr float gap = 12.0f;
    constexpr float labelHeight = 24.0f;
    constexpr float valueHeight = 24.0f;
    constexpr float controlGap = 4.0f;

    const auto bounds = getLocalBounds();
    const auto cellWidth = (bounds.getWidth() - 2.0f * margin - gap * (columns - 1)) / columns;
    const auto rows = std::max (1, static_cast<int> ((sliders.size() + columns - 1) / columns));
    const auto availableHeight = bounds.getHeight() - top - margin;
    const auto cellHeight = (availableHeight - gap * (rows - 1)) / rows;

    titleLabel->setBounds (24.0f, 12.0f, bounds.getWidth() - 48.0f, 30.0f);
    warningLabel->setBounds (24.0f, 43.0f, bounds.getWidth() - 48.0f, 24.0f);
    if (triggerButton != nullptr && meterLabel != nullptr && outputMeter != nullptr)
    {
        triggerButton->setBounds (24.0f, 76.0f, 128.0f, 30.0f);
        meterLabel->setBounds (bounds.getWidth() - 260.0f, 79.0f, 70.0f, 24.0f);
        outputMeter->setBounds (bounds.getWidth() - 180.0f, 82.0f, 156.0f, 18.0f);
    }

    for (std::size_t i = 0; i < sliders.size(); ++i)
    {
        const auto column = static_cast<int> (i) % columns;
        const auto row = static_cast<int> (i) / columns;
        const auto x = margin + column * (cellWidth + gap);
        const auto y = top + row * (cellHeight + gap);
        const auto controlHeight = cellHeight - labelHeight - valueHeight - 2.0f * controlGap;
        const auto controlSize = std::max (20.0f, std::min (cellWidth - 8.0f, controlHeight));
        const auto controlX = x + 0.5f * (cellWidth - controlSize);
        const auto controlY = y + labelHeight + controlGap;

        labels[i]->setBounds (x, y, cellWidth, labelHeight);
        sliders[i]->setBounds (controlX, controlY, controlSize, controlSize);
        valueLabels[i]->setBounds (x, y + cellHeight - valueHeight, cellWidth, valueHeight);
    }
}

void ParameterGridEditor::keyDown (const yup::KeyPress& key, const yup::Point<float>& position)
{
    if (key.getKey() == yup::KeyPress::spaceKey && ! spaceGateActive)
    {
        setSpaceGateHeld (true);
        return;
    }

    yup::AudioProcessorEditor::keyDown (key, position);
}

void ParameterGridEditor::keyUp (const yup::KeyPress& key, const yup::Point<float>& position)
{
    if (key.getKey() == yup::KeyPress::spaceKey && spaceGateActive)
    {
        setSpaceGateHeld (false);
        return;
    }

    yup::AudioProcessorEditor::keyUp (key, position);
}

void ParameterGridEditor::focusLost()
{
    mouseGateHeld = false;
    spaceGateActive = false;
    publishTriggerGate();
    yup::AudioProcessorEditor::focusLost();
}

void ParameterGridEditor::timerCallback()
{
    for (std::size_t i = 0; i < sliders.size(); ++i)
    {
        if (! sliders[i]->isCurrentlyBeingDragged())
            sliders[i]->setValue (parameters[i]->getValue(), yup::dontSendNotification);
        valueLabels[i]->setText (parameters[i]->toString(), yup::dontSendNotification);
    }

    if (auto* controls = dynamic_cast<DustGrainsPlugin*> (processor))
    {
        const auto targetPeak = std::clamp (controls->getOutputPeak(), 0.0f, 1.0f);
        displayedOutputPeak = std::max (targetPeak, displayedOutputPeak * 0.84f);
        if (outputMeter != nullptr)
            outputMeter->setLevel (displayedOutputPeak);
        updateTriggerButtonText();
    }
}

void ParameterGridEditor::setMouseGateHeld (bool shouldBeHeld)
{
    if (mouseGateHeld == shouldBeHeld)
        return;

    mouseGateHeld = shouldBeHeld;
    publishTriggerGate();
}

void ParameterGridEditor::setSpaceGateHeld (bool shouldBeHeld)
{
    if (spaceGateActive == shouldBeHeld)
        return;

    spaceGateActive = shouldBeHeld;
    publishTriggerGate();
}

void ParameterGridEditor::publishTriggerGate()
{
    if (auto* controls = dynamic_cast<DustGrainsPlugin*> (processor))
        controls->setStandaloneTriggerGate (mouseGateHeld || spaceGateActive);
    updateTriggerButtonText();
}

void ParameterGridEditor::updateTriggerButtonText()
{
    if (triggerButton == nullptr)
        return;

    if (auto* controls = dynamic_cast<DustGrainsPlugin*> (processor))
        triggerButton->setButtonText (controls->getStandaloneTriggerGate() ? "Trigger On" : "Trigger");
}

} // namespace violent::plugin
