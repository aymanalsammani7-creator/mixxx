// === CUSTOM MOD (rekordbox-mergefx): simple sine-wave generator for MERGE FX ===
#pragma once

#include "effects/backends/effectprocessor.h"
#include "util/class.h"
#include "util/types.h"

class SineWaveGroupState final : public EffectState {
   public:
    SineWaveGroupState(const mixxx::EngineParameters& engineParameters)
            : EffectState(engineParameters),
              phase(0.0),
              previousDryWet(0.0) {
    }
    ~SineWaveGroupState() override = default;

    double phase;
    double previousDryWet;
};

class SineWaveEffect : public EffectProcessorImpl<SineWaveGroupState> {
   public:
    SineWaveEffect() = default;
    ~SineWaveEffect() override = default;

    static QString getId();
    static EffectManifestPointer getManifest();

    void loadEngineEffectParameters(
            const QMap<QString, EngineEffectParameterPointer>& parameters) override;

    void processChannel(
            SineWaveGroupState* pState,
            const CSAMPLE* pInput,
            CSAMPLE* pOutput,
            const mixxx::EngineParameters& engineParameters,
            const EffectEnableState enableState,
            const GroupFeatureState& groupFeatures) override;

   private:
    EngineEffectParameterPointer m_pFrequencyParameter;
    EngineEffectParameterPointer m_pDryWetParameter;

    DISALLOW_COPY_AND_ASSIGN(SineWaveEffect);
};
