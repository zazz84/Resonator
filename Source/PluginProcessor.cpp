/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
TwoPoleBandPass::TwoPoleBandPass()
{
}

void TwoPoleBandPass::init(int sampleRate)
{
	m_sampleRate = sampleRate;
}

void TwoPoleBandPass::setCoef(float frequency, float resonance)
{
	if (m_sampleRate == 0)
	{
		return;
	}

	const float omega = frequency * (2.0f * 3.141593f / m_sampleRate);
	const float sn = sinf(omega);
	const float alpha = sn * (1.0f - resonance);

	m_a0 = 1.0f + alpha;
	m_a1 = cosf(omega) * -2.0f;
	m_a2 = 1.0f - alpha;

	m_b0 = sn * 0.5f;
	m_b1 = 0.0f;
	m_b2 = -1.0f * m_b0;

	m_a1 /= m_a0;
	m_a2 /= m_a0;

	m_b0 /= m_a0;
	m_b1 /= m_a0;
	m_b2 /= m_a0;
}

float TwoPoleBandPass::process(float in)
{
	float y = m_b0 * in + m_b1 * m_x1 + m_b2 * m_x2 - m_a1 * m_y1 - m_a2 * m_y2;

	m_y2 = m_y1;
	m_y1 = y;
	m_x2 = m_x1;
	m_x1 = in;

	return y;
}

//==============================================================================
EnvelopeFollower::EnvelopeFollower()
{
}

void EnvelopeFollower::setCoef(float attackTimeMs, float releaseTimeMs)
{
	m_AttackCoef = exp(-1000.0f / (attackTimeMs * m_SampleRate));
	m_ReleaseCoef = exp(-1000.0f / (releaseTimeMs * m_SampleRate));

	m_One_Minus_AttackCoef = 1.0f - m_AttackCoef;
	m_One_Minus_ReleaseCoef = 1.0f - m_ReleaseCoef;
}

float EnvelopeFollower::process(float in)
{
	const float inAbs = fabs(in);
	m_Out1Last = fmaxf(inAbs, m_ReleaseCoef * m_Out1Last + m_One_Minus_ReleaseCoef * inAbs);
	return m_OutLast = m_AttackCoef * m_OutLast + m_One_Minus_AttackCoef * m_Out1Last;
}

//==============================================================================
CrestFactor::CrestFactor()
{
}

float CrestFactor::process(float in)
{
	const float inSQ = in * in;
	const float inFactor = (1.0f - m_Coef) * inSQ;

	m_PeakLastSQ = std::max(inSQ, m_Coef * m_PeakLastSQ + inFactor);
	m_RMSLastSQ = m_Coef * m_RMSLastSQ + inFactor;

	return std::sqrtf(m_PeakLastSQ / m_RMSLastSQ);
}
//==============================================================================

const std::string ResonatorAudioProcessor::paramsNames[] = { "Frequency", "Resonance", "Attack", "Mix", "Volume" };

//==============================================================================
ResonatorAudioProcessor::ResonatorAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       )
#endif
{
	frequencyParameter = apvts.getRawParameterValue(paramsNames[0]);
	resonanceParameter = apvts.getRawParameterValue(paramsNames[1]);
	attackParameter    = apvts.getRawParameterValue(paramsNames[2]);
	mixParameter       = apvts.getRawParameterValue(paramsNames[3]);
	volumeParameter    = apvts.getRawParameterValue(paramsNames[4]);
}

ResonatorAudioProcessor::~ResonatorAudioProcessor()
{
}

//==============================================================================
const juce::String ResonatorAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool ResonatorAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool ResonatorAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool ResonatorAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double ResonatorAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int ResonatorAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int ResonatorAudioProcessor::getCurrentProgram()
{
    return 0;
}

void ResonatorAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String ResonatorAudioProcessor::getProgramName (int index)
{
    return {};
}

void ResonatorAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void ResonatorAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
	m_twoPoleBandPass[0].init((int)sampleRate);
	m_twoPoleBandPass[1].init((int)sampleRate);

	m_envelopeFollower[0].init((int)(sampleRate));
	m_envelopeFollower[1].init((int)(sampleRate));

	m_envelopeFollower[0].setCoef(0.01f, 40.0f);
	m_envelopeFollower[1].setCoef(0.01f, 40.0f);

	m_crestFactorCalculator[0].init((int)(sampleRate));
	m_crestFactorCalculator[1].init((int)(sampleRate));

	m_crestFactorCalculator[0].setCoef(0.1f);
	m_crestFactorCalculator[1].setCoef(0.1f);
}

void ResonatorAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool ResonatorAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

void ResonatorAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
	// Get params
	const float frequency = frequencyParameter->load();
	const float resonance = resonanceParameter->load();
	const auto thresholdNormalized = 1.0f - attackParameter->load();
	const float mix = mixParameter->load();
	const float volume = juce::Decibels::decibelsToGain(volumeParameter->load());

	// Mics constants
	const float mixInverse = 1.0f - mix;
	const float resonanceScaled = 0.997f + 0.002f * resonance;
	const int channels = getTotalNumOutputChannels();
	const int samples = buffer.getNumSamples();
	
	// Crest filter parameters
	const float CREST_LIMIT = 50.0f;
	const float ATTENUATION_FACTOR = -96.0f;
	const float ATTENUATION_LIMIT = 18.0f;

	for (int channel = 0; channel < channels; ++channel)
	{
		auto* channelBuffer = buffer.getWritePointer(channel);

		auto& twoPoleBandPass = m_twoPoleBandPass[channel];
		twoPoleBandPass.setCoef(frequency, resonance);

		auto& envelopeFollower = m_envelopeFollower[channel];
		auto& crestFactorCalculator = m_crestFactorCalculator[channel];

		for (int sample = 0; sample < samples; ++sample)
		{
			// Get input
			const float in = channelBuffer[sample];

			// Prefilter
			const float inBandPassFilter = twoPoleBandPass.process(in);

			// Get crest factor
			const float crestFactor = crestFactorCalculator.process(inBandPassFilter);
			const float crestFactorNormalized = std::min(crestFactor / CREST_LIMIT, 1.0f);
			const float crestSkewed = powf(crestFactorNormalized, 0.5f);

			//Get gain reduction, positive values
			float attenuatedB = (crestSkewed >= thresholdNormalized) ? (crestSkewed - thresholdNormalized) * ATTENUATION_FACTOR : 0.0f;
			attenuatedB = fminf(fabsf(attenuatedB), ATTENUATION_LIMIT);

			// Smooth
			const float smoothdB = envelopeFollower.process(attenuatedB);

			// Apply gain reduction
			const float out = inBandPassFilter * juce::Decibels::decibelsToGain(smoothdB);

			// Apply volume, mix and send to output
			channelBuffer[sample] = volume * (mix * out + mixInverse * in);
		}
	}
}

//==============================================================================
bool ResonatorAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* ResonatorAudioProcessor::createEditor()
{
    return new ResonatorAudioProcessorEditor(*this, apvts);
}

//==============================================================================
void ResonatorAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
	auto state = apvts.copyState();
	std::unique_ptr<juce::XmlElement> xml(state.createXml());
	copyXmlToBinary(*xml, destData);
}

void ResonatorAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
	std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));

	if (xmlState.get() != nullptr)
		if (xmlState->hasTagName(apvts.state.getType()))
			apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
}

juce::AudioProcessorValueTreeState::ParameterLayout ResonatorAudioProcessor::createParameterLayout()
{
	APVTS::ParameterLayout layout;

	using namespace juce;

	layout.add(std::make_unique<juce::AudioParameterFloat>(paramsNames[0], paramsNames[0], NormalisableRange<float>( 40.0f, 200.0f,  1.0f, 1.0f), 100.0f));
	layout.add(std::make_unique<juce::AudioParameterFloat>(paramsNames[1], paramsNames[1], NormalisableRange<float>(  0.0f,   1.0f, 0.01f, 1.0f),   0.5f));
	layout.add(std::make_unique<juce::AudioParameterFloat>(paramsNames[2], paramsNames[2], NormalisableRange<float>(  0.0f,   1.0f, 0.01f, 1.0f),   0.5f));
	layout.add(std::make_unique<juce::AudioParameterFloat>(paramsNames[3], paramsNames[3], NormalisableRange<float>(  0.0f,   1.0f, 0.01f, 1.0f),   1.0f));
	layout.add(std::make_unique<juce::AudioParameterFloat>(paramsNames[4], paramsNames[4], NormalisableRange<float>(-24.0f,  24.0f,  0.1f, 1.0f),   0.0f));

	return layout;
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ResonatorAudioProcessor();
}