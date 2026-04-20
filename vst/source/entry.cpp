#include "cids.h"
#include "controller.h"
#include "processor.h"
#include "version.h"

#include "public.sdk/source/main/pluginfactory.h"

using namespace tram8;

BEGIN_FACTORY_DEF(PLUGIN_VENDOR, PLUGIN_URL, PLUGIN_EMAIL)

DEF_CLASS2(INLINE_UID_FROM_FUID(kProcessorUID),
           PClassInfo::kManyInstances,
           kVstAudioEffectClass,
           PLUGIN_NAME,
           Vst::kDistributable,
           Vst::PlugType::kInstrument,
           PLUGIN_VERSION_STR,
           kVstVersionString,
           Processor::createInstance)

DEF_CLASS2(INLINE_UID_FROM_FUID(kControllerUID),
           PClassInfo::kManyInstances,
           kVstComponentControllerClass,
           PLUGIN_NAME " Controller",
           0,
           "",
           PLUGIN_VERSION_STR,
           kVstVersionString,
           Controller::createInstance)

END_FACTORY
