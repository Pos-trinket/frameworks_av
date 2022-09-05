/*
**
** Copyright 2012, The Android Open Source Project
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/


#define LOG_TAG "AudioFlinger"
// #define LOG_NDEBUG 0
#define ATRACE_TAG ATRACE_TAG_AUDIO

#include "Threads.h"

#include "Client.h"
#include "IAfEffect.h"
#include "MelReporter.h"
#include "ResamplerBufferProvider.h"

#include <afutils/DumpTryLock.h>
#include <afutils/Permission.h>
#include <afutils/TypedLogger.h>
#include <afutils/Vibrator.h>
#include <audio_utils/MelProcessor.h>
#include <audio_utils/Metadata.h>
#ifdef DEBUG_CPU_USAGE
#include <audio_utils/Statistics.h>
#include <cpustats/ThreadCpuUsage.h>
#endif
#include <audio_utils/channels.h>
#include <audio_utils/format.h>
#include <audio_utils/minifloat.h>
#include <audio_utils/mono_blend.h>
#include <audio_utils/primitives.h>
#include <audio_utils/safe_math.h>
#include <audiomanager/AudioManager.h>
#include <binder/IPCThreadState.h>
#include <binder/IServiceManager.h>
#include <binder/PersistableBundle.h>
#include <set>
#include <cutils/bitops.h>
#include <cutils/properties.h>
#include <fastpath/AutoPark.h>
#include <media/AudioContainers.h>
#include <media/AudioDeviceTypeAddr.h>
#include <media/AudioParameter.h>
#include <media/AudioResamplerPublic.h>
#ifdef ADD_BATTERY_DATA
#include <media/IMediaPlayerService.h>
#include <media/IMediaDeathNotifier.h>
#endif
#include <media/MmapStreamCallback.h>
#include <media/RecordBufferConverter.h>
#include <media/TypeConverter.h>
#include <media/audiohal/EffectsFactoryHalInterface.h>
#include <media/audiohal/StreamHalInterface.h>
#include <media/nbaio/AudioStreamInSource.h>
#include <media/nbaio/AudioStreamOutSink.h>
#include <media/nbaio/MonoPipe.h>
#include <media/nbaio/MonoPipeReader.h>
#include <media/nbaio/Pipe.h>
#include <media/nbaio/PipeReader.h>
#include <media/nbaio/SourceAudioBufferProvider.h>
#include <mediautils/BatteryNotifier.h>
#include <mediautils/Process.h>
#include <mediautils/SchedulingPolicyService.h>
#include <mediautils/ServiceUtilities.h>
#include <powermanager/PowerManager.h>
#include <private/android_filesystem_config.h>
#include <private/media/AudioTrackShared.h>
#include <system/audio_effects/effect_aec.h>
#include <system/audio_effects/effect_downmix.h>
#include <system/audio_effects/effect_ns.h>
#include <system/audio_effects/effect_spatializer.h>
#include <utils/Log.h>
#include <utils/Trace.h>

#include <fcntl.h>
#include <linux/futex.h>
#include <math.h>
#include <memory>
#include <pthread.h>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <sys/syscall.h>

// ----------------------------------------------------------------------------

