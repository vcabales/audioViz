#ifndef MAINCOMPONENT_H_INCLUDED
#define MAINCOMPONENT_H_INCLUDED

#include "../JuceLibraryCode/JuceHeader.h"

// example implementation of the Exercise 3

class SimpleThumbnailComponent : public Component,
private ChangeListener
{
public:
    SimpleThumbnailComponent (int sourceSamplesPerThumbnailSample,
                              AudioFormatManager& formatManager,
                              AudioThumbnailCache& cache)
    : thumbnail (sourceSamplesPerThumbnailSample, formatManager, cache)
    {
        thumbnail.addChangeListener (this);
    }
    
    void setFile (const File& file)
    {
        thumbnail.setSource (new FileInputSource (file));
    }
    
    void paint (Graphics& g) override
    {
        if (thumbnail.getNumChannels() == 0)
            paintIfNoFileLoaded (g);
        else
            paintIfFileLoaded (g);
    }
    
    void paintIfNoFileLoaded (Graphics& g)
    {
        g.fillAll (Colours::white);
        g.setColour (Colours::darkgrey);
        g.drawFittedText ("No File Loaded", getLocalBounds(), Justification::centred, 1.0f);
    }
    
    void paintIfFileLoaded (Graphics& g)
    {
        g.fillAll(Colours::white);
        
        g.setColour (Colours::red);
        thumbnail.drawChannels (g, getLocalBounds(), 0.0, thumbnail.getTotalLength(), 1.0f);
    }
    
    void changeListenerCallback (ChangeBroadcaster* source) override
    {
        if (source == &thumbnail)
            thumbnailChanged();
    }
    
private:
    void thumbnailChanged()
    {
        repaint();
    }
    
    AudioThumbnail thumbnail;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SimpleThumbnailComponent)
};

//------------------------------------------------------------------------------

class SimplePositionOverlay : public Component,
private Timer
{
public:
    SimplePositionOverlay (AudioTransportSource& transportSourceToUse)
    : transportSource (transportSourceToUse)
    {
        startTimer (40);
    }
    
    void paint (Graphics& g) override
    {
        const double duration = transportSource.getLengthInSeconds();
        
        if (duration > 0.0)
        {
            const double audioPosition = transportSource.getCurrentPosition();
            const float drawPosition = (audioPosition / duration) * getWidth();
            
            g.setColour (Colours::green);
            g.drawLine (drawPosition, 0.0f, drawPosition, (float) getHeight(), 2.0f);
        }
    }
    
    void mouseDown (const MouseEvent& event) override
    {
        const double duration = transportSource.getLengthInSeconds();
        
        if (duration > 0.0)
        {
            const double clickPosition = event.position.x;
            const double audioPosition = (clickPosition / getWidth()) * duration;
            
            transportSource.setPosition (audioPosition);
        }
    }
    
private:
    void timerCallback() override
    {
        repaint();
    }
    
    AudioTransportSource& transportSource;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SimplePositionOverlay)
};

//------------------------------------------------------------------------------

class MainContentComponent   : public AudioAppComponent,
public ChangeListener,
public ButtonListener,
public Slider::Listener

{
public:
    MainContentComponent()
    : state (Stopped),
    thumbnailCache (5),
    thumbnailComp (512, formatManager, thumbnailCache),
    positionOverlay (transportSource)
    {
        setLookAndFeel (&lookAndFeel);
        
        addAndMakeVisible (&openButton);
        openButton.setButtonText ("Open...");
        openButton.addListener (this);
        
        addAndMakeVisible (&playButton);
        playButton.setButtonText ("Play");
        playButton.addListener (this);
        playButton.setColour (TextButton::buttonColourId, Colours::green);
        playButton.setEnabled (false);
        
        addAndMakeVisible (&stopButton);
        stopButton.setButtonText ("Stop");
        stopButton.addListener (this);
        stopButton.setColour (TextButton::buttonColourId, Colours::red);
        stopButton.setEnabled (false);
        
        addAndMakeVisible (&thumbnailComp);
        addAndMakeVisible (&positionOverlay);
        
        addAndMakeVisible (levelSlider);
        levelSlider.setRange(0,100);
        levelSlider.setTextValueSuffix("vol");
        levelSlider.setValue(50);
        levelSlider.addListener (this);
        
        addAndMakeVisible (volumeLabel);
        volumeLabel.setText("Volume", dontSendNotification);
        volumeLabel.attachToComponent (&levelSlider, true);
        
        levelSlider.setTextBoxStyle (Slider::TextBoxLeft, false, 160, levelSlider.getTextBoxHeight());
        
        setSize (600, 400);
        
        formatManager.registerBasicFormats();
        transportSource.addChangeListener (this);
        
        setAudioChannels (2, 2);
    }
    
    ~MainContentComponent()
    {
        shutdownAudio();
    }
    
    void prepareToPlay (int samplesPerBlockExpected, double sampleRate) override
    {
        transportSource.prepareToPlay (samplesPerBlockExpected, sampleRate);
    }
    
    void getNextAudioBlock (const AudioSourceChannelInfo& bufferToFill) override
    {
        AudioIODevice* device = deviceManager.getCurrentAudioDevice();
        const BigInteger activeInputChannels = device->getActiveInputChannels();
        const BigInteger activeOutputChannels = device->getActiveOutputChannels();
        
        
        const int maxInputChannels = activeInputChannels.getHighestBit() + 1;
        const int maxOutputChannels = activeOutputChannels.getHighestBit() + 1;
        if (readerSource == nullptr)
            bufferToFill.clearActiveBufferRegion();
        else
        {
            transportSource.getNextAudioBlock (bufferToFill);
            const float level = (float)levelSlider.getValue();
            
            
            for (int channel = 0; channel < maxOutputChannels; ++channel)
            {
                if ((! activeOutputChannels[channel]) || maxInputChannels == 0)
                {
                    bufferToFill.buffer->clear (channel, bufferToFill.startSample, bufferToFill.numSamples);
                }
                else
                {
                    const int actualInputChannel = channel % maxInputChannels;
                    if (!activeInputChannels[channel])
                    {
                        bufferToFill.buffer->clear (channel, bufferToFill.startSample, bufferToFill.numSamples);
                    }
                    else
                    {
                        const float* inBuffer = bufferToFill.buffer->getReadPointer (actualInputChannel,
                                                                                     bufferToFill.startSample);
                        float* outBuffer = bufferToFill.buffer->getWritePointer (channel, bufferToFill.startSample);
                        
                        for (int sample = 0; sample < bufferToFill.numSamples; ++sample)
                            //outBuffer[sample] = inBuffer[sample] * random.nextFloat() * level;
                            outBuffer[sample] = inBuffer[sample] * level;
                    }
                }
            }

        }
    }
    
    void releaseResources() override
    {
        transportSource.releaseResources();
    }
    
    void resized() override
    {
        openButton.setBounds (10, 10, getWidth() - 20, 20);
        playButton.setBounds (10, 40, getWidth() - 20, 20);
        stopButton.setBounds (10, 70, getWidth() - 20, 20);
        levelSlider.setBounds (10, 100, getWidth() - 20, 20);
        
        const Rectangle<int> thumbnailBounds (10, 100, getWidth() - 20, getHeight() - 120);
        thumbnailComp.setBounds (thumbnailBounds);
        positionOverlay.setBounds (thumbnailBounds);
    }
    
    void changeListenerCallback (ChangeBroadcaster* source) override
    {
        if (source == &transportSource)
            transportSourceChanged();
    }
    
    void buttonClicked (Button* button) override
    {
        if (button == &openButton)  openButtonClicked();
        if (button == &playButton)  playButtonClicked();
        if (button == &stopButton)  stopButtonClicked();
    }
    
    void sliderValueChanged (Slider* slider) override
    {
        //make listeners react to changes in slider values
        levelSlider.setValue(levelSlider.getValue());
    }
    
private:
    Slider levelSlider;
    Label volumeLabel;
    
    enum TransportState
    {
        Stopped,
        Starting,
        Playing,
        Stopping
    };
    
    void changeState (TransportState newState)
    {
        if (state != newState)
        {
            state = newState;
            
            switch (state)
            {
                case Stopped:
                    stopButton.setEnabled (false);
                    playButton.setEnabled (true);
                    transportSource.setPosition (0.0);
                    break;
                    
                case Starting:
                    playButton.setEnabled (false);
                    transportSource.start();
                    break;
                    
                case Playing:
                    stopButton.setEnabled (true);
                    break;
                    
                case Stopping:
                    transportSource.stop();
                    break;
                    
                default:
                    jassertfalse;
                    break;
            }
        }
    }
    
    void transportSourceChanged()
    {
        if (transportSource.isPlaying())
            changeState (Playing);
        else
            changeState (Stopped);
    }
    
    void openButtonClicked()
    {
        FileChooser chooser ("Select a Wave file to play...",
                             File::nonexistent,
                             "*.wav");
        
        if (chooser.browseForFileToOpen())
        {
            File file = chooser.getResult();
            
            if (AudioFormatReader* reader = formatManager.createReaderFor (file))
            {
                ScopedPointer<AudioFormatReaderSource> newSource = new AudioFormatReaderSource (reader, true);
                transportSource.setSource (newSource, 0, nullptr, reader->sampleRate);
                playButton.setEnabled (true);
                thumbnailComp.setFile (file);
                readerSource = newSource.release();
            }
        }
    }
    
    void playButtonClicked()
    {
        changeState (Starting);
    }
    
    void stopButtonClicked()
    {
        changeState (Stopping);
    }
    
    //==========================================================================
    TextButton openButton;
    TextButton playButton;
    TextButton stopButton;
    //Random random;
    
    AudioFormatManager formatManager;
    ScopedPointer<AudioFormatReaderSource> readerSource;
    AudioTransportSource transportSource;
    TransportState state;
    AudioFormatReader* reader;
    AudioThumbnailCache thumbnailCache;
    SimpleThumbnailComponent thumbnailComp;
    SimplePositionOverlay positionOverlay;
    
    LookAndFeel_V3 lookAndFeel;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainContentComponent)
};


Component* createMainContentComponent()     { return new MainContentComponent(); }


#endif  // MAINCOMPONENT_H_INCLUDED
