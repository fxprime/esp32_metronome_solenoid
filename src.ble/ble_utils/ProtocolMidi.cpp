#include "ProtocolMidi.h"

/*
ProtocolMidi documentation found here :
https://www.midi.org/specifications-old/item/table-1-summary-of-midi-message

Pitch bend:
https://sites.uci.edu/camp2014/2014/04/30/managing-midi-pitchbend-messages/
*/

void ProtocolMidi::noteOn(uint8_t channel, uint8_t note, uint8_t velocity)
{
    uint8_t midiMessage[] = {
        (uint8_t)(0x90 | channel), // 0x90 : note on
        note,
        velocity
    };
    if(channel > 15)
        return;
    if(note > 127)
        return;
    if(velocity > 127)
        return;
    sendMessage(midiMessage, sizeof(midiMessage));
}

void ProtocolMidi::noteOff(uint8_t channel, uint8_t note, uint8_t velocity)
{
    uint8_t midiMessage[] = {
        (uint8_t)(0x80 | channel), // 0x80 : note off
        note,
        velocity
    };
    if(channel > 15)
        return;
    if(note > 127)
        return;
    if(velocity > 127)
        return;
    sendMessage(midiMessage, sizeof(midiMessage));
}

void ProtocolMidi::afterTouchPoly(uint8_t channel, uint8_t note, uint8_t pressure)
{
        uint8_t midiMessage[] = {
        (uint8_t)(0xA0 | channel), // 0xA0 : polyphonic after touch
        note,
        pressure
    };
    if(channel > 15)
        return;
    if(note > 127)
        return;
    if(pressure > 127)
        return;
    sendMessage(midiMessage, sizeof(midiMessage));
}

void ProtocolMidi::controlChange(uint8_t channel, uint8_t controller, uint8_t value)
{
    uint8_t midiMessage[] = {
        (uint8_t)(0xB0 | channel), // 0xB0 : control change
        controller,
        value
    };
    // if(channel > 15)
    //     return;
    if(controller > 127)
        return;
    if(value > 127)
        return;
    sendMessage(midiMessage, sizeof(midiMessage));
}
void ProtocolMidi::programChange(uint8_t channel, uint8_t program)
{
    uint8_t midiMessage[] = {
        (uint8_t)(0xC0 | channel), // 0xC0 : program change
        program,
    };
    if(channel > 15)
        return;
    if(program > 127)
        return;
    sendMessage(midiMessage, sizeof(midiMessage));
}

void ProtocolMidi::afterTouch(uint8_t channel, uint8_t pressure)
{
    uint8_t midiMessage[] = {
        (uint8_t)(0xD0 | channel), // 0xD0 : after touch
        pressure,
    };
    if(channel > 15)
        return;
    if(pressure > 127)
        return;
    sendMessage(midiMessage, sizeof(midiMessage));
}

void ProtocolMidi::pitchBend(uint8_t channel, uint8_t lsb, uint8_t msb) {
    uint8_t midiMessage[] = {
        (uint8_t)(0xE0 | channel), // 0xE0 : pitch bend
        lsb,
        msb
    };
    sendMessage(midiMessage, sizeof(midiMessage));
}

void ProtocolMidi::pitchBend(uint8_t channel, uint16_t value)
{
    value <<= 1;
    byte msb = highByte(value);
    byte lsb = lowByte(value) >> 1;
    pitchBend(channel, lsb, msb);
}

void ProtocolMidi::pitchBend(uint8_t channel, float semitones, float range)
{
    if(semitones < -range/2 || semitones > range/2)
        return;
    uint16_t integerValue = semitones * 16384 / range + 8192;
    pitchBend(channel, integerValue);
}

// ###################################
// IO

void ProtocolMidi::midi_receivePacket(uint8_t *data, uint16_t size)
{
    debug.print("Received data : ");
    for(uint8_t i=0; i<size; i++)
        debug.printf("%x ", data[i]);
    debug.println();

    if(size < 3) {
        debug.println("Invalid packet (size < 3)");
        return;
    }

    if((!(data[0] & 0b10000000)) || (!(data[1] & 0b10000000))) {
        debug.println("Invalid packet");
        return;
    }

    currentTimestamp = ((data[0] & 0b111111) << 7);

    uint8_t *ptr = &data[1];

    uint8_t runningStatus = 0;

    while(ptr - data < size) {
        if(ptr[0] & 0b10000000) {
            currentTimestamp = (currentTimestamp & 0b1111110000000) | (ptr[0] & 0b1111111);
            ptr++;
        }

        if(ptr[0] & 0b10000000) {   // Full midi message
            runningStatus = *ptr ;
            ptr++;
        }

        uint8_t command = runningStatus >> 4;
        uint8_t channel = runningStatus & 0b1111;

        switch(command) {
            case 0:
                debug.println("Invalid packet : a running status message must be preceded by a full midi message");
                return;
            case 0b1000: {    // Note off
                uint8_t note = ptr[0];
                uint8_t velocity = ptr[1];
                ptr += 2;
                if(noteOffCallback != nullptr)
                    noteOffCallback(channel, note, velocity, currentTimestamp);
                debug.printf("Note off, channel %d, note %d, velocity %d\n", channel, note, velocity);
                break;
            }

            case 0b1001: {   // Note on
                uint8_t note = ptr[0];
                uint8_t velocity = ptr[1];
                ptr += 2;
                if(noteOnCallback != nullptr)
                    noteOnCallback(channel, note, velocity, currentTimestamp);
                debug.printf("Note on, channel %d, note %d, velocity %d\n", channel, note, velocity);
                break;
            }

            case 0b1010: {    // Polyphonic after touch
                uint8_t note = ptr[0];
                uint8_t pressure = ptr[1];
                ptr += 2;
                if(afterTouchPolyCallback != nullptr)
                    afterTouchPolyCallback(channel, note, pressure, currentTimestamp);
                debug.printf("Polyphonic after touch, channel %d, note %d, pressure %d\n", channel, note, pressure);
                break;
            }

            case 0b1011: {    // Control Change
                uint8_t controller = ptr[0];
                uint8_t value = ptr[1];
                ptr += 2;
                if(controlChangeCallback != nullptr)
                    controlChangeCallback(channel, controller, value, currentTimestamp);
                debug.printf("Control Change, channel %d, controller %d, value %d\n", channel, controller, value);
                
                break;
            }

            case 0b1100: {    // Program Change
                uint8_t program = ptr[0];
                ptr++;
                if(programChangeCallback != nullptr)
                    programChangeCallback(channel, program, currentTimestamp);
                debug.printf("Program Change, channel %d, program %d\n", channel, program);
                break;
            }

            case 0b1101: {    // After touch
                uint8_t pressure = ptr[0];
                ptr++;
                if(afterTouchCallback != nullptr)
                    afterTouchCallback(channel, pressure, currentTimestamp);
                debug.printf("After touch, channel %d, pressure %d\n", channel, pressure);
                break;
            }

            case 0b1110: {    // Pitch bend
                uint8_t lsb = ptr[0];
                uint8_t msb = ptr[1];
                ptr += 2;
                if(pitchBendCallback != nullptr)
                    pitchBendCallback(channel, lsb, msb, currentTimestamp);
                debug.printf("Pitch bend, channel %d, lsb %d, msb %d\n", channel, lsb, msb);
                uint16_t integerPitchBend = ((msb & 127) << 7) | (lsb & 127);
                if(pitchBendCallback2 != nullptr)
                    pitchBendCallback2(channel, integerPitchBend, currentTimestamp);
                debug.printf("Integer value of pitch bend : %d\n", integerPitchBend);
                break;
            }

            case 0b1111:

                if(channel == 0xc) {
                    if(stopCallback != nullptr)
                        stopCallback();
                }

                break;

            default:
                debug.println("Invalid packet");
                return;
                break;

        }
    }
}

void ProtocolMidi::mmcPlay(void)
{
    sendMMC(MMC_PLAY);
}

void ProtocolMidi::mmcDeferredPlay(void)
{
    sendMMC(MMC_DEFERRED_PLAY);
}

void ProtocolMidi::mmcPause(void)
{
    sendMMC(MMC_PAUSE);
}

void ProtocolMidi::mmcStop(void)
{
    sendMMC(MMC_STOP);
}

void ProtocolMidi::mmcRecordStrobe(void)
{
    sendMMC(MMC_RECORD_STROBE);
}

void ProtocolMidi::mmcRecordExit(void)
{
    sendMMC(MMC_RECORD_EXIT);
}

void ProtocolMidi::mmcRecordPause(void)
{
    sendMMC(MMC_RECORD_PAUSE);
}

void ProtocolMidi::mmcEject(void)
{
    sendMMC(MMC_EJECT);
}

void ProtocolMidi::mmcChase(void)
{
    sendMMC(MMC_CHASE);
}

void ProtocolMidi::mmcReset(void)
{
    sendMMC(MMC_RESET);
}

void ProtocolMidi::mmcFastForward(void)
{
    sendMMC(MMC_FAST_FORWARD);
}

void ProtocolMidi::mmcRewind(void)
{
    sendMMC(MMC_REWIND);
}


void ProtocolMidi::sendMMC(mmc_t command)
{
    switch(command) 
    {
        case MMC_STOP:
        case MMC_PLAY:
        case MMC_DEFERRED_PLAY:
        case MMC_FAST_FORWARD:
        case MMC_REWIND:
        case MMC_RECORD_STROBE:
        case MMC_RECORD_EXIT:
        case MMC_RECORD_PAUSE:
        case MMC_PAUSE:
        case MMC_EJECT:
        case MMC_CHASE:
        case MMC_RESET:
            break;
        default:
            debug.print("Warning: Unsupported MMC command");
            break;
    }

    uint8_t midiMessage[] = {
        0xF0,    //sysex
        0x7F,     //
        0x7F,     //all devices
        0x06,     //MIDI Machine Control Command
        command,
        0xF7    //end of sysex
    };
    sendMessage(midiMessage, sizeof(midiMessage));

}


void ProtocolMidi::sendMessage(uint8_t *message, uint16_t messageSize)
{
    uint8_t packet[messageSize+2];

    auto t = millis();
    uint8_t headerByte = (1 << 7) | ((t >> 7) & ((1 << 6) - 1));
    uint8_t timestampByte = (1 << 7) | (t & ((1 << 7) - 1));

    packet[0] = headerByte;
    packet[1] = timestampByte;
    for(int i = 0; i < messageSize; i++)
        packet[i+2] = message[i];
    sendPacket(packet, messageSize + 2); 

}

// ###################################
// Callbacks

void ProtocolMidi::setNoteOnCallback(void (*callback)(uint8_t, uint8_t, uint8_t, uint16_t))
{
    noteOnCallback = callback;
}

void ProtocolMidi::setNoteOffCallback(void (*callback)(uint8_t, uint8_t, uint8_t, uint16_t))
{
    noteOffCallback = callback;
}

void ProtocolMidi::setAfterTouchPolyCallback(void (*callback)(uint8_t, uint8_t, uint8_t, uint16_t))
{
    afterTouchPolyCallback = callback;
}

void ProtocolMidi::setControlChangeCallback(void (*callback)(uint8_t, uint8_t, uint8_t, uint16_t))
{
    controlChangeCallback = callback;
}

void ProtocolMidi::setProgramChangeCallback(void (*callback)(uint8_t, uint8_t, uint16_t))
{
    programChangeCallback = callback;
}

void ProtocolMidi::setAfterTouchCallback(void (*callback)(uint8_t, uint8_t, uint16_t))
{
    afterTouchCallback = callback;
}

void ProtocolMidi::setPitchBendCallback(void (*callback)(uint8_t, uint8_t, uint8_t, uint16_t))
{
    pitchBendCallback = callback;
}

void ProtocolMidi::setPitchBendCallback(void (*callback)(uint8_t, uint16_t, uint16_t))
{
    pitchBendCallback2 = callback;
}

void ProtocolMidi::setStopCallback(void (*callback)(void))
{
    stopCallback = callback;
}

void ProtocolMidi::enableDebugging(Stream& debugStream) {
    debug.enable(debugStream);
}

void ProtocolMidi::disableDebugging()
{
    debug.disable();
}
