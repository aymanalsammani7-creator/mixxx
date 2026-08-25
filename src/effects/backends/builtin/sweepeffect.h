// === CUSTOM MOD (rekordbox-cfx): Sweep/GateComp Sound-Color-FX processors ===
#pragma once

#include <QMap>

#include "effects/backends/effectprocessor.h"
#include "util/class.h"
#include "util/types.h"

namespace {
constexpr double kSweepBaseFrequency = 800.0;
// Full-depth LFO sweep spans 3 octaves below and above the base frequency.
constexpr double kSweepMaxFrequency = 6400.0;
constexpr double kSweepMinRateHz = 0.05;
constexpr double kSweepDefaultRateHz = 1.0;
constexpr double kSweepMaxRateHz = 20.0;
constexpr double kSweepMinResonance = 0.5;
constexpr double kSweepDefaultResonance = 2.0;
constexpr double kSweepMaxResonance = 8.0;
// Recompute the biquad coefficients only when the center frequency moved by
// more than this fraction of the cached value (performance).
constexpr double kSweepCoeffUpdateRatio = 0.005;
constexpr double kDenormalThreshold = 1e-15;
} // anonymous namespace

struct SweepGroupState : public EffectState {
    SweepGroupState(const mixxx::EngineParameters& engineParameters)
            : EffectState(engineParameters),
              lfoPhase(0.0),
              previousSampleRate(engineParameters.sampleRate()),
              // Invalid values force a coefficient computation on the first frame.
              cachedCenterFrequency(-1.0),
              cachedResonance(-1.0),
              cachedBandPass(false),
              b0(CSAMPLE_GAIN_ONE),
              b1(0),
              b2(0),
              a1(0),
              a2(0),
              z1Left(0),
              z2Left(0),
              z1Right(0),
              z2Right(0) {
    }
    ~SweepGroupState() override = default;

    double lfoPhase;
    double previousSampleRate;
    // Coefficient cache keys: recompute only when one of these changes.
    double cachedCenterFrequency;
    double cachedResonance;
    bool cachedBandPass;
    // Normalized RBJ biquad coefficients (transposed direct form II).
    CSAMPLE_GAIN b0;
    CSAMPLE_GAIN b1;
    CSAMPLE_GAIN b2;
    CSAMPLE_GAIN a1;
    CSAMPLE_GAIN a2;
    // Filter memory, one pair per channel.
    CSAMPLE_GAIN z1Left;
    CSAMPLE_GAIN z2Left;
    CSAMPLE_GAIN z1Right;
    CSAMPLE_GAIN z2Right;
};

class SweepEffect : public EffectProcessorImpl<SweepGroupState> {
  public:
    SweepEffect() = default;
    ~SweepEffect() override = default;

    static QString getId();
    static EffectManifestPointer getManifest();

    void loadEngineEffectParameters(
            const QMap<QString, EngineEffectParameterPointer>& parameters) override;

    void processChannel(
            SweepGroupState* pState,
            const CSAMPLE* pInput,
            CSAMPLE* pOutput,
            const mixxx::EngineParameters& engineParameters,
            const EffectEnableState enableState,
            const GroupFeatureState& groupFeatureState) override;

  private:
    QString debugString() const {
        return getId();
    }

    EngineEffectParameterPointer m_pSpeedParameter;
    EngineEffectParameterPointer m_pDepthParameter;
    EngineEffectParameterPointer m_pResonanceParameter;
    EngineEffectParameterPointer m_pModeParameter;

    DISALLOW_COPY_AND_ASSIGN(SweepEffect);
};
