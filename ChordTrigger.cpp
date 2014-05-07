#include "AUInstrumentBase.h"
#include "ChordTriggerVersion.h"
#include "MIDIOutputCallbackHelper.h"
#include <CoreMIDI/CoreMIDI.h>
#include <list>
#include <set>
#include <algorithm>

#ifdef DEBUG
#include <fstream>
#include <ctime>
#define DEBUGLOG_B(x) \
  if (baseDebugFile.is_open()) baseDebugFile << x
#else
#define DEBUGLOG_B(x)
#endif

#define kNoteOn 0x90
#define kNoteOff 0x80

using namespace std;

class ChordTrigger : public AUMonotimbralInstrumentBase {
 public:
  ChordTrigger(AudioUnit inComponentInstance);
  ~ChordTrigger();

  OSStatus GetPropertyInfo(AudioUnitPropertyID inID, AudioUnitScope inScope,
                           AudioUnitElement inElement, UInt32 &outDataSize,
                           Boolean &outWritable);

  OSStatus GetProperty(AudioUnitPropertyID inID, AudioUnitScope inScope,
                       AudioUnitElement inElement, void *outData);

  OSStatus SetProperty(AudioUnitPropertyID inID, AudioUnitScope inScope,
                       AudioUnitElement inElement, const void *inData,
                       UInt32 inDataSize);

  OSStatus HandleMidiEvent(UInt8 status, UInt8 channel, UInt8 data1,
                           UInt8 data2, UInt32 inStartFrame);

  OSStatus Render(AudioUnitRenderActionFlags &ioActionFlags,
                  const AudioTimeStamp &inTimeStamp, UInt32 inNumberFrames);

  OSStatus Initialize();
  void Cleanup();
  OSStatus Version() { return kChordTriggerVersion; }

  AUElement *CreateElement(AudioUnitScope scope, AudioUnitElement element);

  OSStatus GetParameterInfo(AudioUnitScope inScope,
                            AudioUnitParameterID inParameterID,
                            AudioUnitParameterInfo &outParameterInfo);

 private:
  MIDIOutputCallbackHelper mCallbackHelper;
  // set<int> listNoteOn;

 protected:
#ifdef DEBUG
  ofstream baseDebugFile;
#endif
};

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#pragma mark ChordTrigger Methods
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

AUDIOCOMPONENT_ENTRY(AUMusicDeviceFactory, ChordTrigger)

static const int kNumberOfInputNotes = 5;
static const int kNumberOfOutputNotes = 5;

static const int kParameter_Ch = 0;
static const CFStringRef kParamName_Ch = CFSTR("Channel: ");
static const int kNumberOfParameters =
    kNumberOfInputNotes * (kNumberOfOutputNotes + 1) + 1;

ChordTrigger::ChordTrigger(AudioComponentInstance inComponentInstance)
    : AUMonotimbralInstrumentBase(inComponentInstance, 0, 1) {
  CreateElements();

  Globals()->UseIndexedParameters(kNumberOfParameters);
  Globals()->SetParameter(kParameter_Ch, 1);
  for (int i = 1; i < kNumberOfParameters; i++) Globals()->SetParameter(i, 0);

#ifdef DEBUG
  string bPath, bFullFileName;
  bPath = getenv("HOME");
  if (!bPath.empty()) {
    bFullFileName = bPath + "/Desktop/" + "Debug.log";
  } else {
    bFullFileName = "Debug.log";
  }

  baseDebugFile.open(bFullFileName.c_str());
  DEBUGLOG_B("Plug-in constructor invoked with parameters:" << endl);
#endif
}

ChordTrigger::~ChordTrigger() {
#ifdef DEBUG
  DEBUGLOG_B("ChordTrigger::~ChordTrigger" << endl);
#endif
}

OSStatus ChordTrigger::GetPropertyInfo(AudioUnitPropertyID inID,
                                       AudioUnitScope inScope,
                                       AudioUnitElement inElement,
                                       UInt32 &outDataSize,
                                       Boolean &outWritable) {
  if (inScope == kAudioUnitScope_Global) {
    if (inID == kAudioUnitProperty_MIDIOutputCallbackInfo) {
      outDataSize = sizeof(CFArrayRef);
      outWritable = false;
      return noErr;
    } else if (inID == kAudioUnitProperty_MIDIOutputCallback) {
      outDataSize = sizeof(AUMIDIOutputCallbackStruct);
      outWritable = true;
      return noErr;
    }
  }
  return AUMonotimbralInstrumentBase::GetPropertyInfo(inID, inScope, inElement,
                                                      outDataSize, outWritable);
}

void ChordTrigger::Cleanup() {
#ifdef DEBUG
  DEBUGLOG_B("ChordTrigger::Cleanup" << endl);
#endif
}

OSStatus ChordTrigger::Initialize() {
#ifdef DEBUG
  DEBUGLOG_B("->ChordTrigger::Initialize" << endl);
#endif

  AUMonotimbralInstrumentBase::Initialize();

#ifdef DEBUG
  DEBUGLOG_B("<-ChordTrigger::Initialize" << endl);
#endif

  return noErr;
}

AUElement *ChordTrigger::CreateElement(AudioUnitScope scope,
                                       AudioUnitElement element) {
#ifdef DEBUG
  DEBUGLOG_B("CreateElement - scope: " << scope << endl);
#endif
  switch (scope) {
    case kAudioUnitScope_Group:
      return new SynthGroupElement(this, element, new MidiControls);
    case kAudioUnitScope_Part:
      return new SynthPartElement(this, element);
    default:
      return AUBase::CreateElement(scope, element);
  }
}

OSStatus ChordTrigger::GetParameterInfo(
    AudioUnitScope inScope, AudioUnitParameterID inParameterID,
    AudioUnitParameterInfo &outParameterInfo) {

#ifdef DEBUG
  DEBUGLOG_B("GetParameterInfo - inScope: " << inScope << endl);
  DEBUGLOG_B("GetParameterInfo - inParameterID: " << inParameterID << endl);
#endif

  if (inScope != kAudioUnitScope_Global) return kAudioUnitErr_InvalidScope;

  outParameterInfo.flags += kAudioUnitParameterFlag_IsWritable;
  outParameterInfo.flags += kAudioUnitParameterFlag_IsReadable;

  if (inParameterID == kParameter_Ch) {
    AUBase::FillInParameterName(outParameterInfo, kParamName_Ch, false);
    outParameterInfo.unit = kAudioUnitParameterUnit_Indexed;
    outParameterInfo.minValue = 1;
    outParameterInfo.maxValue = 16;
    return noErr;
  } else if (inParameterID < kNumberOfParameters) {
    int input = (inParameterID - 1) / (kNumberOfOutputNotes + 1) + 1;
    int output = (inParameterID - 1) % (kNumberOfOutputNotes + 1) + 1;
    CFStringRef cfs;
    if (output == 1)
      cfs = CFStringCreateWithFormat(NULL, NULL, CFSTR("Input Note Number: %d"),
                                     input);
    else
      cfs = CFStringCreateWithFormat(
          NULL, NULL, CFSTR("Output Note Number: %d -> %d"), input, output - 1);

    AUBase::FillInParameterName(outParameterInfo, cfs, false);
    outParameterInfo.unit = kAudioUnitParameterUnit_Indexed;
    outParameterInfo.minValue = 0;
    outParameterInfo.maxValue = 127;
    return noErr;
  } else
    return kAudioUnitErr_InvalidParameter;

  return noErr;
}

OSStatus ChordTrigger::GetProperty(AudioUnitPropertyID inID,
                                   AudioUnitScope inScope,
                                   AudioUnitElement inElement, void *outData) {
  if (inScope == kAudioUnitScope_Global) {
    if (inID == kAudioUnitProperty_MIDIOutputCallbackInfo) {
      CFStringRef strs[1];
      strs[0] = CFSTR("MIDI Callback");

      CFArrayRef callbackArray =
          CFArrayCreate(NULL, (const void **)strs, 1, &kCFTypeArrayCallBacks);
      *(CFArrayRef *)outData = callbackArray;
      return noErr;
    }
  }
  return AUMonotimbralInstrumentBase::GetProperty(inID, inScope, inElement,
                                                  outData);
}

OSStatus ChordTrigger::SetProperty(AudioUnitPropertyID inID,
                                   AudioUnitScope inScope,
                                   AudioUnitElement inElement,
                                   const void *inData, UInt32 inDataSize) {
#ifdef DEBUG
  DEBUGLOG_B("SetProperty" << endl);
#endif
  if (inScope == kAudioUnitScope_Global) {
    if (inID == kAudioUnitProperty_MIDIOutputCallback) {
      if (inDataSize < sizeof(AUMIDIOutputCallbackStruct))
        return kAudioUnitErr_InvalidPropertyValue;

      AUMIDIOutputCallbackStruct *callbackStruct =
          (AUMIDIOutputCallbackStruct *)inData;
      mCallbackHelper.SetCallbackInfo(callbackStruct->midiOutputCallback,
                                      callbackStruct->userData);
      return noErr;
    }
  }
  return AUMonotimbralInstrumentBase::SetProperty(inID, inScope, inElement,
                                                  inData, inDataSize);
}

OSStatus ChordTrigger::HandleMidiEvent(UInt8 status, UInt8 channel, UInt8 data1,
                                       UInt8 data2, UInt32 inStartFrame) {
// data1 : note number, data2 : velocity

#ifdef DEBUG
  DEBUGLOG_B("HandleMidiEvent - status:"
             << (int)status << " ch:" << (int)channel << "/"
             << (Globals()->GetParameter(kParameter_Ch) - 1)
             << " data1:" << (int)data1 << " data2:" << (int)data2 << endl);
#endif

  if (channel == Globals()->GetParameter(kParameter_Ch) - 1 &&
      (status == 0x80 || status == 0x90)) {

    bool exist = false;
    for (int i = 1; i < kNumberOfParameters; i += (kNumberOfOutputNotes + 1)) {
      int noteCheckNumber = Globals()->GetParameter(i);
      if (data1 == noteCheckNumber) {
        for (int j = 1; j <= kNumberOfOutputNotes; j++) {
          int noteOnOffNumber = Globals()->GetParameter(i + j);

          if (noteOnOffNumber != 0) {
            mCallbackHelper.AddMIDIEvent(status, channel, noteOnOffNumber,
                                         data2, inStartFrame);
            /*
                        if (status == kNoteOn)
                          listNoteOn.insert(noteOnOffNumber);
                        else if (status == kNoteOff)
                          listNoteOn.erase(noteOnOffNumber);*/
          }
        }

        exist = true;
        break;
      }
    }

    if (!exist) {
      mCallbackHelper.AddMIDIEvent(status, channel, data1, data2, inStartFrame);
      /*
            if (status == kNoteOn)
              listNoteOn.insert(data1);
            else if (status == kNoteOff)
              listNoteOn.erase(data1);

      DEBUGLOG_B("can't find.." << listNoteOn.size() << endl);
       */
    }

  } else
    mCallbackHelper.AddMIDIEvent(status, channel, data1, data2, inStartFrame);

  return AUMIDIBase::HandleMidiEvent(status, channel, data1, data2,
                                     inStartFrame);
}

OSStatus ChordTrigger::Render(AudioUnitRenderActionFlags &ioActionFlags,
                              const AudioTimeStamp &inTimeStamp,
                              UInt32 inNumberFrames) {

  OSStatus result =
      AUInstrumentBase::Render(ioActionFlags, inTimeStamp, inNumberFrames);
  if (result == noErr) {
    mCallbackHelper.FireAtTimeStamp(inTimeStamp);
  }
  return result;
}
