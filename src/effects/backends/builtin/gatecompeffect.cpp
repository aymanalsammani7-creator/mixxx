// === CUSTOM MOD (rekordbox-cfx): Sweep/GateComp Sound-Color-FX processors ===
#include "effects/backends/builtin/gatecompeffect.h"

#include "effects/backends/effectmanifest.h"
#include "engine/effects/engineeffectparameter.h"
#include "util/math.h"

// static
QString GateCompEffect::getId() {
    return "org.mixxx.effects.gatecomp";
}

// static
EffectManifestPointer GateCompEffect::getManifest() {
    EffectManifestPointer pManifest(new EffectManifest());
    pManifest->setId(getId());
    pManifest->setName(QObject::tr("Gate Comp"));
    pManifest->setShortName(QObject::tr("Gate Comp"));
    pManifest->setAuthor("Mixxx custom");
    pManifest->setVersion("1.0");
    pManifest->setDescription(QObject::tr(
            "Noise gate and compressor combined in one processor, modeled "
            "after the Rekordbox GATE COMP Sound Color FX"));
    pManifest->setEffectRampsFromDry(true);

    EffectManifestParameterPointer threshold = pManifest->addParameter();
    threshold->setId("threshold");
    threshold->setName(QObject::tr("Threshold"));
    threshold->setShortName(QObject::tr("Thresh"));
    threshold->setDescription(QObject::tr(
            "Input level below which the gate ducks the signal and above "
            "which the compressor starts to reduce the gain"));
    threshold->setValueScaler(EffectManifestParameter::ValueScaler::Linear);
    threshold->setUnitsHint(EffectManifestParameter::UnitsHint::Unknown);
    threshold->setRange(kGateCompMinThreshold,
            kGateCompDefaultThreshold,
            kGateCompMaxThreshold);

    EffectManifestParameterPointer gateAmount = pManifest->addParameter();
    gateAmount->setId("gate_amount");
    gateAmount->setName(QObject::tr("Gate Amount"));
    gateAmount->setShortName(QObject::tr("Gate"));
    gateAmount->setDescription(QObject::tr(
            "How much the gate attenuates the signal below the threshold.\n"
            "At 1.0 the signal is fully muted, at 0.0 the gate is disabled"));
    gateAmount->setValueScaler(EffectManifestParameter::ValueScaler::Linear);
    gateAmount->setUnitsHint(EffectManifestParameter::UnitsHint::Unknown);
    gateAmount->setRange(0.0, kGateCompDefaultGateAmount, 1.0);

    EffectManifestParameterPointer ratio = pManifest->addParameter();
    ratio->setId("ratio");
    ratio->setName(QObject::tr("Ratio (:1)"));
    ratio->setShortName(QObject::tr("Ratio"));
    ratio->setDescription(QObject::tr(
            "Compression ratio applied above the threshold.\n"
            "For a ratio of 4:1 an input level 4 dB over the threshold "
            "leaves the output only 1 dB over it"));
    ratio->setValueScaler(EffectManifestParameter::ValueScaler::Logarithmic);
    ratio->setUnitsHint(EffectManifestParameter::UnitsHint::Coefficient);
    ratio->setRange(kGateCompMinRatio, kGateCompDefaultRatio, kGateCompMaxRatio);

    EffectManifestParameterPointer makeup = pManifest->addParameter();
    makeup->setId("makeup");
    makeup->setName(QObject::tr("Makeup Gain"));
    makeup->setShortName(QObject::tr("Makeup"));
    makeup->setDescription(QObject::tr(
            "Linear gain applied after gating and compression"));
    makeup->setValueScaler(EffectManifestParameter::ValueScaler::Linear);
    makeup->setUnitsHint(EffectManifestParameter::UnitsHint::Unknown);
    makeup->setRange(kGateCompMinMakeup,
            kGateCompDefaultMakeup,
            kGateCompMaxMakeup);

    EffectManifestParameterPointer release = pManifest->addParameter();
    release->setId("release");
    release->setName(QObject::tr("Release (ms)"));
    release->setShortName(QObject::tr("Release"));
    release->setDescription(QObject::tr(
            "Time the envelope needs to recover once the signal falls back "
            "below the threshold"));
    release->setValueScaler(EffectManifestParameter::ValueScaler::Logarithmic);
    release->setUnitsHint(EffectManifestParameter::UnitsHint::Millisecond);
    release->setRange(kGateCompMinReleaseMs,
            kGateCompDefaultReleaseMs,
            kGateCompMaxReleaseMs);

    EffectManifestParameterPointer attack = pManifest->addParameter();
    attack->setId("attack");
    attack->setName(QObject::tr("Attack (ms)"));
    attack->setShortName(QObject::tr("Attack"));
    attack->setDescription(QObject::tr(
            "Time the envelope needs to follow a rising signal"));
    attack->setValueScaler(EffectManifestParameter::ValueScaler::Logarithmic);
    attack->setUnitsHint(EffectManifestParameter::UnitsHint::Millisecond);
    attack->setRange(kGateCompMinAttackMs,
            kGateCompDefaultAttackMs,
            kGateCompMaxAttackMs);

    return pManifest;
}

void GateCompEffect::loadEngineEffectParameters(
        const QMap<QString, EngineEffectParameterPointer>& parameters) {
    m_pThresholdParameter = parameters.value("threshold");
    m_pGateAmountParameter = parameters.value("gate_amount");
    m_pRatioParameter = parameters.value("ratio");
    m_pMakeupParameter = parameters.value("makeup");
    m_pReleaseParameter = parameters.value("release");
    m_pAttackParameter = parameters.value("attack");
}

void GateCompEffect::processChannel(
        GateCompGroupState* pState,
        const CSAMPLE* pInput,
        CSAMPLE* pOutput,
        const mixxx::EngineParameters& engineParameters,
        const EffectEnableState enableState,
        const GroupFeatureState& groupFeatures) {
    Q_UNUSED(groupFeatures);

    const double sampleRate = engineParameters.sampleRate();
    const double attackParamMs = math_clamp(m_pAttackParameter->value(),
            kGateCompMinAttackMs,
            kGateCompMaxAttackMs);
    const double releaseParamMs = math_clamp(m_pReleaseParameter->value(),
            kGateCompMinReleaseMs,
            kGateCompMaxReleaseMs);

    if (enableState == EffectEnableState::Enabling) {
        // Start from a defined state: unity gain and an empty envelope.
        pState->envelope = 0.0;
        pState->smoothedGain = CSAMPLE_GAIN_ONE;
        pState->attackCoeff =
                gateCompBallisticsCoeff(attackParamMs, engineParameters);
        pState->releaseCoeff =
                gateCompBallisticsCoeff(releaseParamMs, engineParameters);
        pState->previousSampleRate = sampleRate;
        pState->previousAttackMs = attackParamMs;
        pState->previousReleaseMs = releaseParamMs;
    } else if (sampleRate != pState->previousSampleRate) {
        // A new sample rate invalidates both ballistics coefficients.
        pState->attackCoeff =
                gateCompBallisticsCoeff(attackParamMs, engineParameters);
        pState->releaseCoeff =
                gateCompBallisticsCoeff(releaseParamMs, engineParameters);
        pState->previousSampleRate = sampleRate;
        pState->previousAttackMs = attackParamMs;
        pState->previousReleaseMs = releaseParamMs;
    } else {
        if (attackParamMs != pState->previousAttackMs) {
            pState->attackCoeff =
                    gateCompBallisticsCoeff(attackParamMs, engineParameters);
            pState->previousAttackMs = attackParamMs;
        }
        if (releaseParamMs != pState->previousReleaseMs) {
            pState->releaseCoeff =
                    gateCompBallisticsCoeff(releaseParamMs, engineParameters);
            pState->previousReleaseMs = releaseParamMs;
        }
    }

    // Hard-knee gain computer settings.
    const double threshold = math_clamp(m_pThresholdParameter->value(),
            kGateCompMinThreshold,
            kGateCompMaxThreshold);
    // Gain the gate settles on below the threshold: 0 (full mute) at
    // amount 1.0, unity at amount 0.0.
    const double gateFloor = 1.0 -
            math_clamp(m_pGateAmountParameter->value(), 0.0, 1.0);
    const double inverseRatio =
            1.0 / math_clamp(m_pRatioParameter->value(),
                    kGateCompMinRatio,
                    kGateCompMaxRatio);
    const double makeup = math_clamp(m_pMakeupParameter->value(),
            kGateCompMinMakeup,
            kGateCompMaxMakeup);
    // Guards against division by zero in the gain computer below.
    const double safeThreshold = math_max(threshold, 1e-6);

    double envelope = pState->envelope;
    CSAMPLE_GAIN gain = pState->smoothedGain;

    for (SINT i = 0;
            i < engineParameters.samplesPerBuffer();
            i += engineParameters.channelCount()) {
        const double level =
                math_max(fabs(pInput[i]), fabs(pInput[i + 1]));

        // Peak envelope follower with attack/release ballistics.
        if (level > envelope) {
            envelope = level + pState->attackCoeff * (envelope - level);
        } else {
            envelope = level + pState->releaseCoeff * (envelope - level);
        }
        if (fabs(envelope) < kGateCompSilenceThreshold) {
            envelope = 0.0; // flush denormals
        }

        // Gain computer: gate below the threshold, compression above it.
        double target;
        if (envelope < safeThreshold) {
            target = gateFloor;
        } else {
            target = pow(safeThreshold / envelope, inverseRatio);
        }
        target *= makeup;

        // Smooth the combined gate/compressor/makeup gain with the same
        // ballistics to avoid zipper noise.
        if (target < gain) {
            gain = static_cast<CSAMPLE_GAIN>(
                    target + pState->attackCoeff * (gain - target));
        } else {
            gain = static_cast<CSAMPLE_GAIN>(
                    target + pState->releaseCoeff * (gain - target));
        }
        if (fabs(gain) < kGateCompSilenceThreshold) {
            gain = 0; // flush denormals
        }

        pOutput[i] = pInput[i] * gain;
        pOutput[i + 1] = pInput[i + 1] * gain;
    }

    pState->envelope = envelope;
    pState->smoothedGain = gain;
}
