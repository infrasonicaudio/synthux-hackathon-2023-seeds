#ifndef DSY_PANNER_H
#define DSY_PANNER_H

#include <DaisyDuino.h>

class Panner
{
    public:
        Panner() = default;
        ~Panner() = default;

        inline void Init()
        {
            pan_ = HALFPI_F * 0.5f;
        }

        inline void Process(float in, float *out_l, float *out_r)
        {
          const float scale_l = cosf(pan_);
          const float scale_r = sinf(pan_);
          *out_l = in * scale_l;
          *out_r = in * scale_r;
        }

        /// Set pan, range -1 (hard left) to 1 (hard right)
        inline void SetPan(const float pan)
        {
            pan_ = (daisysp::fclamp(pan, -1.0f, 1.0f) * 0.5f + 0.5f) * HALFPI_F;
        }

    private:
        float pan_;
};

#endif