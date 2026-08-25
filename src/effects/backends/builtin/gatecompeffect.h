// === CUSTOM MOD (rekordbox-cfx): Sweep/GateComp Sound-Color-FX processors ===
#pragma once

#include <cmath>
#include <QMap>

#include "effects/backends/effectprocessor.h"
#include "util/class.h"
#include "util/types.h"

namespace {
constexpr double kGateCompMinThreshold = 0.0;
constexpr double kGateCompDefaultThreshold = 0.05;
constexpr double kGateCompMaxThreshold = 1.0;
constexpr double kGateCompDefaultGateAmount = 1.0;
constexpr double kGateCompMinRatio = 1.0;
constexpr double kGateCompDefaultRatio = 4.0;
constexpr double kGateCompMaxRatio = 20.0;
constexpr double kGateCompMinMakeup = 0.0;
constexpr double kGateCompDefaultMakeup = 1.5;
constexpr double kGateCompMaxMakeup = 4.0;
constexpr double kGateCompMinReleaseMs = 50.0;
constexpr double kGateCompDefaultReleaseMs = 200.0;
constexpr double kGateCompMaxReleaseMs = 1000.0;
constexpr double kGateCompMinAttackMs = 1.0;
constexpr double kGateCompDefaultAttackMs = 5.0;
constexpr double kGateCompMaxAttackMs = 50.0;
// Levels below this are treated as silence (denormal flush).
constexpr double kGateCompSilenceThreshold = 1e-15;

inline double gateCompBallisticsCoeff(
        double timeMs, const mixxx::EngineParameters& engineParameters) {
    return exp(-1000.0 / (timeMs * engineParameters.sampleRate()));
}
} // anonymous namespace

struct GateCompGroupState : public EffectState {
    GateCompGroupState(const mixxx::EngineParameters& engineParameters)
            : EffectState(engineParameters),
              envelope(0),
              smoothedGain(CSAMPLE_GAIN_ONE),
              attackCoeff(gateCompBallisticsCoeff(
                      kGateCompDefaultAttackMs, engineParameters)),
              releaseCoeff(gateCompBallisticsCoeff(
                      kGateCompDefaultReleaseMs, engineParameters)),
              previousSampleRate(engineParameters.sampleRate()),
              previousAttackMs(kGateCompDefaultAttackMs),
              previousReleaseMs(kGateCompDefaultReleaseMs) {
    }
    ~GateCompGroupState() override = default;

    // Peak envelope of the input signal (max of |L|, |R| per frame).
    double envelope;
    // Smoothed final gain applied to both channels.
    CSAMPLE_GAIN smoothedGain;
    double attackCoeff;
    double releaseCoeff;
    // Ballistics cache keys: recompute the coefficients only when these change.
    double previousSampleRate;
    double previousAttackMs;
    double previousReleaseMs;
};

class GateCompEffect : public EffectProcessorImpl<GateCompGroupState> {
  public:
    GateCompEffect() = default;
    ~GateCompEffect() override = default;

    static QString getId();
    static EffectManifestPointer getManifest();

    void loadEngineEffectParameters(
            const QMap<QString, EngineEffectParameterPointer>& parameters) override;

    void processChannel(
            GateCompGroupState* pState,
            const CSAMPLE* pInput,
            CSAMPLE* pOutput,
            const mixxx::EngineParameters& engineParameters,
            const EffectEnableState enableState,
            const GroupFeatureState& groupFeatureState) override;

  private:
    QString debugString() const {
        return getId();
    }

    EngineEffectParameterPointer m_pThresholdParameter;
    EngineEffectParameterPointer m_pGateAmountParameter;
    EngineEffectParameterPointer m_pRatioParameter;
    EngineEffectParameterPointer m_pMakeupParameter;
    EngineEffectParameterPointer m_pReleaseParameter;
    EngineEffectParameterPointer m_pAttackParameter;

    DISALLOW_COPY_AND_ASSIGN(GateCompEffect);
};
