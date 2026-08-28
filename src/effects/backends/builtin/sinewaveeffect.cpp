#include "effects/backends/builtin/sinewaveeffect.h"

#include <cmath>

#include "effects/backends/effectmanifest.h"
#include "engine/effects/engineeffectparameter.h"
#include "util/rampingvalue.h"

namespace {
const QString kFrequencyParameterId = QStringLiteral("frequency");
const QString kDryWetParameterId = QStringLiteral("dry_wet");
} // anonymous namespace

// static
QString SineWaveEffect::getId() {
    return QStringLiteral("org.mixxx.effects.sinewave");
}

// static
EffectManifestPointer SineWaveEffect::getManifest() {
    EffectManifestPointer pManifest(new EffectManifest());
    pManifest->setId(getId());
    pManifest->setName(QObject::tr("Sine Wave"));
    pManifest->setAuthor(QObject::tr("Mixxx custom"));
    pManifest->setVersion(QStringLiteral("1.0"));
    pManifest->setDescription(QObject::tr("Generates a sine wave tone"));
    pManifest->setEffectRampsFromDry(true);

    EffectManifestParameterPointer frequency = pManifest->addParameter();
    frequency->setId(kFrequencyParameterId);
    frequency->setName(QObject::tr("Frequency"));
    frequency->setDescription(QObject::tr("Sine wave frequency in Hz"));
    frequency->setValueScaler(EffectManifestParameter::ValueScaler::Logarithmic);
    frequency->setUnitsHint(EffectManifestParameter::UnitsHint::Unknown);
    frequency->setRange(20.0, 2000.0, 440.0);

    EffectManifestParameterPointer drywet = pManifest->addParameter();
    drywet->setId(kDryWetParameterId);
    drywet->setName(QObject::tr("Dry/Wet"));
    drywet->setDescription(QObject::tr("Crossfade between input and generated tone"));
    drywet->setValueScaler(EffectManifestParameter::ValueScaler::Logarithmic);
    drywet->setUnitsHint(EffectManifestParameter::UnitsHint::Unknown);
    drywet->setDefaultLinkType(EffectManifestParameter::LinkType::Linked);
    drywet->setRange(0, 1, 1);

    return pManifest;
}

void SineWaveEffect::loadEngineEffectParameters(
        const QMap<QString, EngineEffectParameterPointer>& parameters) {
    m_pFrequencyParameter = parameters.value(kFrequencyParameterId);
    m_pDryWetParameter = parameters.value(kDryWetParameterId);
}

void SineWaveEffect::processChannel(
        SineWaveGroupState* pState,
        const CSAMPLE* pInput,
        CSAMPLE* pOutput,
        const mixxx::EngineParameters& engineParameters,
        const EffectEnableState enableState,
        const GroupFeatureState& groupFeatures) {
    Q_UNUSED(groupFeatures);

    SineWaveGroupState& gs = *pState;

    const double frequency = m_pFrequencyParameter->value();
    const double phaseIncrement = 2.0 * M_PI * frequency / engineParameters.sampleRate();
    const CSAMPLE dryWet = static_cast<CSAMPLE>(m_pDryWetParameter->value());
    RampingValue<CSAMPLE_GAIN> drywet_ramping_value(
            gs.previousDryWet, dryWet, engineParameters.samplesPerBuffer());

    for (SINT i = 0; i < engineParameters.samplesPerBuffer(); ++i) {
        CSAMPLE_GAIN drywet_ramped = drywet_ramping_value.getNth(i);
        CSAMPLE tone = static_cast<CSAMPLE>(std::sin(gs.phase));
        gs.phase += phaseIncrement;
        if (gs.phase > 2.0 * M_PI) {
            gs.phase -= 2.0 * M_PI;
        }
        pOutput[i] = pInput[i] * (1.0 - drywet_ramped) + tone * drywet_ramped;
    }

    if (enableState == EffectEnableState::Disabling) {
        gs.previousDryWet = 0;
    } else {
        gs.previousDryWet = dryWet;
    }
}
