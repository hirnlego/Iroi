#pragma once

#include "Commons.h"
#include "Ambience.h"
#include "Filter.h"
#include "Resonator.h"
#include "Echo.h"
#include "Schmitt.h"
#include "TGate.h"
#include "EnvFollower.h"
#include "DcBlockingFilter.h"
#include "SmoothValue.h"
#include "Modulation.h"
#include "Limiter.h"
#include "Compressor.h"
#include "BiquadFilter.h"

class Iroi
{
private:
    PatchCtrls* patchCtrls_;
    PatchCvs* patchCvs_;
    PatchState* patchState_;

    Filter* filter_;
    Resonator* resonator_;
    Echo* echo_;
    Ambience* ambience_;
    Limiter* limiter_;

    BiquadFilter* lpf_[2];
    Compressor* comp_;
    
    Modulation* modulation_;

    StereoDcBlockingFilter* inputDcFilter_;
    StereoDcBlockingFilter* outputDcFilter_;

    EnvFollower* outEnvFollower_[2];

    FilterPosition filterPosition_, lastFilterPosition_;

    bool bypass_;

public:
    Iroi(PatchCtrls* patchCtrls, PatchCvs* patchCvs, PatchState* patchState)
    {
        patchCtrls_ = patchCtrls;
        patchCvs_ = patchCvs;
        patchState_ = patchState;

        filter_ = Filter::create(patchCtrls_, patchCvs_, patchState_);
        resonator_ = Resonator::create(patchCtrls_, patchCvs_, patchState_);
        echo_ = Echo::create(patchCtrls_, patchCvs_, patchState_);
        ambience_ = Ambience::create(patchCtrls_, patchCvs_, patchState_);

        modulation_ = Modulation::create(patchCtrls_, patchCvs_, patchState_);

        limiter_ = Limiter::create();
        comp_ = Compressor::create(patchState_->sampleRate);
        comp_->setAttack(20.f);
        comp_->setRelease(100.f);
        comp_->setRatio(20.f);
        comp_->setThreshold(-35.f);

        for (size_t i = 0; i < 2; i++)
        {
            outEnvFollower_[i] = EnvFollower::create();
            outEnvFollower_[i]->setLambda(0.9f);

            lpf_[i] = BiquadFilter::create(patchState_->sampleRate);
            lpf_[i]->setHighShelf(8000.f, -6.f);
        }

        inputDcFilter_ = StereoDcBlockingFilter::create();
        outputDcFilter_ = StereoDcBlockingFilter::create();

        bypass_ = false;
    }
    ~Iroi()
    {
        Filter::destroy(filter_);
        Resonator::destroy(resonator_);
        Echo::destroy(echo_);
        Ambience::destroy(ambience_);
        Modulation::destroy(modulation_);
        Limiter::destroy(limiter_);
        Compressor::destroy(comp_);

        for (size_t i = 0; i < 2; i++)
        {
            EnvFollower::destroy(outEnvFollower_[i]);
            BiquadFilter::destroy(lpf_[i]);
        }
    }

    static Iroi* create(PatchCtrls* patchCtrls, PatchCvs* patchCvs, PatchState* patchState)
    {
        return new Iroi(patchCtrls, patchCvs, patchState);
    }

    static void destroy(Iroi* obj)
    {
        delete obj;
    }

    inline void Process(AudioBuffer &buffer)
    {
        if (bypass_)
        {
            return;
        }

        FloatArray left = buffer.getSamples(LEFT_CHANNEL);
        FloatArray right = buffer.getSamples(RIGHT_CHANNEL);

        inputDcFilter_->process(buffer, buffer);

        const int size = buffer.getSize();

        modulation_->Process();

        //buffer.multiply(kInputGain);

        //left.clip(0.01f);
        //right.clip(0.01f);

        //lpf_[LEFT_CHANNEL]->process(buffer.getSamples(LEFT_CHANNEL), buffer.getSamples(LEFT_CHANNEL));
        //lpf_[RIGHT_CHANNEL]->process(buffer.getSamples(RIGHT_CHANNEL), buffer.getSamples(RIGHT_CHANNEL));


        /*
        limiter_->ProcessSoft(buffer, buffer);
        comp_->setRatio(20.f * patchCtrls_->echoDensity);
        comp_->setThreshold(-100.f * patchCtrls_->echoRepeats);
        debugMessage("c", comp_->getRatio(), comp_->getThreshold());
        comp_->process(buffer, buffer);
        */

        if (patchCtrls_->filterPosition < 0.25f)
        {
            filterPosition_ = FilterPosition::POSITION_1;
        }
        else if (patchCtrls_->filterPosition >= 0.25f && patchCtrls_->filterPosition < 0.5f)
        {
            filterPosition_ = FilterPosition::POSITION_2;
        }
        else if (patchCtrls_->filterPosition >= 0.5f && patchCtrls_->filterPosition < 0.75f)
        {
            filterPosition_ = FilterPosition::POSITION_3;
        }
        else if (patchCtrls_->filterPosition >= 0.75f)
        {
            filterPosition_ = FilterPosition::POSITION_4;
        }

        if (filterPosition_ != lastFilterPosition_)
        {
            lastFilterPosition_ = filterPosition_;
            patchState_->filterPositionFlag = true;
        }
        else
        {
            patchState_->filterPositionFlag = false;
        }

        if (FilterPosition::POSITION_1 == filterPosition_)
        {
            filter_->process(buffer, buffer);
        }
        resonator_->process(buffer, buffer);
        if (FilterPosition::POSITION_2 == filterPosition_)
        {
            filter_->process(buffer, buffer);
        }
        echo_->process(buffer, buffer);
        if (FilterPosition::POSITION_3 == filterPosition_)
        {
            filter_->process(buffer, buffer);
        }
        ambience_->process(buffer, buffer);
        if (FilterPosition::POSITION_4 == filterPosition_)
        {
            filter_->process(buffer, buffer);
        }

        //outputDcFilter_->process(buffer, buffer);
        
        /*
        buffer.multiply(kOutputMakeupGain);
        limiter_->ProcessSoft(buffer, buffer);
        
        buffer.multiply(patchState_->outLevel);
        */

        // Input leds.
        for (size_t i = 0; i < size; i++)
        {
            patchState_->outputLevel[i] = Mix2(outEnvFollower_[0]->process(left[i]), outEnvFollower_[1]->process(right[i]));
        }
    }
};

