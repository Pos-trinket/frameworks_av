/*
 * Copyright (C) 2008 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ANDROID_AUDIOSYSTEM_H_
#define ANDROID_AUDIOSYSTEM_H_

#include <sys/types.h>

#include <set>
#include <vector>

#include <android/content/AttributionSourceState.h>
#include <android/media/AudioPolicyConfig.h>
#include <android/media/AudioPortFw.h>
#include <android/media/AudioVibratorInfo.h>
#include <android/media/BnAudioFlingerClient.h>
#include <android/media/BnAudioPolicyServiceClient.h>
#include <android/media/EffectDescriptor.h>
#include <android/media/INativeSpatializerCallback.h>
#include <android/media/ISoundDose.h>
#include <android/media/ISoundDoseCallback.h>
#include <android/media/ISpatializer.h>
#include <android/media/MicrophoneInfoFw.h>
#include <android/media/RecordClientInfo.h>
#include <android/media/audio/common/AudioConfigBase.h>
#include <android/media/audio/common/AudioMMapPolicyInfo.h>
#include <android/media/audio/common/AudioMMapPolicyType.h>
#include <android/media/audio/common/AudioPort.h>
#include <media/AidlConversionUtil.h>
#include <media/AppVolume.h>
#include <media/AudioContainers.h>
#include <media/AudioDeviceTypeAddr.h>
#include <media/AudioPolicy.h>
#include <media/AudioProductStrategy.h>
#include <media/AudioVolumeGroup.h>
#include <media/AudioIoDescriptor.h>
#include <system/audio.h>
#include <system/audio_effect.h>
#include <system/audio_policy.h>
#include <utils/Errors.h>
#include <utils/Mutex.h>

using android::content::AttributionSourceState;

namespace android {

struct record_client_info {
    audio_unique_id_t riid;
    uid_t uid;
    audio_session_t session;
    audio_source_t source;
    audio_port_handle_t port_id;
    bool silenced;
};

typedef struct record_client_info record_client_info_t;

// AIDL conversion functions.
ConversionResult<record_client_info_t>
aidl2legacy_RecordClientInfo_record_client_info_t(const media::RecordClientInfo& aidl);
ConversionResult<media::RecordClientInfo>
legacy2aidl_record_client_info_t_RecordClientInfo(const record_client_info_t& legacy);

typedef void (*audio_error_callback)(status_t err);
typedef void (*dynamic_policy_callback)(int event, String8 regId, int val);
typedef void (*record_config_callback)(int event,
                                       const record_client_info_t *clientInfo,
                                       const audio_config_base_t *clientConfig,
                                       std::vector<effect_descriptor_t> clientEffects,
                                       const audio_config_base_t *deviceConfig,
                                       std::vector<effect_descriptor_t> effects,
                                       audio_patch_handle_t patchHandle,
                                       audio_source_t source);
typedef void (*routing_callback)();
typedef void (*vol_range_init_req_callback)();

class IAudioFlinger;
class String8;

namespace media {
class IAudioPolicyService;
}

class AudioSystem
{
public:

    // FIXME Declare in binder opcode order, similarly to IAudioFlinger.h and IAudioFlinger.cpp

    /* These are static methods to control the system-wide AudioFlinger
     * only privileged processes can have access to them
     */

    // mute/unmute microphone
    static status_t muteMicrophone(bool state);
    static status_t isMicrophoneMuted(bool *state);

    // set/get master volume
    static status_t setMasterVolume(float value);
    static status_t getMasterVolume(float* volume);

    // mute/unmute audio outputs
    static status_t setMasterMute(bool mute);
    static status_t getMasterMute(bool* mute);

    // set/get stream volume on specified output
    static status_t setStreamVolume(audio_stream_type_t stream, float value,
                                    audio_io_handle_t output);
    static status_t getStreamVolume(audio_stream_type_t stream, float* volume,
                                    audio_io_handle_t output);

    // mute/unmute stream
    static status_t setStreamMute(audio_stream_type_t stream, bool mute);
    static status_t getStreamMute(audio_stream_type_t stream, bool* mute);

    // set audio mode in audio hardware
    static status_t setMode(audio_mode_t mode);

    // test API: switch HALs into the mode which simulates external device connections
    static status_t setSimulateDeviceConnections(bool enabled);

    // returns true in *state if tracks are active on the specified stream or have been active
    // in the past inPastMs milliseconds
    static status_t isStreamActive(audio_stream_type_t stream, bool *state, uint32_t inPastMs);
    // returns true in *state if tracks are active for what qualifies as remote playback
    // on the specified stream or have been active in the past inPastMs milliseconds. Remote
    // playback isn't mutually exclusive with local playback.
    static status_t isStreamActiveRemotely(audio_stream_type_t stream, bool *state,
            uint32_t inPastMs);
    // returns true in *state if a recorder is currently recording with the specified source
    static status_t isSourceActive(audio_source_t source, bool *state);

    // set/get audio hardware parameters. The function accepts a list of parameters
    // key value pairs in the form: key1=value1;key2=value2;...
    // Some keys are reserved for standard parameters (See AudioParameter class).
    // The versions with audio_io_handle_t are intended for internal media framework use only.
    static status_t setParameters(audio_io_handle_t ioHandle, const String8& keyValuePairs);
    static String8  getParameters(audio_io_handle_t ioHandle, const String8& keys);
    // The versions without audio_io_handle_t are intended for JNI.
    static status_t setParameters(const String8& keyValuePairs);
    static String8  getParameters(const String8& keys);

    // Registers an error callback. When this callback is invoked, it means all
    // state implied by this interface has been reset.
    // Returns a token that can be used for un-registering.
    // Might block while callbacks are being invoked.
    static uintptr_t addErrorCallback(audio_error_callback cb);

    // Un-registers a callback previously added with addErrorCallback.
    // Might block while callbacks are being invoked.
    static void removeErrorCallback(uintptr_t cb);

    static void setDynPolicyCallback(dynamic_policy_callback cb);
    static void setRecordConfigCallback(record_config_callback);
    static void setRoutingCallback(routing_callback cb);
    static void setVolInitReqCallback(vol_range_init_req_callback cb);

    // Sets the binder to use for accessing the AudioFlinger service. This enables the system server
    // to grant specific isolated processes access to the audio system. Currently used only for the
    // HotwordDetectionService.
    static void setAudioFlingerBinder(const sp<IBinder>& audioFlinger);

    // Sets a local AudioFlinger interface to be used by AudioSystem.
    // This is used by audioserver main() to avoid binder AIDL translation.
    static status_t setLocalAudioFlinger(const sp<IAudioFlinger>& af);

    // helper function to obtain AudioFlinger service handle
    static const sp<IAudioFlinger> get_audio_flinger();
    static const sp<IAudioFlinger> get_audio_flinger_for_fuzzer();

    static float linearToLog(int volume);
    static int logToLinear(float volume);
    static size_t calculateMinFrameCount(
            uint32_t afLatencyMs, uint32_t afFrameCount, uint32_t afSampleRate,
            uint32_t sampleRate, float speed /*, uint32_t notificationsPerBufferReq*/);

    // Returned samplingRate and frameCount output values are guaranteed
    // to be non-zero if status == NO_ERROR
    // FIXME This API assumes a route, and so should be deprecated.
    static status_t getOutputSamplingRate(uint32_t* samplingRate,
            audio_stream_type_t stream);
    // FIXME This API assumes a route, and so should be deprecated.
    static status_t getOutputFrameCount(size_t* frameCount,
            audio_stream_type_t stream);
    // FIXME This API assumes a route, and so should be deprecated.
    static status_t getOutputLatency(uint32_t* latency,
            audio_stream_type_t stream);
    // returns the audio HAL sample rate
    static status_t getSamplingRate(audio_io_handle_t ioHandle,
                                          uint32_t* samplingRate);
    // For output threads with a fast mixer, returns the number of frames per normal mixer buffer.
    // For output threads without a fast mixer, or for input, this is same as getFrameCountHAL().
    static status_t getFrameCount(audio_io_handle_t ioHandle,
                                  size_t* frameCount);
    // returns the audio output latency in ms. Corresponds to
    // audio_stream_out->get_latency()
    static status_t getLatency(audio_io_handle_t output,
                               uint32_t* latency);

    // return status NO_ERROR implies *buffSize > 0
    // FIXME This API assumes a route, and so should deprecated.
    static status_t getInputBufferSize(uint32_t sampleRate, audio_format_t format,
        audio_channel_mask_t channelMask, size_t* buffSize);

    static status_t setVoiceVolume(float volume);

    // return the number of audio frames written by AudioFlinger to audio HAL and
    // audio dsp to DAC since the specified output has exited standby.
    // returned status (from utils/Errors.h) can be:
