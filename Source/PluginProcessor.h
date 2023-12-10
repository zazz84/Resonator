/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

//==============================================================================
class TwoPoleBandPass
{
public:
	TwoPoleBandPass();

	void init(int sampleRate);
	void setCoef(float frequency, float resonance);
	float process(float in);

protected:
	int m_sampleRate;

	float m_a0 = 1.0f;
	float m_a1 = 0.0f;
	float m_a2 = 0.0f;
	float m_b0 = 0.0f;
	float m_b1 = 0.0f;
	float m_b2 = 1.0f;

	float m_x1 = 0.0f;
	float m_x2 = 0.0f;
	float m_y1 = 0.0f;
	float m_y2 = 0.0f;
};

//==============================================================================
class EnvelopeFollower
{
public:
	EnvelopeFollower();

	void init(int sampleRate) { m_SampleRate = sampleRate; }
	void setCoef(float attackTime, float releaseTime);
	float process(float in);

protected:
	int  m_SampleRate = 48000;
	float m_AttackCoef = 0.0f;
	float m_One_Minus_AttackCoef = 0.0f;
	float m_ReleaseCoef = 0.0f;
	float m_One_Minus_ReleaseCoef = 0.0f;

	float m_OutLast = 0.0f;
	float m_Out1Last = 0.0f;
};

//==============================================================================
class CrestFactor
{
public:
	CrestFactor();

	void init(int sampleRate) { m_SampleRate = sampleRate; }
	void setCoef(float time) { m_Coef = exp(-1.0f / (m_SampleRate * time)); }
	float process(float in);

protected:
	int  m_SampleRate = 48000;
	float m_Coef = 0.0f;

	float m_PeakLastSQ = 0.0f;
	float m_RMSLastSQ = 0.0f;
};

//==============================================================================
/**
*/
class ResonatorAudioProcessor  : public juce::AudioProcessor
                            #if JucePlugin_Enable_ARA
                             , public juce::AudioProcessorARAExtension
                            #endif
{
public:
    //==============================================================================
    ResonatorAudioProcessor();
    ~ResonatorAudioProcessor() override;

	static const std::string paramsNames[];

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

	using APVTS = juce::AudioProcessorValueTreeState;
	static APVTS::ParameterLayout createParameterLayout();

	APVTS apvts{ *this, nullptr, "Parameters", createParameterLayout() };

private:
    //==============================================================================

	std::atomic<float>* frequencyParameter = nullptr;
	std::atomic<float>* resonanceParameter = nullptr;
	std::atomic<float>* attackParameter = nullptr;
	std::atomic<float>* mixParameter = nullptr;
	std::atomic<float>* volumeParameter = nullptr;

	TwoPoleBandPass  m_twoPoleBandPass[2] = {};
	EnvelopeFollower m_envelopeFollower[2] = {};
	CrestFactor      m_crestFactorCalculator[2] = {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ResonatorAudioProcessor)
};