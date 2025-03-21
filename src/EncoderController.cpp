#include "EncoderController.h"

// Global pointer for ISR to access
EncoderController *globalEncoderController = nullptr;

// ISR function that calls the controller method
void IRAM_ATTR globalEncoderISR()
{
  if (globalEncoderController)
  {
    globalEncoderController->encoderISRHandler();
  }
}

EncoderController::EncoderController(MetronomeState &state, Timing &timing)
    : state(state), timing(timing)
{
  globalEncoderController = this;
}

void EncoderController::begin()
{
  pinMode(ENCODER_A, INPUT_PULLUP);
  pinMode(ENCODER_B, INPUT_PULLUP);
  pinMode(ENCODER_BTN, INPUT_PULLUP);
  pinMode(BTN_START, INPUT_PULLUP);
  pinMode(BTN_STOP, INPUT_PULLUP);

  attachInterrupt(digitalPinToInterrupt(ENCODER_A), globalEncoderISR, CHANGE);
}

bool EncoderController::handleControls()
{
  bool stateChanged = false;
  
  // Store initial state values to detect changes
  uint16_t initialBpm = state.bpm;
  uint8_t initialMultiplierIndex = state.currentMultiplierIndex;
  MetronomeMode initialRhythmMode = state.rhythmMode;
  
  // Store initial channel states
  struct ChannelState {
    bool enabled;
    uint8_t barLength;
    uint16_t pattern;
  };
  
  ChannelState initialChannelStates[MetronomeState::CHANNEL_COUNT];
  for (uint8_t i = 0; i < MetronomeState::CHANNEL_COUNT; i++) {
    initialChannelStates[i].enabled = state.getChannel(i).isEnabled();
    initialChannelStates[i].barLength = state.getChannel(i).getBarLength();
    initialChannelStates[i].pattern = state.getChannel(i).getPattern();
  }
  
  // Call the handler methods
  handleEncoderButton();
  handleStartButton();
  handleStopButton();
  handleRotaryEncoder();
  
  // Check if any state values have changed
  if (initialBpm != state.bpm || 
      initialMultiplierIndex != state.currentMultiplierIndex ||
      initialRhythmMode != state.rhythmMode) {
    stateChanged = true;
  }
  
  // Check if any channel states have changed
  for (uint8_t i = 0; i < MetronomeState::CHANNEL_COUNT; i++) {
    const MetronomeChannel& channel = state.getChannel(i);
    if (initialChannelStates[i].enabled != channel.isEnabled() ||
        initialChannelStates[i].barLength != channel.getBarLength() ||
        initialChannelStates[i].pattern != channel.getPattern()) {
      stateChanged = true;
      break;
    }
  }
  
  return stateChanged;
}

void EncoderController::encoderISRHandler()
{
  uint8_t a = digitalRead(ENCODER_A);
  uint8_t b = digitalRead(ENCODER_B);

  if (a != lastEncA)
  {
    lastEncA = a;
    encoderValue += (a != b) ? 1 : -1;
  }
}

void EncoderController::handleEncoderButton()
{
  bool encBtn = digitalRead(ENCODER_BTN);
  uint32_t currentTime = millis();

  // Button pressed
  if (encBtn == LOW && lastEncBtn == HIGH) {
    // Start timing for long press
    buttonPressStartTime = currentTime;
    buttonLongPressActive = false;
  }
  // Button released
  else if (encBtn == HIGH && lastEncBtn == LOW) {
    // If it wasn't a long press, handle as a normal click
    if (!buttonLongPressActive) {
      // Check if rhythm mode is selected
      if (state.isRhythmModeSelected()) {
        // Toggle between polymeter and polyrhythm modes
        state.toggleRhythmMode();
        lastEncBtn = encBtn;
        return;
      }
      
      // Check if a channel toggle is selected
      for (uint8_t i = 0; i < MetronomeState::CHANNEL_COUNT; i++) {
        if (state.isToggleSelected(i)) {
          // Toggle the channel on/off
          state.getChannel(i).toggleEnabled();
          lastEncBtn = encBtn;
          return;
        }
      }
      
      // Otherwise toggle editing mode as before
      state.isEditing = !state.isEditing;
    }
    // Reset long press state on button release
    buttonLongPressActive = false;
  }
  // Button still pressed, check for long press
  else if (encBtn == LOW && !buttonLongPressActive) {
    // Check if we've held the button long enough for a long press
    if (currentTime - buttonPressStartTime > LONG_PRESS_DURATION_MS) {
      buttonLongPressActive = true;
      
      // Handle long press for BPM reset
      if (state.isBpmSelected()) {
        // Reset BPM to default (120)
        state.resetBpmToDefault();
        
        // Update the tempo
        timing.setTempo(state.bpm);
        
        // Exit editing mode
        state.isEditing = false;
        
        // Debug output
        Serial.println("BPM reset to default");
        lastEncBtn = encBtn;
        return;
      }
      
      // Handle long press for multiplier and pattern reset
      if (state.isMultiplierSelected()) {
        // Reset patterns and multiplier
        state.resetPatternsAndMultiplier();
        
        // Exit editing mode
        state.isEditing = false;
        
        // Debug output
        Serial.println("Patterns and multiplier reset");
        lastEncBtn = encBtn;
        return;
      }
      
      // Handle long press for channel bar length to reset the channel pattern
      uint8_t channelIndex = state.getActiveChannel();
      if (state.isLengthSelected(channelIndex)) {
        // Reset the channel pattern to default (only first beat active)
        state.resetChannelPattern(channelIndex);
        
        // Exit editing mode
        state.isEditing = false;
        
        // Debug output
        Serial.print("Channel ");
        Serial.print(channelIndex + 1);
        Serial.println(" pattern reset");
        lastEncBtn = encBtn;
        return;
      }
      
      // Handle long press for Euclidean rhythm reset
      if (state.isPatternSelected(channelIndex)) {
        auto &channel = state.getChannel(channelIndex);
        
        // Count active beats in current pattern
        uint16_t pattern = channel.getPattern();
        uint8_t barLength = channel.getBarLength();
        uint8_t activeBeats = 0;
        
        // Count active beats in the pattern
        // First beat is always active
        activeBeats = 1;
        
        // Count active beats in the rest of the pattern
        for (uint8_t i = 0; i < barLength - 1; i++) {
          if ((pattern >> i) & 1) {
            activeBeats++;
          }
        }
        
        // Debug output
        Serial.print("Active beats: ");
        Serial.print(activeBeats);
        Serial.print(" / Bar length: ");
        Serial.println(barLength);
        
        // Generate Euclidean rhythm with the same number of active beats
        channel.generateEuclidean(activeBeats);
        
        // Exit editing mode
        state.isEditing = false;
      }
    }
  }
  
  lastEncBtn = encBtn;
}

void EncoderController::handleStartButton()
{
  bool startBtn = digitalRead(BTN_START);

  if (startBtn != lastStartBtn && startBtn == LOW)
  {
    // If stopped, start playback
    if (!state.isRunning && !state.isPaused) {
      state.isRunning = true;
      state.isPaused = false;
      timing.start();
    } 
    // If running, pause playback
    else if (state.isRunning && !state.isPaused) {
      state.isRunning = false;
      state.isPaused = true;
      timing.pause();
    }
    // If paused, resume playback
    else if (!state.isRunning && state.isPaused) {
      state.isRunning = true;
      state.isPaused = false;
      timing.pause(); // pause() toggles between pause and resume
    }
  }
  lastStartBtn = startBtn;
}

void EncoderController::handleStopButton()
{
  bool stopBtn = digitalRead(BTN_STOP);
  bool startBtn = digitalRead(BTN_START);
  bool encoderBtn = digitalRead(ENCODER_BTN);

  // Check for factory reset combination (all three buttons pressed at once)
  if (stopBtn == LOW && startBtn == LOW && encoderBtn == LOW) {
    if (!factoryResetDetected) {
      factoryResetStartTime = millis();
      factoryResetDetected = true;
    } else if (millis() - factoryResetStartTime > FACTORY_RESET_DURATION_MS) {
      // Reset all settings to factory defaults
      state.resetBpmToDefault();
      state.resetPatternsAndMultiplier();
      
      // Clear stored configuration
      state.clearStorage();
      
      // Reset all state variables
      state.isRunning = false;
      state.isPaused = false;
      state.currentBeat = 0;
      state.globalTick = 0;
      state.lastBeatTime = 0;
      state.tickFraction = 0.0f;
      state.lastPpqnTick = 0;
      
      // Reset all channels
      for (uint8_t i = 0; i < MetronomeState::CHANNEL_COUNT; i++) {
        state.getChannel(i).resetBeat();
      }
      
      // Update tempo
      timing.setTempo(state.bpm);
      
      // Debug output
      Serial.println("FACTORY RESET PERFORMED");
      
      // Wait for buttons to be released
      while (digitalRead(BTN_STOP) == LOW || digitalRead(BTN_START) == LOW || digitalRead(ENCODER_BTN) == LOW) {
        delay(10);
      }
      
      factoryResetDetected = false;
    }
    
    // Skip normal stop button processing
    lastStopBtn = stopBtn;
    return;
  } else {
    // If any button is released, cancel factory reset detection
    if (factoryResetDetected) {
      factoryResetDetected = false;
    }
  }

  // Normal stop button processing
  if (stopBtn != lastStopBtn && stopBtn == LOW)
  {
    // Reset all state variables
    state.isRunning = false;
    state.isPaused = false;
    state.currentBeat = 0;
    state.globalTick = 0;
    state.lastBeatTime = 0;
    state.tickFraction = 0.0f;
    state.lastPpqnTick = 0;

    // Stop the clock
    timing.stop();

    // Reset all channels
    for (uint8_t i = 0; i < MetronomeState::CHANNEL_COUNT; i++)
    {
      state.getChannel(i).resetBeat();
    }
    
    // Debug output
    Serial.println("Metronome stopped and reset");
  }
  lastStopBtn = stopBtn;
}

void EncoderController::handleRotaryEncoder()
{
  static int32_t lastEncoderValue = encoderValue;
  int32_t currentStep = encoderValue / 2;
  int32_t lastStep = lastEncoderValue / 2;

  if (currentStep == lastStep)
    return;

  int32_t diff = currentStep - lastStep;
  lastEncoderValue = encoderValue;

  if (state.isEditing)
  {
    if (state.isBpmSelected())
    {
      state.bpm = constrain(state.bpm + diff, MIN_GLOBAL_BPM, MAX_GLOBAL_BPM);
      timing.setTempo(state.bpm);
    }
    else if (state.isMultiplierSelected())
    {
      state.adjustMultiplier(diff);
    }
    else
    {
      uint8_t channelIndex = state.getActiveChannel();
      auto &channel = state.getChannel(channelIndex);

      if (state.isLengthSelected(channelIndex))
      {
        channel.setBarLength(channel.getBarLength() + diff);
      }
      else if (state.isPatternSelected(channelIndex))
      {
        int newPattern = (static_cast<int>(channel.getPattern()) + channel.getMaxPattern() + 1 + diff) % (channel.getMaxPattern() + 1);
        channel.setPattern(static_cast<uint16_t>(newPattern));
      }
    }
  }
  else
  {
    int newPosition = (static_cast<int>(state.menuPosition) + state.getMenuItemsCount() + diff) % state.getMenuItemsCount();
    state.menuPosition = static_cast<MenuPosition>(newPosition);
  }
}