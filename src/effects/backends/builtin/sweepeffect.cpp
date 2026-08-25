// === CUSTOM MOD (rekordbox-cfx): Sweep/GateComp Sound-Color-FX processors ===
#include "effects/backends/builtin/sweepeffect.h"

#include "effects/backends/effectmanifest.h"
#include "engine/effects/engineeffectparameter.h"
#include "util/math.h"

namespace {

// RBJ cookbook (Audio EQ Cookbook by Robert Bristow-Johnson) coefficients for
// a 2-pole band-reject (notch) or constant 0 dB peak gain band-pass biquad,
// normalized and stored in transposed direct form II layout.
inline void computeSweepCoefficients(
        SweepGroupState* pState,
        double centerFrequency,
        double resonance,
        bool bandPass,
        double sampleRate) {
    const double w0 = 2.0 * M_PI * centerFrequency / sampleRate;
    const double cosw0 = cos(w0);
    const double alpha = sin(w0) / (2.0 * resonance);

    const double a0 = 1.0 + alpha;
    const double a1 = -2.0 * cosw0;
    const double a2 = 1.0 - alpha;

    double b0;
    double b1;
    double b2;
    if (bandPass) {
        b0 = alpha;
        b1 = 0.0;
        b2 = -alpha;
    } else {
        // Band-reject / notch
        b0 = 1.0;
        b1 = -2.0 * cosw0;
        b2 = 1.0;
    }

    pState->b0 = static_cast<CSAMPLE_GAIN>(b0 / a0);
    pState->b1 = static_cast<CSAMPLE_GAIN>(b1 / a0);
    pState->b2 = static_cast<CSAMPLE_GAIN>(b2 / a0);
    pState->a1 = static_cast<CSAMPLE_GAIN>(a1 / a0);
    pState->a2 = static_cast<CSAMPLE_GAIN>(a2 / a0);
}

} // anonymous namespace

// static
QString SweepEffect::getId() {
    return "org.mixxx.effects.sweep";
}

// static
EffectManifestPointer SweepEffect::getManifest() {
    EffectManifestPointer pManifest(new EffectManifest());
    pManifest->setId(getId());
    pManifest->setName(QObject::tr("Sweep"));
    pManifest->setShortName(QObject::tr("Sweep"));
    pManifest->setAuthor("Mixxx custom");
    pManifest->setVersion("1.0");
    pManifest->setDescription(QObject::tr(
            "Resonant band-reject/band-pass filter whose center frequency is "
            "swept by an LFO, modeled after the Rekordbox SWEEP Sound Color FX"));
    pManifest->setEffectRampsFromDry(true);

    EffectManifestParameterPointer speed = pManifest->addParameter();
    speed->setId("speed");
    speed->setName(QObject::tr("LFO Rate"));
    speed->setShortName(QObject::tr("Rate"));
    speed->setDescription(QObject::tr(
            "Speed of the LFO (low frequency oscillator) that sweeps the "
            "filter center frequency"));
    speed->setValueScaler(EffectManifestParameter::ValueScaler::Logarithmic);
    speed->setUnitsHint(EffectManifestParameter::UnitsHint::Hertz);
    speed->setRange(kSweepMinRateHz, kSweepDefaultRateHz, kSweepMaxRateHz);

    EffectManifestParameterPointer depth = pManifest->addParameter();
    depth->setId("depth");
    depth->setName(QObject::tr("Depth"));
    depth->setShortName(QObject::tr("Depth"));
    depth->setDescription(QObject::tr(
            "How far the filter center frequency is swept around its "
            "base frequency"));
    depth->setValueScaler(EffectManifestParameter::ValueScaler::Linear);
    depth->setUnitsHint(EffectManifestParameter::UnitsHint::Unknown);
    depth->setDefaultLinkType(EffectManifestParameter::LinkType::Linked);
    depth->setRange(0.0, 1.0, 1.0);

    EffectManifestParameterPointer resonance = pManifest->addParameter();
    resonance->setId("resonance");
    resonance->setName(QObject::tr("Resonance"));
    resonance->setShortName(QObject::tr("Reso"));
    resonance->setDescription(QObject::tr(
            "Resonance (Q factor) of the swept filter.\n"
            "Higher values produce a narrower, more resonant sweep"));
    resonance->setValueScaler(EffectManifestParameter::ValueScaler::Logarithmic);
    resonance->setUnitsHint(EffectManifestParameter::UnitsHint::Coefficient);
    resonance->setRange(kSweepMinResonance, kSweepDefaultResonance,
            kSweepMaxResonance);

    EffectManifestParameterPointer mode = pManifest->addParameter();
    mode->setId("mode");
    mode->setName(QObject::tr("Band-pass Mode"));
    mode->setShortName(QObject::tr("BPF"));
    mode->setDescription(QObject::tr(
            "Toggle between band-reject (notch) and band-pass filter mode"));
    mode->setValueScaler(EffectManifestParameter::ValueScaler::Toggle);
    mode->setUnitsHint(EffectManifestParameter::UnitsHint::Unknown);
    mode->setRange(0, 0, 1);
    mode->appendStep(qMakePair(QObject::tr("Off"), 0.0));
    mode->appendStep(qMakePair(QObject::tr("On"), 1.0));

    return pManifest;
}

void SweepEffect::loadEngineEffectParameters(
        const QMap<QString, EngineEffectParameterPointer>& parameters) {
    m_pSpeedParameter = parameters.value("speed");
    m_pDepthParameter = parameters.value("depth");
    m_pResonanceParameter = parameters.value("resonance");
    m_pModeParameter = parameters.value("mode");
}

void SweepEffect::processChannel(
        SweepGroupState* pState,
        const CSAMPLE* pInput,
        CSAMPLE* pOutput,
        const mixxx::EngineParameters& engineParameters,
        const EffectEnableState enableState,
        const GroupFeatureState& groupFeatures) {
    Q_UNUSED(groupFeatures);

    const double sampleRate = engineParameters.sampleRate();
    const double speed = math_clamp(m_pSpeedParameter->value(),
            kSweepMinRateHz,
            kSweepMaxRateHz);
    const double depth = math_clamp(m_pDepthParameter->value(), 0.0, 1.0);
    const double resonance = math_clamp(m_pResonanceParameter->value(),
            kSweepMinResonance,
            kSweepMaxResonance);
    const bool bandPass = m_pModeParameter->toInt() != 0;

    if (enableState == EffectEnableState::Enabling ||
            sampleRate != pState->previousSampleRate) {
        // Clear the filter memory so stale energy does not bleed through and
        // invalidate the cached coefficients (the sample rate may have changed).
        pState->z1Left = 0;
        pState->z2Left = 0;
        pState->z1Right = 0;
        pState->z2Right = 0;
        pState->cachedCenterFrequency = -1.0;
        pState->previousSampleRate = sampleRate;
    }

    // The LFO phase is advanced once per frame.
    const double phaseIncrement = 2.0 * M_PI * speed / sampleRate;
    const double frequencyRatio = kSweepMaxFrequency / kSweepBaseFrequency;

    for (SINT i = 0;
            i < engineParameters.samplesPerBuffer();
            i += engineParameters.channelCount()) {
        pState->lfoPhase += phaseIncrement;
        if (pState->lfoPhase >= 2.0 * M_PI) {
            pState->lfoPhase -= 2.0 * M_PI;
        }

        // Exponential frequency modulation: +/- 3 octaves at full depth.
        const double centerFrequency = math_clamp(
                kSweepBaseFrequency *
                        pow(frequencyRatio,
                                sin(pState->lfoPhase) * depth),
                20.0,
                sampleRate * 0.45);

        // Recompute the biquad coefficients only when the center frequency
        // moved by more than 0.5 % or the Q/mode changed (performance).
        if (pState->cachedCenterFrequency <= 0.0 ||
                fabs(centerFrequency - pState->cachedCenterFrequency) >
                        kSweepCoeffUpdateRatio *
                                pState->cachedCenterFrequency ||
                resonance != pState->cachedResonance ||
                bandPass != pState->cachedBandPass) {
            computeSweepCoefficients(pState,
                    centerFrequency,
                    resonance,
                    bandPass,
                    sampleRate);
            pState->cachedCenterFrequency = centerFrequency;
            pState->cachedResonance = resonance;
            pState->cachedBandPass = bandPass;
        }

        const CSAMPLE xLeft = pInput[i];
        const CSAMPLE xRight = pInput[i + 1];

        // Transposed direct form II biquad, independent state per channel.
        const CSAMPLE_GAIN yLeft = pState->b0 * xLeft + pState->z1Left;
        pState->z1Left =
                pState->b1 * xLeft - pState->a1 * yLeft + pState->z2Left;
        pState->z2Left = pState->b2 * xLeft - pState->a2 * yLeft;

        const CSAMPLE_GAIN yRight = pState->b0 * xRight + pState->z1Right;
        pState->z1Right =
                pState->b1 * xRight - pState->a1 * yRight + pState->z2Right;
        pState->z2Right = pState->b2 * xRight - pState->a2 * yRight;

        // Flush denormals in the feedback paths.
        if (fabs(pState->z1Left) < kDenormalThreshold) {
            pState->z1Left = 0;
        }
        if (fabs(pState->z2Left) < kDenormalThreshold) {
            pState->z2Left = 0;
        }
        if (fabs(pState->z1Right) < kDenormalThreshold) {
            pState->z1Right = 0;
        }
        if (fabs(pState->z2Right) < kDenormalThreshold) {
            pState->z2Right = 0;
        }

        pOutput[i] = yLeft;
        pOutput[i + 1] = yRight;
    }
}
