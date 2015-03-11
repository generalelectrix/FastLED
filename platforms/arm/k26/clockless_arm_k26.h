#ifndef __INC_CLOCKLESS_ARM_K20_H
#define __INC_CLOCKLESS_ARM_K20_H

// Definition for a single channel clockless controller for the k20 family of chips, like that used in the teensy 3.0/3.1
// See clockless.h for detailed info on how the template parameters are used.
#if defined(FASTLED_TEENSYLC)
FASTLED_NAMESPACE_BEGIN

#define FASTLED_HAS_CLOCKLESS 1

#define FASTLED_K26_TIMER_CNT FTM2_CNT
#define FASTLED_K26_TIMER_SC FTM2_SC

template <int DATA_PIN, int T1, int T2, int T3, EOrder RGB_ORDER = RGB, int XTRA0 = 0, bool FLIP = false, int WAIT_TIME = 50>
class ClocklessController : public CLEDController {
  typedef typename FastPin<DATA_PIN>::port_ptr_t data_ptr_t;
  typedef typename FastPin<DATA_PIN>::port_t data_t;

  data_t mPinMask;
  data_ptr_t mPort;
  CMinWait<WAIT_TIME> mWait;
public:
  virtual void init() {
    FastPin<DATA_PIN>::setOutput();
    mPinMask = FastPin<DATA_PIN>::mask();
    mPort = FastPin<DATA_PIN>::port();
  }

  virtual void clearLeds(int nLeds) {
    showColor(CRGB(0, 0, 0), nLeds, 0);
  }

protected:

  // set all the leds on the controller to a given color
  virtual void showColor(const struct CRGB & rgbdata, int nLeds, CRGB scale) {
    PixelController<RGB_ORDER> pixels(rgbdata, nLeds, scale, getDither());

    mWait.wait();
    showRGBInternal(pixels);
    mWait.mark();
  }

  virtual void show(const struct CRGB *rgbdata, int nLeds, CRGB scale) {
    PixelController<RGB_ORDER> pixels(rgbdata, nLeds, scale, getDither());

    mWait.wait();
    showRGBInternal(pixels);
    mWait.mark();
  }

  #ifdef SUPPORT_ARGB
  virtual void show(const struct CARGB *rgbdata, int nLeds, CRGB scale) {
    PixelController<RGB_ORDER> pixels(rgbdata, nLeds, scale, getDither());
    mWait.wait();
    showRGBInternal(pixels);
    mWait.mark();
  }
  #endif

  template<int BITS> __attribute__ ((always_inline)) inline static void writeBits(register uint32_t & next_mark, register data_ptr_t port, register data_t hi, register data_t lo, register uint8_t & b)  {
    for(register uint32_t i = BITS-1; i > 0; i--) {
      while(!(FTM2_STATUS & 0x100));
      FASTLED_K26_TIMER_CNT = 0;
      FTM2_STATUS = 0x1FF;
      FastPin<DATA_PIN>::fastset(port, hi);
      if(b&0x80) {
        while(!(FTM2_STATUS & 0x02));
        FastPin<DATA_PIN>::fastset(port, lo);
      } else {
        while(!(FTM2_STATUS & 0x01));
        FastPin<DATA_PIN>::fastset(port, lo);
      }
      b <<= 1;
    }

    while(!(FTM2_STATUS & 0x100));
    FASTLED_K26_TIMER_CNT = 0;
    FTM2_STATUS = 0x1FF;
    FastPin<DATA_PIN>::fastset(port, hi);

    if(b&0x80) {
      while(!(FTM2_STATUS & 0x02));
      FastPin<DATA_PIN>::fastset(port, lo);
    } else {
      while(!(FTM2_STATUS & 0x01));
      FastPin<DATA_PIN>::fastset(port, lo);
    }
  }

  // This method is made static to force making register Y available to use for data on AVR - if the method is non-static, then
  // gcc will use register Y for the this pointer.
  static uint32_t showRGBInternal(PixelController<RGB_ORDER> & pixels) {

    register data_ptr_t port = FastPin<DATA_PIN>::port();
    register data_t hi = *port | FastPin<DATA_PIN>::mask();;
    register data_t lo = *port & ~FastPin<DATA_PIN>::mask();;
    *port = lo;

    // Setup the pixel controller and load/scale the first byte
    pixels.preStepFirstByteDithering();
    register uint8_t b = pixels.loadAndScale0();

    cli();
    // Get access to the clock
    // SIM_SOPT2 |= SIM_SOPT2_TPMSRC(1);

    FASTLED_K26_TIMER_SC = FTM_SC_PS(0) | FTM_SC_CLKS(1);
    FTM2_CONF = 0;

    // Clear the channel modes
    FTM2_C0SC = 0;
    FTM2_C1SC = 0;

    // Set the channel values for T1
    FTM2_C0SC = FTM_CSC_MSA | FTM_CSC_ELSB | FTM_CSC_ELSA;
    FTM2_C0V = T1;

    // Set the channel values for T1+T2
    FTM2_C1SC = FTM_CSC_MSA | FTM_CSC_ELSB | FTM_CSC_ELSA;
    FTM2_C1V = (T1+T2);

    // Set the overflow for the full pixel
    FTM2_MOD = (T1 + T2 + T3);

    uint32_t next_mark = 0; //  + (T1+T2+T3);

    FASTLED_K26_TIMER_CNT = 0;
    FTM2_STATUS = 0x1FF;
    while(pixels.has(1)) {
      pixels.stepDithering();
      // #if (FASTLED_ALLOW_INTERRUPTS == 1)
      // cli();
      // // if interrupts took longer than 45µs, punt on the current frame
      // if(ARM_DWT_CYCCNT > next_mark) {
      //   if((ARM_DWT_CYCCNT-next_mark) > ((WAIT_TIME-INTERRUPT_THRESHOLD)*CLKS_PER_US)) { sei(); return ARM_DWT_CYCCNT; }
      // }
      // #endif

      // Write first byte, read next byte
      writeBits<8+XTRA0>(next_mark, port, hi, lo, b);
      b = pixels.loadAndScale1();

      // Write second byte, read 3rd byte
      writeBits<8+XTRA0>(next_mark, port, hi, lo, b);
      b = pixels.loadAndScale2();

      // Write third byte, read 1st byte of next pixel
      writeBits<8+XTRA0>(next_mark, port, hi, lo, b);
      b = pixels.advanceAndLoadAndScale0();
      // #if (FASTLED_ALLOW_INTERRUPTS == 1)
      // sei();
      // #endif
    };

    FASTLED_K26_TIMER_SC = 0;
    sei();
    return ARM_DWT_CYCCNT;
  }
};
FASTLED_NAMESPACE_END

#endif

#endif