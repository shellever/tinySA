/* All rights reserved.
 *
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3, or (at your option)
 * any later version.
 *
 * The software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GNU Radio; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street,
 * Boston, MA 02110-1301, USA.
 */


#include "SI4432.h"		// comment out for simulation

int dirty = true;
int scandirty = true;

extern int actualStepDelay;

setting_t setting;
uint32_t frequencies[POINTS_COUNT];

float actual_rbw = 0;
int vbwSteps = 1;
uint32_t minFreq = 0;
uint32_t maxFreq = 520000000;

//int setting.refer = -1;  // Off by default
int const reffer_freq[] = {30000000, 15000000, 10000000, 4000000, 3000000, 2000000, 1000000};

int in_selftest = false;

#if 0
const char *dummy = "this is a very long string only used to fill memory so I know when the memory is full and I can remove some of this string to make more memory available\
this is a very long string only used to fill memory so I know when the memory is full and I can remove some of this string to make more memory available\
this is a very long string only used to fill memory so I know when the memory is full and I can remove some of this string to make more memory available\
this is a very long string only used to fill memory so I know when the memory is full and I can remove some of this string to make more memory available\
this is a very long string only used to fill memory so I know when the memory is full and I can remove some of this string to make more memory available\
this is a very long string only used to fill memory so I know when the memory is full and I can remove some of this string to make more memory available\
this is a very long string only used to fill memory so I know when the memory is full and I can remove some of this string to make more memory available\
this is a very long string only used to fill memory so I know when the memory is full and I can remove some of this string to make more memory available\
this is a very long string only used to fill memory so I know when the memory is full and I can remove some of this string to make more memory available\
this is a very long string only used to fill memory so I know when the memory is full and I can remove some of this string to make more memory available\
this is a very long string only used to fill memory so I know when the memory is full and I can remove some of this string to make more memory available"
;
#endif

void reset_settings(int m)
{
//  strcpy((char *)spi_buffer, dummy);
  setting.mode = m;
  setting.unit_scale_index = 0;
  setting.unit_scale = 1;
  setting.unit = U_DBM;
  set_scale(10);
  set_reflevel(-10);
  setting.attenuate = 0;
  setting.rbw = 0;
  setting.average = 0;
  setting.harmonic = 0;
  setting.show_stored = 0;
  setting.auto_attenuation = true;
  setting.subtract_stored = 0;
  setting.drive=13;
  setting.atten_step = 0;       // Only used in low output mode
  setting.agc = S_AUTO_ON;
  setting.lna = S_AUTO_OFF;
  setting.tracking = false;
  setting.modulation = MO_NONE;
  setting.step_delay = 0;
  setting.vbw = 0;
  setting.auto_reflevel = true;     // Must be after SetReflevel
  setting.decay=20;
  setting.noise=5;
  setting.below_IF = S_AUTO_OFF;
  setting.repeat = 1;
  setting.tracking_output = false;
  setting.measurement = M_OFF;
  setting.frequency_IF = 433800000;
  setting.auto_IF = true;
  setting.offset = 0.0;
  setting.trigger = T_AUTO;
  setting.level_sweep = 0.0;
  setting.level = -15.0;
  setting.trigger_level = -150.0;
  setting.linearity_step = 0;
  trace[TRACE_STORED].enabled = false;
  trace[TRACE_TEMP].enabled = false;
//  setting.refer = -1;             // do not reset reffer when switching modes
  setting.mute = true;
#ifdef __SPUR__
  setting.spur = 0;
#endif
  switch(m) {
  case M_LOW:
    minFreq = 0;
    maxFreq = 520000000;
    set_sweep_frequency(ST_START, (uint32_t) 0);
    set_sweep_frequency(ST_STOP, (uint32_t) 350000000);
    setting.attenuate = 30.0;
    setting.sweep_time_us = 0;
    break;
#ifdef __ULTRA__
  case M_ULTRA:
    minFreq = 674000000;
    maxFreq = 4300000000;
    set_sweep_frequency(ST_START, (uint32_t) minFreq);
    set_sweep_frequency(ST_STOP, (uint32_t) maxFreq);
    setting.attenuate = 0;
    setting.sweep_time_us = 0;
    break;
#endif
  case M_GENLOW:
    setting.drive=8;
    minFreq = 0;
    maxFreq = 520000000;
    set_sweep_frequency(ST_CENTER, 10000000);
    set_sweep_frequency(ST_SPAN, 0);
    setting.sweep_time_us = 10*ONE_SECOND_TIME;
    break;
  case M_HIGH:
#ifdef __ULTRA_SA__
    minFreq = 00000000;
    maxFreq = 2000000000;
#else
    minFreq = 24*setting_frequency_10mhz;
    maxFreq = 96*setting_frequency_10mhz;
#endif
    set_sweep_frequency(ST_START, minFreq);
    set_sweep_frequency(ST_STOP,  maxFreq);
    setting.sweep_time_us = 0;
    break;
  case M_GENHIGH:
    setting.drive=8;
    minFreq = 240000000;
    maxFreq = 960000000;
    set_sweep_frequency(ST_CENTER, 300000000);
    set_sweep_frequency(ST_SPAN, 0);
    setting.sweep_time_us = 10*ONE_SECOND_TIME;
    break;
  }
  for (int i = 0; i< MARKERS_MAX; i++) {
    markers[i].enabled = M_DISABLED;
    markers[i].mtype = M_NORMAL;
  }
  markers[0].mtype = M_REFERENCE | M_TRACKING;
  markers[0].enabled = M_ENABLED;

  dirty = true;
}

uint32_t calc_min_sweep_time_us(void)         // Calculate minimum sweep time in uS
{
  float t;
  float a = (actualStepDelay + MEASURE_TIME); // in uS
  if (MODE_OUTPUT(setting.mode))
    t = 100;
  else {
    if (FREQ_IS_CW()) {
      a = MINIMUM_SWEEP_TIME / (sweep_points - 1);       // time per step in CW mode
      if (setting.repeat != 1 || setting.sweep_time_us >= ONE_SECOND_TIME || setting.spur != 0)
        a = 15000.0 / (sweep_points - 1);       // time per step in CW mode with repeat
    }
    t = vbwSteps * (sweep_points - 1) * (setting.spur ? 2 : 1) * ( (a + (setting.repeat - 1)* REPEAT_TIME));
  }
  return t;
}


void set_refer_output(int v)
{
  setting.refer = v;
  dirty = true;
}

void set_decay(int d)
{
  if (d < 0 || d > 200)
    return;
  setting.decay = d;
  dirty = true;
}

void set_noise(int d)
{
  if (d < 2 || d > 50)
    return;
  setting.noise = d;
  dirty = true;
}

void set_measurement(int m)
{
  setting.measurement = m;
  if (m == M_LINEARITY) {
    trace[TRACE_STORED].enabled = true;
    for (int j = 0; j < setting._sweep_points; j++)
      stored_t[j] = -150;
    setting.linearity_step = 0;
    setting.attenuate = 29.0;
    setting.auto_attenuation = false;
  }
  dirty = true;
}
void set_drive(int d)
{
  setting.drive = d;
  dirty = true;
}

void set_level_sweep(float l)
{
  setting.level_sweep = l;
  dirty = true;
}

void set_sweep_time_us(uint32_t t)
{
  if (t < MINIMUM_SWEEP_TIME)
    t = MINIMUM_SWEEP_TIME;
  if (t > MAXIMUM_SWEEP_TIME)
    t = MAXIMUM_SWEEP_TIME;
  setting.sweep_time_us = t;
  uint32_t ta = calc_min_sweep_time_us();
  if (ta < t)
    ta = t;
  setting.actual_sweep_time_us = ta;
  if (FREQ_IS_CW())
    update_grid();            // Really only needed in zero span mode
  redraw_request |= REDRAW_FREQUENCY;
  dirty = true;
}

void set_tracking_output(int t)
{
  setting.tracking_output = t;
  dirty = true;
}

void toggle_tracking_output(void)
{
  setting.tracking_output = !setting.tracking_output;
  dirty = true;
}

void toggle_mute(void)
{
  setting.mute = !setting.mute;
  dirty = true;
}

void toggle_below_IF(void)
{
  if (S_IS_AUTO(setting.below_IF ))
    setting.below_IF = false;
  else if (setting.below_IF)
    setting.below_IF = S_AUTO_OFF;
  else
    setting.below_IF = true;
  dirty = true;
}

void set_modulation(int m)
{
  setting.modulation = m;
  dirty = true;
}

void set_repeat(int r)
{
  if (r > 0 && r <= 100) {
    setting.repeat = r;
    dirty = true;
  }
}

void set_IF(int f)
{
  if (f == 0)
    setting.auto_IF = true;
  setting.frequency_IF = f;
  dirty = true;
}

#define POWER_STEP  0           // Should be 5 dB but appearently it is lower
#define POWER_OFFSET    15
#define SWITCH_ATTENUATION  30
#define RECEIVE_SWITCH_ATTENUATION  21


void set_auto_attenuation(void)
{
  setting.auto_attenuation = true;
  if (setting.mode == M_LOW) {
    setting.attenuate = 30.0;
  } else {
    setting.attenuate = 0;
  }
  setting.atten_step = false;
  dirty = true;
}

void set_auto_reflevel(int v)
{
  setting.auto_reflevel = v;
}

float get_attenuation(void)
{
  if (setting.mode == M_GENLOW) {
    if (setting.atten_step)
      return ( -(POWER_OFFSET + setting.attenuate - (setting.atten_step-1)*POWER_STEP + SWITCH_ATTENUATION));
    else
      return ( -POWER_OFFSET - setting.attenuate + (setting.drive & 7) * 3);
  } else if (setting.atten_step) {
    if (setting.mode == M_LOW)
      return setting.attenuate + RECEIVE_SWITCH_ATTENUATION;
    else
      return setting.attenuate + SWITCH_ATTENUATION;
  }
  return(setting.attenuate);
}

static const int drive_dBm [16] = {-38,-35,-33,-30,-27,-24,-21,-19,-7,-4,-2, 1, 4, 7, 10, 13};

void set_level(float v)
{
  if (setting.mode == M_GENHIGH) {
    int d = 0;
    while (drive_dBm[d] < v - 1 && d < 16)
      d++;
    if (d == 8 && v < -12)  // Round towards closest level
      d = 7;
    set_drive(d);
  } else {
    setting.level = v;
    set_attenuation((int)v);
  }
  dirty = true;
}

void set_attenuation(float a)
{
  if (setting.mode == M_GENLOW) {
    a = a + POWER_OFFSET;
    if (a > 6) {                // +9dB
      setting.drive = 11;   // Maximum save drive for SAW filters.
      a = a - 9;
    } else if (a > 3) {         // +6dB
      setting.drive = 10;
      a = a - 6;
    } else if (a > 0) {         // +3dB
      setting.drive = 9;
      a = a - 3;
    } else
      setting.drive = 8;        // defined as 0dB level
    if (a > 0)
      a = 0;
    if( a >  - SWITCH_ATTENUATION) {
      setting.atten_step = 0;
    } else {
      a = a + SWITCH_ATTENUATION;
      setting.atten_step = 1;
    }
    a = -a;
  } else {
    if (setting.mode == M_LOW && a > 31) {
      setting.atten_step = 1;
      a = a - RECEIVE_SWITCH_ATTENUATION;
    } else if (setting.mode == M_HIGH && a > 0) {
      setting.atten_step = 1;
      a = a - SWITCH_ATTENUATION;
    } else
      setting.atten_step = 0;
    setting.auto_attenuation = false;
  }
  if (a<0.0)
      a = 0;
  if (a> 31)
    a=31.0;
  if (setting.mode == M_HIGH)   // No attenuator in high mode
    a = 0;
//  if (setting.attenuate == a)
//    return;
  setting.attenuate = a;
  dirty = true;
}

void set_storage(void)
{
  for (int i=0; i<POINTS_COUNT;i++)
    stored_t[i] = actual_t[i];
  setting.show_stored = true;
  trace[TRACE_STORED].enabled = true;
  dirty = true;
}

void set_clear_storage(void)
{
  setting.show_stored = false;
  setting.subtract_stored = false;
  trace[TRACE_STORED].enabled = false;
  dirty = true;
}

void set_subtract_storage(void)
{
  if (!setting.subtract_stored) {
    if (!setting.show_stored)
      set_storage();
    setting.subtract_stored = true;
//    setting.auto_attenuation = false;
  } else {
    setting.subtract_stored = false;
  }
  dirty = true;
}


void toggle_normalize(void)
{
  if (!setting.subtract_stored) {
    for (int i=0; i<POINTS_COUNT;i++)
      stored_t[i] = actual_t[i];
    setting.subtract_stored = true;
    setting.auto_attenuation = false;       // Otherwise noise level may move leading to strange measurements
  } else {
    setting.subtract_stored = false;
  }
  dirty = true;
}


extern float peakLevel;
void set_actual_power(float o)
{
  float new_offset = o - peakLevel + get_level_offset();
  if (o == 100) new_offset = 0;
  if (setting.mode == M_HIGH) {
    config.high_level_offset = new_offset;
  } else if (setting.mode == M_LOW) {
    config.low_level_offset = new_offset;
#ifdef __ULTRA__
  } else if (setting.mode == M_ULTRA) {
    config.low_level_offset = new_offset;
#endif
  }
  dirty = true;
}

int get_level_offset(void)
{
  if (setting.mode == M_HIGH) {
    if (config.high_level_offset == 100)
      return 0;
    return(config.high_level_offset);
  }
  if (setting.mode == M_LOW) {
    if (config.low_level_offset == 100)
      return 0;
    return(config.low_level_offset);
  }
  return(0);
}

int level_is_calibrated(void)
{
  if (setting.mode == M_HIGH && config.high_level_offset != 100)
    return 1;
  if (setting.mode == M_LOW && config.low_level_offset != 100)
    return 1;
  return(0);
}

void set_RBW(int v)
{
  setting.rbw = v;
  update_rbw();
  dirty = true;
}

#ifdef __SPUR__
void set_spur(int v)
{
  if (setting.mode!=M_LOW)
    return;
  setting.spur = v;
//  if (setting.spur && actual_rbw > 360)           // moved to update_rbw
//    set_RBW(300);
  dirty = true;
}
#endif

#ifdef __ULTRA__
void set_harmonic(int h)
{
  setting.harmonic = h;
  minFreq = 684000000.0;
  if ((uint32_t)(setting.harmonic * 240000000)+434000000 >  minFreq)
    minFreq = setting.harmonic * 240000000.0+434000000.0;
  maxFreq = 4360000000;
  if (setting.harmonic != 0 && (960000000.0 * setting.harmonic + 434000000.0 )< 4360000000.0)
    maxFreq = (960000000.0 * setting.harmonic + 434000000.0 );
  set_sweep_frequency(ST_START, minFreq);
  set_sweep_frequency(ST_STOP, maxFreq);
}
#endif

void set_step_delay(int d)
{

  if ((3 <= d && d < 300) || d > 30000)
    return;
  setting.step_delay = d;
  dirty = true;
}

void set_average(int v)
{
  setting.average = v;
  trace[TRACE_TEMP].enabled = (v != 0);
  dirty = true;
}

void toggle_LNA(void)
{
  if (S_IS_AUTO(setting.lna ))
    setting.lna = false;
  else if (setting.lna)
    setting.lna = S_AUTO_OFF;
  else
    setting.lna = true;
  dirty = true;
}

void toggle_tracking(void)
{
  setting.tracking = !setting.tracking;
  if (setting.tracking) {
    set_refer_output(2);
    set_sweep_frequency(ST_CENTER, 10000000);
    set_sweep_frequency(ST_SPAN,    5000000);
  } else {
    set_refer_output(-1);
  }
  dirty = true;
}

void toggle_AGC(void)
{
  if (S_IS_AUTO(setting.agc ))
    setting.agc = false;
  else if (setting.agc)
    setting.agc = S_AUTO_ON;
  else
    setting.agc = true;
  dirty = true;
}

void set_unit(int u)
{
  if (setting.unit == u)
    return;
  float r = to_dBm(setting.reflevel);   // Get neutral unit
  float s = to_dBm(setting.scale);
//  float t = setting.trigger;            // Is always in dBm
  // float m = r - NGRIDSY*s;

  setting.unit = u;                     // Switch unit

  r = value(r);                         // Convert to target unit
  s = value(s);
  if (UNIT_IS_LINEAR(setting.unit)) {
    if (r < REFLEVEL_MIN)
      r = REFLEVEL_MIN;                          // Minimum value to ensure display
    if (r >REFLEVEL_MAX)
      r = REFLEVEL_MAX;                          // Maximum value
    set_scale(r/NGRIDY);
    set_reflevel(setting.scale*NGRIDY);
    if (S_IS_AUTO(setting.agc))
      setting.agc = S_AUTO_ON;
    if (S_IS_AUTO(setting.lna))
      setting.lna = S_AUTO_OFF;
  } else {
    r = 10 * round((r*1.2)/10.0);
    set_reflevel(r);
    set_scale(10);
    if (S_IS_AUTO(setting.agc))
      setting.agc = S_AUTO_ON;
    if (S_IS_AUTO(setting.lna))
      setting.lna = S_AUTO_OFF;
  }
  plot_into_index(measured);
  force_set_markmap();
  dirty = true;
}
float const unit_scale_value[]={1,0.001,0.000001,0.000000001,0.000000000001};
const char * const unit_scale_text[]= {"","m", "u",     "n",        "p"};

void user_set_reflevel(float level)
{
  set_auto_reflevel(false);
  if (UNIT_IS_LINEAR(setting.unit) && level < setting.scale*NGRIDY) {
    set_scale(level/NGRIDY);
    set_reflevel(setting.scale*NGRIDY);
  } else
    set_reflevel(level);
}

void set_reflevel(float level)
{

  if (UNIT_IS_LINEAR(setting.unit)) {
    if (level < REFLEVEL_MIN)
      level = REFLEVEL_MIN;
    if (level > REFLEVEL_MAX)
      level = REFLEVEL_MAX;
  }

  setting.unit_scale_index = 0;
  setting.unit_scale = 1.0;
  while (UNIT_IS_LINEAR(setting.unit) && setting.unit_scale_index < sizeof(unit_scale_value)/sizeof(float) - 1) {
    if (level > unit_scale_value[setting.unit_scale_index])
      break;
    setting.unit_scale_index++;
  }
  setting.unit_scale = unit_scale_value[setting.unit_scale_index];

  setting.reflevel = level;
  set_trace_refpos(0, /* NGRIDY - */ level /* / get_trace_scale(0) */);
  set_trace_refpos(1, /* NGRIDY - */ level /* / get_trace_scale(0) */ );
  set_trace_refpos(2, /* NGRIDY - */ level /* / get_trace_scale(0) */ );
  redraw_request |= REDRAW_AREA | REDRAW_CAL_STATUS;
//  dirty = true;
}

void round_reflevel_to_scale(void) {
  int multi = floor((setting.reflevel + setting.scale/2)/setting.scale);
  if (UNIT_IS_LINEAR(setting.unit)) {
    if (multi < NGRIDY) {
      setting.reflevel = setting.scale*10;  // Never negative bottom
    }
  } else {

  }
  setting.reflevel = multi*setting.scale;
  set_trace_refpos(0,setting.reflevel);
  set_trace_refpos(1,setting.reflevel);
  set_trace_refpos(2,setting.reflevel);
}

void user_set_scale(float s)
{
  if (UNIT_IS_LINEAR(setting.unit))
    set_auto_reflevel(false);
  set_scale(s);
  if (UNIT_IS_LINEAR(setting.unit) && setting.reflevel < setting.scale*NGRIDY)
    set_reflevel(setting.scale*NGRIDY);
}

void set_scale(float t) {
  if (UNIT_IS_LINEAR(setting.unit)) {
    if (t < REFLEVEL_MIN/10.0)
      t = REFLEVEL_MIN/10.0;
    if (t > REFLEVEL_MAX/10.0)
      t = REFLEVEL_MAX/10.0;
  } else {
    if (t > 20.0)
      t = 20.0;
    else if (t < 1)
      t = 1.0;
  }

  float m = 1;
//        t = t * 1.2;
  while (t > 10) { m *= 10; t/=10; }
  while (t < 1.0)  { m /= 10; t*=10; }
  if (t>5.0001)
    t = 10.0;
  else if (t>2.0001)
    t = 5.0;
  else if (t > 1.0001)
    t = 2.0;
  else
    t = 1.0;
  t = t*m;
  setting.scale = t;
  set_trace_scale(0, t);
  set_trace_scale(1, t);
  set_trace_scale(2, t);
  round_reflevel_to_scale();
  redraw_request |= REDRAW_AREA | REDRAW_CAL_STATUS;
}


void set_offset(float offset)
{
  setting.offset = offset;
  dirty = true;
}

void show_stored_trace_at(float v)
{
  for (int j = 0; j < setting._sweep_points; j++)
    stored_t[j] = v;
  trace[TRACE_STORED].enabled = true;
}

void set_trigger_level(float trigger_level)
{
  setting.trigger_level = trigger_level;
  if (setting.trigger != T_AUTO) {
    show_stored_trace_at(setting.trigger_level);
  }
  dirty = true;
}

void set_trigger(int trigger)
{
  setting.trigger = trigger;
  if (trigger == T_AUTO) {
    trace[TRACE_STORED].enabled = false;
  } else {
    show_stored_trace_at(setting.trigger_level);
  }
  sweep_mode = SWEEP_ENABLE;
  dirty = true;
}


//int GetRefpos(void) {
//  return (NGRIDY - get_trace_refpos(2)) * get_trace_scale(2);
//}

//int GetScale(void) {
//  return get_trace_refpos(2);
//}
void set_mode(int m)
{
#ifdef __ULTRA__
  if (m == 6)
    m = M_ULTRA;
#endif
  dirty = true;
  if (setting.mode == m)
    return;
  reset_settings(m);
//  dirty = true;
}

void apply_settings(void)
{
  set_switches(setting.mode);
  if (setting.mode == M_HIGH)
    PE4302_Write_Byte(40);  // Ensure defined input impedance of low port when using high input mode (power calibration)
  else
    PE4302_Write_Byte((int)(setting.attenuate * 2));
  if (setting.mode == M_LOW) {

  }
  SI4432_SetReference(setting.refer);
  update_rbw();
  if (setting.frequency_step == 0.0) {
    if (setting.step_delay <= 2)
      actualStepDelay = 0;
    else
      actualStepDelay = setting.step_delay;
  } else if (setting.step_delay <= 2){
    if (actual_rbw >= 191.0)        actualStepDelay =  280;
    else if (actual_rbw >= 142.0)   actualStepDelay =  350;
    else if (actual_rbw >= 75.0)    actualStepDelay =  450;
    else if (actual_rbw >= 56.0)    actualStepDelay =  650;
    else if (actual_rbw >= 37.0)    actualStepDelay =  700;
    else if (actual_rbw >= 18.0)    actualStepDelay = 1100;
    else if (actual_rbw >=  9.0)    actualStepDelay = 1700;
    else if (actual_rbw >=  5.0)    actualStepDelay = 3300;
    else                           actualStepDelay = 6400;
    if (setting.step_delay == 1)
      actualStepDelay *= 2;
  } else
    actualStepDelay = setting.step_delay;
}

//------------------------------------------
#if 0
#define CORRECTION_POINTS  10

static const uint32_t correction_frequency[CORRECTION_POINTS] =
{ 100000, 200000, 400000, 1000000, 2000000, 50000000, 100000000, 200000000, 300000000, 350000000 };

static const float correction_value[CORRECTION_POINTS] =
{ +4.0, +2.0, +1.5, +0.5, 0.0, 0.0, +1.0, +1.0, +2.5, +5.0 };
#endif

float get_frequency_correction(uint32_t f)
{
  if (!(setting.mode == M_LOW))
    return(0.0);
  int i = 0;
  while (f > config.correction_frequency[i] && i < CORRECTION_POINTS)
    i++;
  if (i >= CORRECTION_POINTS)
    return(config.correction_value[CORRECTION_POINTS-1]);
  if (i == 0)
    return(config.correction_value[0]);
  f = f - config.correction_frequency[i-1];
  uint32_t m = config.correction_frequency[i] - config.correction_frequency[i-1] ;
  float cv = config.correction_value[i-1] + (config.correction_value[i] - config.correction_value[i-1]) * (float)f / (float)m;
  return(cv);
}


float peakLevel;
float min_level;
uint32_t peakFreq;
int peakIndex;
float temppeakLevel;
int temppeakIndex;
static unsigned long old_freq[4] = { 0, 0, 0, 0 };
static unsigned long real_old_freq[4] = { 0, 0, 0, 0 };


void setupSA(void)
{
  SI4432_Init();
  old_freq[0] = 0;
  old_freq[1] = 0;
  real_old_freq[0] = 0;
  real_old_freq[1] = 0;
  SI4432_Sel = 0;
  SI4432_Receive();

  SI4432_Sel = 1;
  SI4432_Transmit(0);
  PE4302_init();
  PE4302_Write_Byte(0);
}
extern int SI4432_frequency_changed;
extern int SI4432_offset_changed;

void set_freq(int V, unsigned long freq)
{
  if (old_freq[V] != freq) {
    if (V <= 1) {
      SI4432_Sel = V;
      if (freq < 240000000 || freq > 960000000) {
        real_old_freq[V] = freq + 1;
        return;
      }
#if 1
      if (setting.step_delay == 2) {
        int delta =  freq - real_old_freq[V];

        if (real_old_freq[V] >= 480000000)    // 480MHz, high band
          delta = delta >> 1;
        if (delta > 0 && delta < 80000) { // use frequency deviation registers
          if (0) {
            if (real_old_freq[V] >= 480000000)
              shell_printf("%d: Offs %q HW %d\r\n", SI4432_Sel, (uint32_t)(real_old_freq[V]+delta*2),  real_old_freq[V]);
            else
              shell_printf("%d: Offs %q HW %d\r\n", SI4432_Sel, (uint32_t)(real_old_freq[V]+delta*1),  real_old_freq[V]);
          }

          delta = delta * 4 / 625; // = 156.25;
          SI4432_Write_Byte(0x73, (uint8_t)(delta & 0xff));
          uint8_t reg = delta >> 8;
          SI4432_Write_Byte(0x74, (uint8_t)((delta >> 8) & 0x03));
          SI4432_offset_changed = true;
          old_freq[V] = freq;
          return;
        }
      }
#endif
      SI4432_Set_Frequency(freq);
      SI4432_Write_Byte(0x73, 0);
      SI4432_Write_Byte(0x74, 0);
#ifdef __ULTRA_SA__
    } else {
      ADF4351_set_frequency(V-2,freq,3);
#endif
    }
    old_freq[V] = freq;
    real_old_freq[V] = freq;
  }
}

void set_switch_transmit(void) {
  SI4432_Write_Byte(0x0b, 0x1f);// Set switch to transmit
  SI4432_Write_Byte(0x0c, 0x1d);
}

void set_switch_receive(void) {
  SI4432_Write_Byte(0x0b, 0x1d);// Set switch to receive
  SI4432_Write_Byte(0x0c, 0x1f);
}

void set_switch_off(void) {
  SI4432_Write_Byte(0x0b, 0x1d);// Set both switch off
  SI4432_Write_Byte(0x0c, 0x1f);
}

void set_AGC_LNA(void) {
  unsigned char v = 0x40;
  if (S_STATE(setting.agc)) v |= 0x20;
  if (S_STATE(setting.lna)) v |= 0x10;
  SI4432_Write_Byte(0x69, v);
}

void set_switches(int m)
{
  SI4432_Init();
  old_freq[0] = 0;
  old_freq[1] = 0;
  real_old_freq[0] = 0;
  real_old_freq[1] = 0;
  switch(m) {
case M_LOW:     // Mixed into 0
#ifdef __ULTRA__
case M_ULTRA:
#endif
    SI4432_Sel = 0;
    SI4432_Receive();
    if (setting.atten_step) {
      set_switch_transmit();
    } else {
      set_switch_receive();
    }
    set_AGC_LNA();

    SI4432_Sel = 1;
    if (setting.tracking_output)
      set_switch_transmit();
    else
      set_switch_off();
//    SI4432_Receive(); For noise testing only
    SI4432_Transmit(setting.drive);
    // SI4432_SetReference(setting.refer);
    break;
case M_HIGH:    // Direct into 1
mute:
    // SI4432_SetReference(-1); // Stop reference output
    SI4432_Sel = 0; // both as receiver to avoid spurs
    set_switch_receive();
    SI4432_Receive();

    SI4432_Sel = 1;
    SI4432_Receive();
    if (setting.atten_step) {
       set_switch_transmit();
     } else {
       set_switch_receive();
     }
    set_AGC_LNA();

    break;
case M_GENLOW:  // Mixed output from 0
    if (setting.mute)
      goto mute;
    SI4432_Sel = 0;
    if (setting.atten_step) {
      set_switch_off();
    } else {
      set_switch_transmit();
    }
    SI4432_Transmit(setting.drive);

    SI4432_Sel = 1;
    if (setting.modulation == MO_EXTERNAL) {
      set_switch_transmit();  // High input for external LO scuh as tracking output of other tinySA
      SI4432_Receive();
    } else {
      set_switch_off();
      SI4432_Transmit(12);                 // Fix LO drive a 10dBm
    }
    break;
case M_GENHIGH: // Direct output from 1
    if (setting.mute)
      goto mute;
    SI4432_Sel = 0;
    SI4432_Receive();
    set_switch_receive();

    SI4432_Sel = 1;
    if (setting.drive < 8) {
      set_switch_off();
    } else {
      set_switch_transmit();
    }
    SI4432_Transmit(setting.drive);

    break;
  }
  SI4432_Sel = 1;
  SI4432_Write_Byte(0x73, 0);  // Back to nominal offset
  SI4432_Write_Byte(0x74, 0);

}

void update_rbw(void)
{
  if (setting.frequency_step > 0 && MODE_INPUT(setting.mode)) {
    setting.vbw = (setting.frequency_step)/1000.0;
  } else {
    setting.vbw = 300; // trick to get right default rbw in zero span mode
  }
  actual_rbw = setting.rbw;
  if (actual_rbw == 0) {
    actual_rbw = 2*setting.vbw;
  }
  if (actual_rbw < 2.6)
    actual_rbw = 2.6;
  if (actual_rbw > 600)
    actual_rbw = 600;

  if (setting.spur && actual_rbw > 300)
    actual_rbw = 250;

  SI4432_Sel =  MODE_SELECT(setting.mode);
  actual_rbw = SI4432_SET_RBW(actual_rbw);

  if (setting.frequency_step > 0 && MODE_INPUT(setting.mode)) {
    if (setting.step_delay==1) // Precise
      vbwSteps = ((int)(2 * (setting.vbw + (actual_rbw/2)) / (actual_rbw / 2)));
    else
      vbwSteps = ((int)(2 * (setting.vbw + (actual_rbw/2)) / actual_rbw));

    if (vbwSteps < 1)
      vbwSteps = 1;
  } else {
    setting.vbw = actual_rbw;
    vbwSteps = 1;
  }
  dirty = true;
}

int binary_search_frequency(int f)
{
  int L = 0;
  int R =  (sizeof frequencies)/sizeof(int) - 1;
  int fmin =  f - ((int)actual_rbw ) * 1000;
  int fplus = f + ((int)actual_rbw ) * 1000;
  while (L <= R) {
    int m = (L + R) / 2;
    if ((int)frequencies[m] < fmin)
      L = m + 1;
    else if ((int)frequencies[m] > fplus)
      R = m - 1;
    else
       return m; // index is m
  }
  return -1;
}


#define MAX_MAX 4
int
search_maximum(int m, int center, int span)
{
  center = binary_search_frequency(center);
  if (center < 0)
    return false;
  int from = center - span/2;
  int found = false;
  int to = center + span/2;
  int cur_max = 0;          // Always at least one maximum
  int max_index[4];
  if (from<0)
    from = 0;
  if (to > setting._sweep_points-1)
    to = setting._sweep_points-1;
  temppeakIndex = 0;
  temppeakLevel = actual_t[from];
  max_index[cur_max] = from;
  int downslope = true;

  for (int i = from; i <= to; i++) {
    if (downslope) {
      if (temppeakLevel > actual_t[i]) {    // Follow down
        temppeakIndex = i;                  // Latest minimum
        temppeakLevel = actual_t[i];
      } else if (temppeakLevel + setting.noise < actual_t[i]) {    // Local minimum found
        temppeakIndex = i;                          // This is now the latest maximum
        temppeakLevel = actual_t[i];
        downslope = false;
      }
    } else {
      if (temppeakLevel < actual_t[i]) {    // Follow up
        temppeakIndex = i;
        temppeakLevel = actual_t[i];
      } else if (temppeakLevel - setting.noise > actual_t[i]) {    // Local max found

        found = true;
        int j = 0;                                            // Insertion index
        while (j<cur_max && actual_t[max_index[j]] >= temppeakLevel)   // Find where to insert
          j++;
        if (j < MAX_MAX) {                                    // Larger then one of the previous found
          int k = MAX_MAX-1;
          while (k > j) {                                      // Shift to make room for max
            max_index[k] = max_index[k-1];
            //              maxlevel_index[k] = maxlevel_index[k-1];        // Only for debugging
            k--;
          }
          max_index[j] = temppeakIndex;
          //            maxlevel_index[j] = actual_t[temppeakIndex];      // Only for debugging
          if (cur_max < MAX_MAX) {
            cur_max++;
          }
          //STOP_PROFILE
        }
        temppeakIndex = i;            // Latest minimum
        temppeakLevel = actual_t[i];

        downslope = true;
      }
    }
  }
  markers[m].index = max_index[0];
  return found;
}

//static int spur_old_stepdelay = 0;
static const unsigned int spur_IF =            433800000;
static const unsigned int spur_alternate_IF =  433900000;
static const int spur_table[] =
{
 580000,            // 433.8 MHz table
 961000,
 1600000,
 1837000,           // Real signal
 2755000,           // Real signal
 2760000,
 2961000,
 4933000,
 4960000,
 6961000,
 6980000,
 8267000,
 8961000,
 10000000,
 10960000,
 11600000,
 16960000,
 22960000,
 28960000,
 29800000,
 38105000,
 49500000,
#ifdef IF_AT_4339
  780000,           // 433.9MHz table
   830000,
   880000,
   949000,
  1390000,
  1468000,
  1830000,
  1900000,
  2770000,
  2840000,
  2880000,
  4710000,
  4780000,
  4800000,
  4880000,
  6510000,
  6750000,
  6790000,
  6860000,
  7340000,
  8100000,
  8200000,
  8880000,
//  9970000,    10MHz!!!!!!
 10870000,
 11420000,
 14880000,
 16820000,
#endif
};

int binary_search(int f)
{
  int L = 0;
  int R =  (sizeof spur_table)/sizeof(int) - 1;
  int fmin =  f - ((int)actual_rbw ) * 1000;
  int fplus = f + ((int)actual_rbw ) * 1000;
  while (L <= R) {
    int m = (L + R) / 2;
    if (spur_table[m] < fmin)
      L = m + 1;
    else if (spur_table[m] > fplus)
      R = m - 1;
    else
       return true; // index is m
  }
  return false;
}


int avoid_spur(int f)
{
//  int window = ((int)actual_rbw ) * 1000*2;
//  if (window < 50000)
//    window = 50000;
  if (! setting.mode == M_LOW || !setting.auto_IF || actual_rbw > 300.0)
    return(false);
  return binary_search(f);
}

static int modulation_counter = 0;

static const int am_modulation[5] =  { 4,0,1,5,7 };
static const int nfm_modulation[5] = { 0, 2, 1, -1, -2};
static const int wfm_modulation[5] = { 0, 190, 118, -118, -190 };

char age[POINTS_COUNT];

static float old_a = -150;

float perform(bool break_on_operation, int i, uint32_t f, int tracking)
{
  if (i == 0 && dirty ) {                                                        // SCan initiation
    apply_settings();
    scandirty = true;
    dirty = false;
    if (setting.spur)
      setting.spur = 1;         // resync spur in case of previous abort
  }

  if (setting.mode == M_GENLOW && setting.level_sweep != 0.0) {
    float ls=setting.level_sweep;
    if (ls > 0)
      ls += 0.5;
    else
      ls -= 0.5;
    float a = ((int)((setting.level + (i / 290.0) * ls)*2.0)) / 2.0;
    if (a != old_a) {
      old_a = a;
      int d = 0;              // Start at lowest drive level;
      a = a + POWER_OFFSET;
      if (a > 0) {
        d++;
        a = a - 3;
      }
      if (a > 0) {
        d++;
        a = a - 3;
      }
      if (a > 0) {
        d++;
        a = a - 3;
      }
      SI4432_Sel = 0;
      SI4432_Drive(d);
      if (a > 0)
        a = 0;

      if( a >  - SWITCH_ATTENUATION) {
        set_switch_transmit();
      } else {
        a = a + SWITCH_ATTENUATION;
        set_switch_receive();
      }
      if (a < -31)
        a = -31;
      a = -a;
      PE4302_Write_Byte((int)(a * 2) );
    }
  }

  if (setting.mode == M_LOW && S_IS_AUTO(setting.agc) && UNIT_IS_LOG(setting.unit)) {
    unsigned char v;
    static unsigned char old_v;
    if (f < 500000)
      v = 0x50; // Disable AGC and enable LNA
    else
      v = 0x60; // Enable AGC and disable LNA
    if (old_v != v) {
      SI4432_Write_Byte(0x69, v);
      old_v = v;
    }
  }
  if (MODE_OUTPUT(setting.mode) && (setting.modulation == MO_AM_1kHz||setting.modulation == MO_AM_10Hz)) {               // AM modulation
    int p = setting.attenuate * 2 + am_modulation[modulation_counter];
    if (p>63)
      p = 63;
    if (p<0)
      p = 0;
    PE4302_Write_Byte(p);
    if (modulation_counter == 4) {  // 3dB modulation depth
      modulation_counter = 0;
    } else {
      modulation_counter++;
    }
    if (setting.modulation == MO_AM_10Hz)
      my_microsecond_delay(20000);
    else
      my_microsecond_delay(180);
//    chThdSleepMicroseconds(200);

  } else if (MODE_OUTPUT(setting.mode) && (setting.modulation == MO_NFM || setting.modulation == MO_WFM )) { //FM modulation
      SI4432_Sel = 1;
      int offset;
      if (setting.modulation == MO_NFM ) {
        offset = nfm_modulation[modulation_counter] ;
        SI4432_Write_Byte(0x73, (offset & 0xff ));  // Use frequency hopping channel for FM modulation
        SI4432_Write_Byte(0x74, ((offset >> 8) & 0x03 ));  // Use frequency hopping channel for FM modulation
      }
      else {
        offset = wfm_modulation[modulation_counter] ;
        SI4432_Write_Byte(0x73, (offset & 0xff ));  // Use frequency hopping channel for FM modulation
        SI4432_Write_Byte(0x74, ((offset >> 8) & 0x03 ));  // Use frequency hopping channel for FM modulation
      }
      if (modulation_counter == 4)
        modulation_counter = 0;
      else
        modulation_counter++;
      my_microsecond_delay(200);
//      chThdSleepMicroseconds(200);
  }

  float RSSI = -150.0;
  int t = 0;
  do {           // ------------- Acquisition loop ----------
    int offs = 0,sm;
    uint32_t lf = (uint32_t)f;
    if (vbwSteps > 1) {         // Calculate sub steps
      if (setting.step_delay == 1)
        sm = 250; // steps of a quarter rbw
      else
        sm = 500; // steps of half the rbw
      if (vbwSteps & 1) { // Uneven steps, center
        offs = (t - (vbwSteps >> 1)) * sm;
      } else {            // Even, shift half step
        offs = (t - (vbwSteps >> 1)) * sm + sm/2;
      }
      offs = (int)(offs * actual_rbw);
      lf = (uint32_t)(f + offs);
    }



    // --------------- Set all the LO's ------------------------
#ifdef __SPUR__
    float spur_RSSI = 0;
#endif


    if (MODE_INPUT(setting.mode) && i > 0 && FREQ_IS_CW())
      goto skip_LO_setting;                                                 // No LO changes during CW loop

    long local_IF;
    again:
    if (MODE_HIGH(setting.mode))
      local_IF = 0;
    else {
      if (setting.auto_IF) {
        if (setting.spur)
          local_IF = 433900000;
        else
          local_IF = 433800000;
      }
      else
        local_IF = setting.frequency_IF;
    }
    if (setting.mode == M_LOW && tracking) {                                // Measure BPF
      set_freq (0, local_IF + lf - reffer_freq[setting.refer]);    // Offset so fundamental of reffer is visible
    } else if (MODE_LOW(setting.mode)) {
      if (setting.mode == M_LOW && !in_selftest && avoid_spur(f)) {
        local_IF = spur_alternate_IF;
#ifdef __SPUR__
      } else if (setting.mode== M_LOW && setting.spur){
        if (S_IS_AUTO(setting.below_IF) && lf < 150000000) // if below 150MHz and auto_below_IF swap IF
        {              // else low/above IF
          if (setting.spur == 1)
            setting.below_IF = S_AUTO_ON;
          else
            setting.below_IF = S_AUTO_OFF;
        }
        else {
          int32_t spur_offset = actual_rbw * 1000;
          if (setting.spur == -1)
            spur_offset = - spur_offset;
          local_IF  = local_IF + spur_offset;
        }
#endif
      } else {
//        local_IF = setting.frequency_IF ;
      }
      if (setting.mode == M_GENLOW && setting.modulation == MO_EXTERNAL)    // LO input via high port
        local_IF += lf;
      set_freq (0, local_IF);
#ifdef __ULTRA__
    } else if (setting.mode == M_ULTRA) {               // No above/below IF mode in Ultra
      local_IF  = setting.frequency_IF + (int)(actual_rbw < 350.0 ? setting.spur*300000 : 0 );
      set_freq (0, local_IF);
 //     local_IF  = setting.frequency_IF + (int)(actual_rbw < 300.0?setting.spur * 1000 * actual_rbw:0);
#endif
    } else          // This must be high mode
      local_IF= 0;
#ifdef __ULTRA__
    if (setting.mode == M_ULTRA) {      // Set LO to correct harmonic in Ultra mode
//      if (lf > 3406000000 )
//        setFreq (1, local_IF/5 + lf/5);
//      else
      if (setting.spur != 1) {  // Left of tables
        if (lf > 3250000000 )
          set_freq (1, lf/5 - local_IF/5);
        if (lf > 1250000000 )
          set_freq (1, lf/3 - local_IF/3);
        else
          set_freq (1,  lf - local_IF);

      } else {              // Right of tables
        if (lf >= 2350000000)
          set_freq (1,  lf/5 + local_IF/5);
        else
          set_freq (1, lf/3 + local_IF/3);
      }
    } else
#endif
    {                                           // Else set LO ('s)
#ifdef __ULTRA_SA__
//#define IF_1    2550000000
#define IF_2    2025000000                      // First IF in Ultra SA mode

       set_freq (2, IF_2 + lf);                 // Scanning LO up to IF2
       set_freq (3, IF_2 - 433800000);          // Down from IF2 to fixed second IF in Ultra SA mode
       set_freq (1, 433800000);                 // Second IF fixe in Ultra SA mode
#else
       if (setting.mode == M_LOW && !setting.tracking && S_STATE(setting.below_IF))
         set_freq (1, local_IF-lf);
       else
         set_freq (1, local_IF+lf);
#endif
    }
    if (MODE_OUTPUT(setting.mode))              // No substepping and no RSSI in output mode
      return(0);

    // ---------------- Prepare RSSI ----------------------

    float signal_path_loss;

 skip_LO_setting:
#ifdef __FAST_SWEEP__
    if (i == 0 && setting.frequency_step == 0 && setting.trigger == T_AUTO && setting.spur == 0 && actualStepDelay == 0 && setting.repeat == 1 && setting.sweep_time_us < ONE_SECOND_TIME) {
      SI4432_Fill(MODE_SELECT(setting.mode), 0);
    }
#endif

#ifdef __ULTRA__
    if (setting.mode == M_ULTRA)
      signal_path_loss = -15;      // Loss in dB, -9.5 for v0.1, -12.5 for v0.2
    else
#endif
    if (setting.mode == M_LOW)
      signal_path_loss = -5.5;      // Loss in dB, -9.5 for v0.1, -12.5 for v0.2
    else
      signal_path_loss = +7;         // Loss in dB (+ is gain)

    int wait_for_trigger = false;
    int old_actual_step_delay = actualStepDelay;
    if (i == 0 && setting.frequency_step == 0 && setting.trigger != T_AUTO) { // prepare for wait for trigger to happen
      wait_for_trigger = true;
      actualStepDelay = 0;      // fastest possible in zero span trigger mode
    }
    float subRSSI;

    static float correct_RSSI;
    if (i == 0 || setting.frequency_step != 0 ) // only cases where the value can change
      correct_RSSI = get_level_offset()+ get_attenuation() - signal_path_loss - setting.offset + get_frequency_correction(f);
   wait:
    subRSSI = SI4432_RSSI(lf, MODE_SELECT(setting.mode)) + correct_RSSI ;
//    if ( i < 3)
//      shell_printf("%d %.3f %.3f %.1f\r\n", i, local_IF/1000000.0, lf/1000000.0, subRSSI);

    if (wait_for_trigger) { // wait for trigger to happen
      if ((operation_requested || shell_function) && break_on_operation)
        break;         // abort
      if (subRSSI < setting.trigger_level)
        goto wait;
#ifdef __FAST_SWEEP__
        if (i == 0 && setting.frequency_step == 0 /* && setting.trigger == T_AUTO */&& setting.spur == 0 && old_actual_step_delay == 0 && setting.repeat == 1 && setting.sweep_time_us < ONE_SECOND_TIME) {
           SI4432_Fill(MODE_SELECT(setting.mode), 1);
        }
#endif
      actualStepDelay = old_actual_step_delay; // Trigger happened, restore step delay
      if (setting.trigger == T_SINGLE)
        pause_sweep();                    // Trigger once so pause after this sweep has completed!!!!!!!
    }

#ifdef __SPUR__
    if (setting.spur == 1) {                                     // If first spur pass
      spur_RSSI = subRSSI;
      setting.spur = -1;
      goto again;                                                // Skip all other processing
    } else if (setting.spur == -1) {                            // If second  spur pass
      subRSSI = ( subRSSI < spur_RSSI ? subRSSI : spur_RSSI);  // Take minimum of two
      setting.spur = 1;
    }
#endif

    if (RSSI < subRSSI)                                     // Take max during subscanning
      RSSI = subRSSI;
    t++;
    if ((operation_requested || shell_function ) && break_on_operation)       // break subscanning if requested
      break;         // abort
  } while (t < vbwSteps);
  return(RSSI);
}

#define MAX_MAX 4
int16_t max_index[MAX_MAX];
int16_t cur_max = 0;

static int low_count = 0;


// main loop for measurement
static bool sweep(bool break_on_operation)
{
  float RSSI;
  int16_t downslope;
//  if (setting.mode== -1)
//    return;
//  START_PROFILE;
again:
  downslope = true;
  palClearPad(GPIOB, GPIOB_LED);
  temppeakLevel = -150;
  float temp_min_level = 100;
  //  spur_old_stepdelay = 0;
  int repeats = 1;
//  shell_printf("\r\n");
  if (MODE_OUTPUT(setting.mode) && setting.modulation != MO_NONE) {
    repeats = 1000; // to avoid interrupting the tone during UI processing
    modulation_counter = 0;
  }
  uint32_t t = calc_min_sweep_time_us(); // Time to delay in uS
  if (t < setting.sweep_time_us){
    t = setting.sweep_time_us - t;
    t = t / (sweep_points - 1);                   // Now in uS per point
  }
  else
    t = 0;
  if (MODE_OUTPUT(setting.mode) && t < 500)     // Minimum wait time to prevent LO from lockup
    t = 500;
  while (repeats--) {
  for (int i = 0; i < sweep_points; i++) {

    RSSI = perform(break_on_operation, i, frequencies[i], setting.tracking);
    if (t && (MODE_INPUT(setting.mode) || setting.modulation == MO_NONE)) {
      if (t < 30*ONE_MS_TIME)
        my_microsecond_delay(t);
      else
        osalThreadSleepMilliseconds(t / ONE_MS_TIME);
    }

    // back to toplevel to handle ui operation
    if ((operation_requested || shell_function) && break_on_operation) {
      if (setting.actual_sweep_time_us > ONE_SECOND_TIME) {
        ili9341_fill(OFFSETX, HEIGHT_NOSCROLL+1, WIDTH, 1, 0);
      }

      return false;
    }
    if (MODE_OUTPUT(setting.mode)) {
      continue;             // Skip all other processing
    }

    if (MODE_INPUT(setting.mode)) {
      if (setting.actual_sweep_time_us > ONE_SECOND_TIME && (i & 0x07) == 0) {
        ili9341_fill(OFFSETX, HEIGHT_NOSCROLL+1, i, 1, BRIGHT_COLOR_GREEN);
        ili9341_fill(OFFSETX+i, HEIGHT_NOSCROLL+1, WIDTH-i, 1, 0);
      }

      if (setting.average != AV_OFF)
          temp_t[i] = RSSI;
      if (setting.subtract_stored) {
        RSSI = RSSI - stored_t[i] ;
      }
//         stored_t[i] = (SI4432_Read_Byte(0x69) & 0x0f) * 3.0 - 90.0; // Display the AGC value in the stored trace
      if (scandirty || setting.average == AV_OFF) {             // Level calculations
        actual_t[i] = RSSI;
        age[i] = 0;
      } else {
        switch(setting.average) {
        case AV_MIN:      if (actual_t[i] > RSSI) actual_t[i] = RSSI; break;
        case AV_MAX_HOLD: if (actual_t[i] < RSSI) actual_t[i] = RSSI; break;
        case AV_MAX_DECAY:
          if (actual_t[i] < RSSI) {
            actual_t[i] = RSSI;
            age[i] = 0;
          } else {
            if (age[i] > setting.decay)
              actual_t[i] -= 0.5;
            else
              age[i] += 1;
          }
          break;
        case AV_4:  actual_t[i] = (actual_t[i]*3 + RSSI) / 4.0; break;
        case AV_16: actual_t[i] = (actual_t[i]*15 + RSSI) / 16.0; break;
        }
      }
#if 1

      // START_PROFILE
      if (i == 0) {                                          // Prepare peak finding
        cur_max = 0;          // Always at least one maximum
        temppeakIndex = 0;
        temppeakLevel = actual_t[i];
        max_index[0] = 0;
        downslope = true;
      }
      if (downslope) {                               // If in down slope peak finding
        if (temppeakLevel > actual_t[i]) {           // Follow down
          temppeakIndex = i;                         // Latest minimum
          temppeakLevel = actual_t[i];
        } else if (temppeakLevel + setting.noise < actual_t[i] ) {    // Local minimum found
          temppeakIndex = i;                         // This is now the latest maximum
          temppeakLevel = actual_t[i];
          downslope = false;
        }
      } else {                                      // up slope peak finding
        if (temppeakLevel < actual_t[i]) {    // Follow up
          temppeakIndex = i;
          temppeakLevel = actual_t[i];
        } else if (actual_t[i] < temppeakLevel - setting.noise) {    // Local max found


          int j = 0;                                            // Insert max in sorted table
          while (j<cur_max && actual_t[max_index[j]] >= temppeakLevel)   // Find where to insert
            j++;
          if (j < MAX_MAX) {                                    // Larger then one of the previous found
            int k = MAX_MAX-1;
            while (k > j) {                                      // Shift to make room for max
              max_index[k] = max_index[k-1];
              //              maxlevel_index[k] = maxlevel_index[k-1];        // Only for debugging
              k--;
            }
            max_index[j] = temppeakIndex;
            //            maxlevel_index[j] = actual_t[temppeakIndex];      // Only for debugging
            if (cur_max < MAX_MAX) {
              cur_max++;
            }
            //STOP_PROFILE
          }
                                                              // Insert done
          temppeakIndex = i;            // Latest minimum
          temppeakLevel = actual_t[i];

          downslope = true;
        }
      }
    }                   // end of peak finding
#else
    if (frequencies[i] > 1000000) {
      if (temppeakLevel < actual_t[i]) {
        temppeakIndex = i;
        temppeakLevel = actual_t[i];
      }
    }
#endif
    if (temp_min_level > actual_t[i])   // Remember minimum
      temp_min_level = actual_t[i];

  }

  if (setting.trigger != T_AUTO && setting.frequency_step > 0) {    // Trigger active
    if (actual_t[max_index[0]] < setting.trigger_level) {
      goto again;
    } else {
      if (setting.trigger == T_SINGLE)
        pause_sweep();                    // Stop scanning after completing this sweep if above trigger
    }
    scandirty = true;                // To show trigger happened
  }

  if (scandirty) {
    scandirty = false;
    redraw_request |= REDRAW_CAL_STATUS;
  }

  if (!in_selftest && setting.mode == M_LOW && setting.auto_attenuation && max_index[0] > 0) {  // Auto attenuate
    setting.atten_step = false;     // No step attenuate in low mode auto attenuate
    float old_attenuate = get_attenuation();
    float actual_max_level = actual_t[max_index[0]] - old_attenuate;
    if (actual_max_level < - 31 && setting.attenuate >= 10) {
      setting.attenuate -= 10.0;
    } else if (actual_max_level < - 26 && setting.attenuate >= 5) {
        setting.attenuate -= 5.0;
    } else if (actual_max_level > - 19 && setting.attenuate <= 20) {
      setting.attenuate += 10.0;
    }
    if (old_attenuate != get_attenuation()) {
      redraw_request |= REDRAW_CAL_STATUS;
      PE4302_Write_Byte((int)(setting.attenuate * 2));
      SI4432_Sel = 0;
      if (setting.atten_step) {
        set_switch_transmit();          // This should never happen
      } else {
        set_switch_receive();
      }
      // dirty = true;                               // Must be  above if(scandirty!!!!!)
    }
  }
  if (!in_selftest && MODE_INPUT(setting.mode) && S_IS_AUTO(setting.agc) && UNIT_IS_LINEAR(setting.unit)) { // Auto AGC in linear mode
    unsigned char v;
    static unsigned char old_v;
    float actual_max_level = actual_t[max_index[0]] - get_attenuation();
    if (actual_max_level > - 45)
      v = 0x50; // Disable AGC and enable LNA
    else
      v = 0x60; // Enable AGC and disable LNA
    if (old_v != v) {
      SI4432_Write_Byte(0x69, v);
      old_v = v;
    }

  }
  if (max_index[0] > 0)
    temppeakLevel = actual_t[max_index[0]];

  float r = value(temppeakLevel);
  float s_r = r / setting.scale;

  if (!in_selftest && MODE_INPUT(setting.mode) && setting.auto_reflevel) {  // Auto reflevel
    if (UNIT_IS_LINEAR(setting.unit)) {            // Linear scales can not have negative values
      if (setting.reflevel > REFLEVEL_MIN)  {
        if (s_r <  2)
            low_count = 5;
        else if (s_r < 4)
            low_count++;
          else
            low_count = 0;
      }
      if ((low_count > 4) || (setting.reflevel < REFLEVEL_MAX && s_r > NGRIDY) ) { // ensure minimum and maximum reflevel
        if (r < REFLEVEL_MIN)
          r = REFLEVEL_MIN;
        if (r > REFLEVEL_MAX)
          r = REFLEVEL_MAX;
        if (r != setting.reflevel) {
        //if (setting.scale * NGRIDY > r)
          set_scale(r / NGRIDY);
          set_reflevel(setting.scale*NGRIDY);
 //         dirty = false;                        // Prevent reset of SI4432
          redraw_request |= REDRAW_CAL_STATUS;
        }
      }
    } else {
      float s_min = value(temp_min_level)/setting.scale;
      float s_ref = setting.reflevel/setting.scale;
      if (s_r < s_ref  - NGRIDY || s_min > s_ref) { //Completely outside
        set_reflevel(setting.scale*(floor(s_r)+1));
        redraw_request |= REDRAW_CAL_STATUS;
//        dirty = true;                               // Must be  above if(scandirty!!!!!)
      }else if (s_r > s_ref  - 0.5 || s_min > s_ref - 8.8 ) { // maximum to high or minimum to high
        set_reflevel(setting.reflevel + setting.scale);
        redraw_request |= REDRAW_CAL_STATUS;
//        dirty = true;                               // Must be  above if(scandirty!!!!!)
      } else if (s_min < s_ref - 10.1 && s_r < s_ref -  1.5) { // minimum to low and maximum can move up
        set_reflevel(setting.reflevel - setting.scale);
        redraw_request |= REDRAW_CAL_STATUS;
//        dirty = true;                               // Must be  above if(scandirty!!!!!)
      }
 //     dirty = false;                        // Prevent reset of SI4432
    }
  }
#if 1
  if (MODE_INPUT(setting.mode)) {               // Assign maxima found to tracking markers
    int i = 0;
    int m = 0;
    while (i < cur_max) {                                 // For all maxima found
      while (m < MARKERS_MAX) {
        if (markers[m].enabled && markers[m].mtype & M_TRACKING) {   // Available marker found
          markers[m].index = max_index[i];
          markers[m].frequency = frequencies[markers[m].index];
          m++;
          break;                          // Next maximum
        }
        m++;                              // Try next marker
      }
      i++;
    }
    while (m < MARKERS_MAX) {                  // Insufficient maxima found
      if (markers[m].enabled && markers[m].mtype & M_TRACKING) {    // More available markers found
        markers[m].index = 0;                             // Enabled but no max so set to left most frequency
        markers[m].frequency = frequencies[markers[m].index];
      }
      m++;                              // Try next marker
    }
#ifdef __MEASURE__
    if (setting.measurement == M_IMD && markers[0].index > 10) {                    // IMD measurement
      markers[1].enabled = search_maximum(1, frequencies[markers[0].index]*2, 8);
      markers[2].enabled = search_maximum(2, frequencies[markers[0].index]*3, 12);
      markers[3].enabled = search_maximum(3, frequencies[markers[0].index]*4, 16);
    } else if (setting.measurement == M_OIP3  && markers[0].index > 10 && markers[1].index > 10) { // IOP measurement
      int l = markers[0].index;
      int r = markers[1].index;
      if (r < l) {
        l = markers[1].index;
        r = markers[0].index;
        markers[0].index = l;
        markers[1].index = r;
      }
      uint32_t lf = frequencies[l];
      uint32_t rf = frequencies[r];
      markers[2].enabled = search_maximum(2, lf - (rf - lf), 12);
      markers[3].enabled = search_maximum(3, rf + (rf - lf), 12);
    } else if (setting.measurement == M_PHASE_NOISE  && markers[0].index > 10) {    // Phase noise measurement
      markers[1].index =  markers[0].index + (setting.mode == M_LOW ? 290/4 : -290/4);  // Position phase noise marker at requested offset
    } else if (setting.measurement == M_STOP_BAND  && markers[0].index > 10) {      // Stop band measurement
      markers[1].index =  marker_search_left_min(markers[0].index);
      if (markers[1].index < 0) markers[1].index = 0;
      markers[2].index =  marker_search_right_min(markers[0].index);
      if (markers[2].index < 0) markers[1].index = setting._sweep_points - 1;
    } else if (setting.measurement == M_PASS_BAND  && markers[0].index > 10) {      // Pass band measurement
      int t = markers[0].index;
      float v = actual_t[t];
      while (t > 0 && actual_t[t] > v - 3.0)                                        // Find left -3dB point
        t --;
      if (t > 0)
        markers[1].index = t;
      t = markers[0].index;
      while (t < setting._sweep_points - 1 && actual_t[t] > v - 3.0)                // find right -3dB point
        t ++;
      if (t < setting._sweep_points - 1 )
        markers[2].index = t;
    }
#endif
    peakIndex = max_index[0];
    peakLevel = actual_t[peakIndex];
    peakFreq = frequencies[peakIndex];
#else
    int peak_marker = 0;
    markers[peak_marker].enabled = true;
    markers[peak_marker].index = peakIndex;
    markers[peak_marker].frequency = frequencies[markers[peak_marker].index];
#endif
    min_level = temp_min_level;
  }
  }
  if (setting.measurement == M_LINEARITY && setting.linearity_step < setting._sweep_points) {
    setting.attenuate = 29.0 - setting.linearity_step * 30.0 / 290.0;
    dirty = true;
    stored_t[setting.linearity_step] = peakLevel;
    setting.linearity_step++;
  }

  //    redraw_marker(peak_marker, FALSE);
//  STOP_PROFILE;
  if (setting.actual_sweep_time_us > ONE_SECOND_TIME) {
    ili9341_fill(OFFSETX, HEIGHT_NOSCROLL+1, WIDTH, 1, 0);
  }

  palSetPad(GPIOB, GPIOB_LED);
  return true;
}

//------------------------------- SEARCH ---------------------------------------------

int
marker_search_left_max(int from)
{
  int i;
  int found = -1;
  if (uistat.current_trace == -1)
    return -1;

  int value = actual_t[from];
  for (i = from - 1; i >= 0; i--) {
    int new_value = actual_t[i];
    if (new_value < value) {
      value = new_value;
      found = i;
    } else if (new_value > value + setting.noise )
      break;
  }

  for (; i >= 0; i--) {
    int new_value = actual_t[i];
    if (new_value > value) {
      value = new_value;
      found = i;
    } else if (new_value < value  - setting.noise )
      break;
  }
  return found;
}

int
marker_search_right_max(int from)
{
  int i;
  int found = -1;

  if (uistat.current_trace == -1)
    return -1;
  int value = actual_t[from];
  for (i = from + 1; i < sweep_points; i++) {
    int new_value = actual_t[i];
    if (new_value < value) {    // follow down
      value = new_value;
      found = i;
    } else if (new_value > value + setting.noise) // larger then lowest value + noise
      break;    //  past the minimum
  }
  for (; i < sweep_points; i++) {
    int new_value = actual_t[i];
    if (new_value > value) {    // follow up
      value = new_value;
      found = i;
    } else if (new_value < value - setting.noise)
      break;
  }
  return found;
}

#define MINMAX_DELTA 10


int
marker_search_left_min(int from)
{
  int i;
  int found = from;
  if (uistat.current_trace == -1)
    return -1;

  int value = actual_t[from];
  for (i = from - 1; i >= 0; i--) {
    int new_value = actual_t[i];
    if (new_value > value) {
      value = new_value;        // follow up
//      found = i;
    } else if (new_value < value - MINMAX_DELTA )
      break;  // past the maximum
  }

  for (; i >= 0; i--) {
    int new_value = actual_t[i];
    if (new_value < value) {
      value = new_value;        // follow down
      found = i;
    } else if (new_value > value  + MINMAX_DELTA )
      break;
  }
  return found;
}

int
marker_search_right_min(int from)
{
  int i;
  int found = from;

  if (uistat.current_trace == -1)
    return -1;
  int value = actual_t[from];
  for (i = from + 1; i < sweep_points; i++) {
    int new_value = actual_t[i];
    if (new_value > value) {    // follow up
      value = new_value;
//      found = i;
    } else if (new_value < value - MINMAX_DELTA) // less then largest value - noise
      break;    // past the maximum
  }
  for (; i < sweep_points; i++) {
    int new_value = actual_t[i];
    if (new_value < value) {    // follow down
      value = new_value;
      found = i;
    } else if (new_value > value + MINMAX_DELTA) // larger then smallest value + noise
      break;
  }
  return found;
}





// -------------------------- CAL STATUS ---------------------------------------------
const char * const averageText[] = { "OFF", "MIN", "MAX", "MAXD", " A 4", "A 16"};
const char * const dBText[] = { "1dB/", "2dB/", "5dB/", "10dB/", "20dB/"};
const int refMHz[] = { 30, 15, 10, 4, 3, 2, 1 };

float my_round(float v)
{
  float m = 1;
  int sign = 1;
  if (v < 0) {
    sign = -1;
    v = -v;
  }
  while (v < 100) {
    v = v * 10;
    m = m / 10;
  }
  while (v > 1000) {
    v = v / 10;
    m = m * 10;
  }
  v = (int)(v+0.5);
  v = v * m;
  if (sign == -1) {
    v = -v;
  }
  return v;
}

const char * const unit_string[] = { "dBm", "dBmV", "dBuV", "V", "W", "dBc", "dBc", "dBc", "Vc", "Wc" }; // unit + 5 is delta unit

static const float scale_value[]={50000, 20000, 10000, 5000, 2000, 1000, 500, 200, 100, 50, 20,10,5,2,1,0.5,0.2,0.1,0.05,0.02,0.01,0.005,0.002, 0.001,0.0005,0.0002, 0.0001};
static const char * const scale_vtext[]= {"50000", "20000", "10000", "5000", "2000", "1000", "500", "200", "100", "50", "20","10","5","2","1","0.5","0.2","0.1","0.05","0.02","0.01", "0.005","0.002","0.001", "0.0005","0.0002","0.0001"};



void draw_cal_status(void)
{
#define BLEN    7
  char buf[BLEN+1];
  buf[6]=0;
#define YSTEP   8
  int x = 0;
  int y = OFFSETY;
  unsigned int color;
  int rounding = false;
  if (!UNIT_IS_LINEAR(setting.unit))
    rounding  = true;
  const char * const unit = unit_string[setting.unit];

  ili9341_fill(0, 0, OFFSETX, LCD_HEIGHT-1, 0x0000);
  if (MODE_OUTPUT(setting.mode)) {     // No cal status during output
    return;
  }

    //  if (current_menu_is_form() && !in_selftest)
//    return;

  ili9341_set_background(DEFAULT_BG_COLOR);

  float yMax = setting.reflevel;
  // Top level
  if (rounding)
    plot_printf(buf, BLEN, "%+4d", (int)yMax);
  else
    plot_printf(buf, BLEN, "%+.3F", (yMax/setting.unit_scale));

  if (level_is_calibrated()) {
    if (setting.auto_reflevel)
      color = DEFAULT_FG_COLOR;
    else
      color = BRIGHT_COLOR_GREEN;
  }
  else
    color = BRIGHT_COLOR_RED;
  ili9341_set_foreground(color);
  ili9341_drawstring(buf, x, y);

  // Unit
#if 0
  color = DEFAULT_FG_COLOR;
  ili9341_set_foreground(color);
  if (setting.auto_reflevel){
    y += YSTEP + YSTEP/2 ;
    ili9341_drawstring("AUTO", x, y);
  }
#endif
  y += YSTEP + YSTEP/2 ;
  plot_printf(buf, BLEN, "%s%s",unit_scale_text[setting.unit_scale_index], unit);
  ili9341_drawstring(buf, x, y);

  // Scale
  color = DEFAULT_FG_COLOR;
  ili9341_set_foreground(color);
  y += YSTEP + YSTEP/2;
#if 1
  unsigned int i = 0;
  while (i < sizeof(scale_value)/sizeof(float)) {
    float t = (setting.scale/setting.unit_scale) / scale_value[i];;
    if (t > 0.9 && t < 1.1){
      plot_printf(buf, BLEN, "%s%s/",scale_vtext[i],unit_scale_text[setting.unit_scale_index]);
      break;
    }
    i++;
  }
#else
  plot_printf(buf, BLEN, "%.2F/",setting.scale);
#endif
  ili9341_drawstring(buf, x, y);

//  if (setting.mode == M_LOW) {
    // Attenuation
    if (setting.auto_attenuation)
      color = DEFAULT_FG_COLOR;
    else
      color = BRIGHT_COLOR_GREEN;
    ili9341_set_foreground(color);
    y += YSTEP + YSTEP/2 ;
    ili9341_drawstring("Atten:", x, y);
    y += YSTEP;
    plot_printf(buf, BLEN, "%.2FdB", get_attenuation());
    ili9341_drawstring(buf, x, y);
//  }

  // Average
  if (setting.average>0) {
    ili9341_set_foreground(BRIGHT_COLOR_BLUE);
    y += YSTEP + YSTEP/2 ;
    ili9341_drawstring("Calc:", x, y);

    y += YSTEP;
    plot_printf(buf, BLEN, "%s",averageText[setting.average]);
    ili9341_drawstring(buf, x, y);
  }
  // Spur
#ifdef __SPUR__
  if (setting.spur) {
    ili9341_set_foreground(BRIGHT_COLOR_GREEN);
    y += YSTEP + YSTEP/2 ;
    ili9341_drawstring("Spur:", x, y);

    y += YSTEP;
    plot_printf(buf, BLEN, "ON");
    ili9341_drawstring(buf, x, y);
  }
#endif

  if (setting.subtract_stored) {
    ili9341_set_foreground(BRIGHT_COLOR_GREEN);
    y += YSTEP + YSTEP/2 ;
    ili9341_drawstring("Norm.", x, y);
  }

  // RBW
  if (setting.rbw)
    color = BRIGHT_COLOR_GREEN;
  else
    color = DEFAULT_FG_COLOR;
  ili9341_set_foreground(color);

  y += YSTEP + YSTEP/2 ;
  ili9341_drawstring("RBW:", x, y);

  y += YSTEP;
  plot_printf(buf, BLEN, "%.1FkHz", actual_rbw);
  ili9341_drawstring(buf, x, y);

#if 0
  // VBW
  if (setting.frequency_step > 0) {
    ili9341_set_foreground(DEFAULT_FG_COLOR);
    y += YSTEP + YSTEP/2 ;
    ili9341_drawstring("VBW:", x, y);

    y += YSTEP;
    plot_printf(buf, BLEN, "%dkHz",(int)setting.vbw);
    buf[6]=0;
    ili9341_drawstring(buf, x, y);
  }
#endif
  // Sweep time
  if (dirty)
    color = BRIGHT_COLOR_RED;
  else if (setting.step_delay)
    color = BRIGHT_COLOR_GREEN;
  else
    color = DEFAULT_FG_COLOR;

  ili9341_set_foreground(color);

  y += YSTEP + YSTEP/2 ;
  ili9341_drawstring("Scan:", x, y);

  y += YSTEP;
  uint32_t t = calc_min_sweep_time_us();
  if (t < setting.sweep_time_us)
    t = setting.sweep_time_us;
  setting.actual_sweep_time_us = t;
  plot_printf(buf, BLEN, "%.3FS", (float)t/ONE_SECOND_TIME);
  ili9341_drawstring(buf, x, y);

   // Cal output
  if (setting.refer >= 0) {
    ili9341_set_foreground(BRIGHT_COLOR_RED);
    y += YSTEP + YSTEP/2 ;
    ili9341_drawstring("Ref:", x, y);

    y += YSTEP;
    plot_printf(buf, BLEN, "%dMHz",reffer_freq[setting.refer]/1000000);
    buf[6]=0;
    ili9341_drawstring(buf, x, y);
  }

  // Offset
  if (setting.offset != 0.0) {
    ili9341_set_foreground(BRIGHT_COLOR_RED);
    y += YSTEP + YSTEP/2 ;
    ili9341_drawstring("Amp:", x, y);

    y += YSTEP;
    plot_printf(buf, BLEN, "%.1fdB",setting.offset);
    ili9341_drawstring(buf, x, y);
  }

  // Repeat
  if (setting.repeat != 1) {
    ili9341_set_foreground(BRIGHT_COLOR_GREEN);
    y += YSTEP + YSTEP/2 ;
    ili9341_drawstring("Repeat:", x, y);

    y += YSTEP;
    plot_printf(buf, BLEN, "%d",setting.repeat);
    buf[6]=0;
    ili9341_drawstring(buf, x, y);
  }

  // Trigger
  if (setting.trigger != T_AUTO) {
    if (is_paused()) {
      ili9341_set_foreground(BRIGHT_COLOR_GREEN);
    } else {
      ili9341_set_foreground(BRIGHT_COLOR_RED);
    }
    y += YSTEP + YSTEP/2 ;
    ili9341_drawstring("TRIG:", x, y);

    y += YSTEP;
    if (rounding)
      plot_printf(buf, BLEN, "%4f", value(setting.trigger_level));
    else
      plot_printf(buf, BLEN, "%.4F", value(setting.trigger_level));
//    plot_printf(buf, BLEN, "%4f", value(setting.trigger_level)/setting.unit_scale);
    ili9341_drawstring(buf, x, y);
  }

  // Mode
  if (level_is_calibrated())
    color = BRIGHT_COLOR_GREEN;
  else
    color = BRIGHT_COLOR_RED;
  ili9341_set_foreground(color);
  y += YSTEP + YSTEP/2 ;
  ili9341_drawstring_7x13(MODE_LOW(setting.mode) ? "LOW" : "HIGH", x, y);

  // Compact status string
//  ili9341_set_background(DEFAULT_FG_COLOR);
  ili9341_set_foreground(DEFAULT_FG_COLOR);
  y += YSTEP + YSTEP/2 ;
  strncpy(buf,"     ",BLEN-1);
  if (setting.auto_attenuation)
    buf[0] = 'a';
  else
    buf[0] = 'A';
  if (setting.auto_IF)
    buf[1] = 'f';
  else
    buf[1] = 'F';
  if (setting.auto_reflevel)
    buf[2] = 'r';
  else
    buf[2] = 'R';
  if (S_IS_AUTO(setting.agc))
    buf[3] = 'g';
  else if (S_STATE(setting.agc))
    buf[3] = 'G';
  if (S_IS_AUTO(setting.lna))
    buf[4] = 'n';
  else if (S_STATE(setting.lna))
    buf[4] = 'N';
  if (S_IS_AUTO(setting.below_IF))
    buf[5] = 'b';
  else if (S_STATE(setting.below_IF))
    buf[5] = 'B';
  ili9341_drawstring(buf, x, y);

  // Version
  y += YSTEP + YSTEP/2 ;
  strncpy(buf,&VERSION[8], BLEN-1);
  ili9341_drawstring(buf, x, y);

//  ili9341_set_background(DEFAULT_BG_COLOR);

  // Bottom level
  y = area_height - 7 + OFFSETY;
  if (rounding)
    plot_printf(buf, BLEN, "%4d", (int)(yMax - setting.scale * NGRIDY));
  else
    plot_printf(buf, BLEN, "%.3F", ((yMax - setting.scale * NGRIDY)/setting.unit_scale));
//  buf[5]=0;
  if (level_is_calibrated())
    if (setting.auto_reflevel)
      color = DEFAULT_FG_COLOR;
    else
      color = BRIGHT_COLOR_GREEN;
  else
    color = BRIGHT_COLOR_RED;
  ili9341_set_foreground(color);
  ili9341_drawstring(buf, x, y);

}

// -------------------- Self testing -------------------------------------------------

enum {
  TC_SIGNAL, TC_BELOW, TC_ABOVE, TC_FLAT, TC_MEASURE, TC_SET, TC_END,
};

enum {
  TP_SILENT, TPH_SILENT, TP_10MHZ, TP_10MHZEXTRA, TP_10MHZ_SWITCH, TP_30MHZ, TPH_30MHZ
};

#define TEST_COUNT  17

static const struct {
  int kind;
  int setup;
  float center;      // In MHz
  float span;        // In MHz
  float pass;
  int width;
  float stop;
} test_case [TEST_COUNT] =
{// Condition   Preparation     Center  Span    Pass Width  Stop
 {TC_BELOW,     TP_SILENT,      0.005,  0.01,  0,0,     0},         // 1 Zero Hz leakage
 {TC_BELOW,     TP_SILENT,      0.01,   0.01,  -30,   0,     0},         // 2 Phase noise of zero Hz
 {TC_SIGNAL,    TP_10MHZ,       20,     7,      -37, 30,    -90 },      // 3
 {TC_SIGNAL,    TP_10MHZ,       30,     7,      -32, 30,    -90 },      // 4
 {TC_BELOW,     TP_SILENT,      200,    100,    -75, 0,     0},         // 5  Wide band noise floor low mode
 {TC_BELOW,     TPH_SILENT,     600,    720,    -75, 0,     0},         // 6 Wide band noise floor high mode
 {TC_SIGNAL,    TP_10MHZEXTRA,  10,     8,      -20, 80,    -80 },      // 7 BPF loss and stop band
 {TC_FLAT,      TP_10MHZEXTRA,  10,     4,      -18, 20,    -60},       // 8 BPF pass band flatness
 {TC_BELOW,     TP_30MHZ,       430,    60,     -80, 0,     -80},       // 9 LPF cutoff
 {TC_SIGNAL,    TP_10MHZ_SWITCH,20,     7,      -38, 30,    -65 },      // 10 Switch isolation using high attenuation
 {TC_END,       0,              0,      0,      0,   0,     0},
 {TC_MEASURE,   TP_30MHZ,       30,     7,      -22.5, 30,  -70 },      // 12 Measure power level and noise
 {TC_MEASURE,   TP_30MHZ,       270,    4,      -50, 30,    -75 },       // 13 Measure powerlevel and noise
 {TC_MEASURE,   TPH_30MHZ,      270,    4,      -40, 30,    -65 },       // 14 Calibrate power high mode
 {TC_END,       0,              0,      0,      0,   0,     0},
 {TC_MEASURE,   TP_30MHZ,       30,     1,      -20, 30,    -70 },      // 16 Measure RBW step time
 {TC_END,       0,              0,      0,      0,   0,     0},
};

enum {
  TS_WAITING, TS_PASS, TS_FAIL, TS_CRITICAL
};
static const  char *(test_text [4]) =
{
 "Waiting", "Pass", "Fail", "Critical"
};
static const  char *(test_fail_cause [TEST_COUNT]);

static int test_status[TEST_COUNT];
static int show_test_info = FALSE;
static volatile int test_wait = false;
static float test_value;

static void test_acquire(int i)
{
  (void)i;
  pause_sweep();
#if 0
  if (test_case[i].center < 300)
    setting.mode = M_LOW;
  else
    setting.mode = M_HIGH;
#endif
//  SetAverage(4);
  sweep(false);
//  sweep(false);
//  sweep(false);
//  sweep(false);
  plot_into_index(measured);
  redraw_request |= REDRAW_CELLS | REDRAW_FREQUENCY;
}

void cell_drawstring(char *str, int x, int y);

static char self_test_status_buf[35];
void cell_draw_test_info(int x0, int y0)
{
#define INFO_SPACING    13
//  char self_test_status_buf[35];
  if (!show_test_info)
    return;
  int i = -2;
  do {
    i++;
    int xpos = 25 - x0;
    int ypos = 50+i*INFO_SPACING - y0;
    unsigned int color = RGBHEX(0xFFFFFF);
    if (i == -1) {
        plot_printf(self_test_status_buf, sizeof self_test_status_buf, "Self test status:");
    } else if (test_case[i].kind == TC_END) {
        if (test_wait)
          plot_printf(self_test_status_buf, sizeof self_test_status_buf, "Touch screen to continue");
        else
          self_test_status_buf[0] = 0;
      } else {
      plot_printf(self_test_status_buf, sizeof self_test_status_buf, "Test %d: %s%s", i+1, test_fail_cause[i], test_text[test_status[i]] );
      if (test_status[i] == TS_PASS)
        color = RGBHEX(0x00FF00);
      else if (test_status[i] == TS_CRITICAL)
        color = RGBHEX(0xFFFF00);
      else if (test_status[i] == TS_FAIL)
        color = RGBHEX(0xFF7F7F);
      else
        color = RGBHEX(0x0000FF);
    }
    ili9341_set_foreground(color);
    cell_drawstring(self_test_status_buf, xpos, ypos);
  } while (test_case[i].kind != TC_END);
}

#define fabs(X) ((X)<0?-(X):(X))

int validate_signal_within(int i, float margin)
{
  test_fail_cause[i] = "Signal level ";
  if (fabs(peakLevel-test_case[i].pass) > 2*margin) {
    return TS_FAIL;
  }
  if (fabs(peakLevel-test_case[i].pass) > margin) {
    return TS_CRITICAL;
  }
  test_fail_cause[i] = "Frequency ";
  if (peakFreq < test_case[i].center * 1000000 - 200000 || test_case[i].center * 1000000 + 200000 < peakFreq )
    return TS_FAIL;
  test_fail_cause[i] = "";
  return TS_PASS;
}

int validate_peak_below(int i, float margin) {
  return(test_case[i].pass - peakLevel > margin);
}

int validate_below(int tc, int from, int to) {
  int status = TS_PASS;
  float threshold=stored_t[from];
  float sum = 0;
  int sum_count = 0;
  for (int j = from; j < to; j++) {
    sum += actual_t[j];
    sum_count++;
    if (actual_t[j] > threshold) {
      status = TS_FAIL;
      break;
    }
  }
  sum = sum / sum_count;
  if (sum > threshold - 5)
    status = TS_CRITICAL;
  if (status != TS_PASS)
    test_fail_cause[tc] = "Above ";
  return(status);
}

int validate_flatness(int i) {
  volatile int j;
  test_fail_cause[i] = "Passband ";
  for (j = peakIndex; j < setting._sweep_points; j++) {
    if (actual_t[j] < peakLevel - 6)    // Search right -3dB
      break;
  }
  //shell_printf("\n\rRight width %d\n\r", j - peakIndex );
  if (j - peakIndex < test_case[i].width)
    return(TS_FAIL);
  for (j = peakIndex; j > 0; j--) {
    if (actual_t[j] < peakLevel - 6)    // Search left -3dB
      break;
  }
  //shell_printf("Left width %d\n\r", j - peakIndex );
  if (peakIndex - j < test_case[i].width)
    return(TS_FAIL);
  test_fail_cause[i] = "";
  return(TS_PASS);
}

int validate_above(int tc) {
  int status = TS_PASS;
  for (int j = 0; j < setting._sweep_points; j++) {
    if (actual_t[j] < stored_t[j] + 5)
      status = TS_CRITICAL;
    else if (actual_t[j] < stored_t[j]) {
      status = TS_FAIL;
      break;
    }
  }
  if (status != TS_PASS)
    test_fail_cause[tc] = "Below ";
  return(status);
}


int test_validate(int i)
{
//  draw_all(TRUE);
  int current_test_status = TS_PASS;
  switch (test_case[i].kind) {
  case TC_SET:
    if (test_case[i].pass == 0) {
      if (test_value != 0)
        set_actual_power(test_value);
    } else
      set_actual_power(test_case[i].pass);
    goto common;
  case TC_MEASURE:
  case TC_SIGNAL:           // Validate signal
  common: current_test_status = validate_signal_within(i, 5.0);
    if (current_test_status == TS_PASS) {            // Validate noise floor
      current_test_status = validate_below(i, 0, setting._sweep_points/2 - test_case[i].width);
      if (current_test_status == TS_PASS) {
        current_test_status = validate_below(i, setting._sweep_points/2 + test_case[i].width, setting._sweep_points);
      }
      if (current_test_status != TS_PASS)
        test_fail_cause[i] = "Stopband ";
    }
    if (current_test_status == TS_PASS && test_case[i].kind == TC_MEASURE)
      test_value = peakLevel;
    else
      test_value = 0;           //   Not valid
  break;
  case TC_ABOVE:   // Validate signal above curve
    current_test_status = validate_above(i);
    break;
  case TC_BELOW:   // Validate signal below curve
    current_test_status = validate_below(i, 0, setting._sweep_points);
    break;
  case TC_FLAT:   // Validate passband flatness
    current_test_status = validate_flatness(i);
    break;

  }

  // Report status

  if (current_test_status != TS_PASS || test_case[i+1].kind == TC_END)
    test_wait = true;
  test_status[i] = current_test_status;     // Must be set before draw_all() !!!!!!!!
  //  draw_frequencies();
//  draw_cal_status();
  draw_all(TRUE);
  return current_test_status;
}

void test_prepare(int i)
{
  setting.tracking = false; //Default test setup
  setting.atten_step = false;
  setting.frequency_IF = 433800000;                // Default frequency
  setting.auto_IF = true;
  setting.auto_attenuation = false;
  switch(test_case[i].setup) {                // Prepare test conditions
  case TPH_SILENT:                             // No input signal
    set_mode(M_HIGH);
    goto common_silent;
  case TP_SILENT:                             // No input signal
    set_mode(M_LOW);
common_silent:
    set_refer_output(-1);
    for (int j = 0; j < setting._sweep_points; j++)
      stored_t[j] = test_case[i].pass;
    break;
  case TP_10MHZ_SWITCH:
    set_mode(M_LOW);
    set_refer_output(2);
    goto common;
  case TP_10MHZEXTRA:                         // Swept receiver
    set_mode(M_LOW);
    setting.tracking = true; //Sweep BPF
    setting.auto_IF = false;
    setting.frequency_IF = 434000000;                // Center on SAW filters
    set_refer_output(2);
    goto common;
  case TP_10MHZ:                              // 10MHz input
    set_mode(M_LOW);
    set_refer_output(2);
    set_step_delay(1);                      // Precise scanning speed
#ifdef __SPUR__
    setting.spur = 1;
#endif
 common:

    for (int j = 0; j < setting._sweep_points/2 - test_case[i].width; j++)
      stored_t[j] = test_case[i].stop;
    for (int j = setting._sweep_points/2 + test_case[i].width; j < setting._sweep_points; j++)
      stored_t[j] = test_case[i].stop;
    for (int j = setting._sweep_points/2 - test_case[i].width; j < setting._sweep_points/2 + test_case[i].width; j++)
      stored_t[j] = test_case[i].pass;
    break;
  case TP_30MHZ:
    set_mode(M_LOW);
    set_refer_output(0);
 //   set_step_delay(1);                      // Do not set !!!!!
#ifdef __SPUR__
    setting.spur = 1;
#endif
    goto common;
  case TPH_30MHZ:
    set_mode(M_HIGH);
    set_refer_output(0);
    goto common;
  }
  switch(test_case[i].setup) {                // Prepare test conditions
  case TP_10MHZ_SWITCH:
    set_attenuation(32);                        // This forces the switch to transmit so isolation can be tested
    break;
  default:
    set_attenuation(0.0);
  }
  trace[TRACE_STORED].enabled = true;
  set_reflevel(test_case[i].pass+10);
  set_sweep_frequency(ST_CENTER, (uint32_t)(test_case[i].center * 1000000));
  set_sweep_frequency(ST_SPAN, (uint32_t)(test_case[i].span * 1000000));
  draw_cal_status();
}

extern void menu_autosettings_cb(int item);
extern float SI4432_force_RBW(int i);

int last_spur = 0;
int add_spur(int f)
{
  for (int i = 0; i < last_spur; i++) {
    if (temp_t[i] == f) {
      stored_t[i] += 1;
      return stored_t[i];
    }
  }
  if (last_spur < 290) {
    temp_t[last_spur] = f;
    stored_t[last_spur++] = 1;
  }
  return 1;
}

//static bool test_wait = false;
static int test_step = 0;

void self_test(int test)
{
  if (test == 0) {
    if (test_wait ) {
      if (test_case[test_step].kind == TC_END || setting.test_argument != 0)
        goto resume2;
      else
        goto resume;
    }
    reset_settings(M_LOW);                      // Make sure we are in a defined state
    in_selftest = true;
    menu_autosettings_cb(0);
    for (int i=0; i < TEST_COUNT; i++) {          // All test cases waiting
      if (test_case[i].kind == TC_END)
        break;
      test_status[i] = TS_WAITING;
      test_fail_cause[i] = "";
    }
    show_test_info = TRUE;
    test_step=0;
    if (setting.test_argument > 0)
      test_step=setting.test_argument-1;
    do {
      test_prepare(test_step);
      test_acquire(test_step);                        // Acquire test
      test_status[test_step] = test_validate(test_step);                       // Validate test
      if (test_status[test_step] != TS_PASS) {
        resume:
        test_wait = true;
        if (!check_touched())
          return;
//        wait_user();
      }
      test_step++;
    } while (test_case[test_step].kind != TC_END && setting.test_argument == 0 );
    ili9341_set_foreground(BRIGHT_COLOR_GREEN);
    ili9341_drawstring_7x13("Self test complete", 50, 200);
    ili9341_drawstring_7x13("Touch screen to continue", 50, 215);
   resume2:
    test_wait = true;
    if (!check_touched())
      return;

    ili9341_clear_screen();
    reset_settings(M_LOW);
    set_refer_output(-1);
  } else if (test ==1) {
    in_selftest = true;               // Spur search
    reset_settings(M_LOW);
    test_prepare(4);
    int f = 400000;           // Start search at 400kHz
    //  int i = 0;                     // Index in spur table (temp_t)
    float p2, p1, p;

#define FREQ_STEP   3000

    set_RBW(FREQ_STEP/1000);
    last_spur = 0;
    for (int j = 0; j < 10; j++) {

      p2 = perform(false, 0, f, false);
      vbwSteps = 1;
      f += FREQ_STEP;
      p1 = perform(false, 1, f, false);
      f += FREQ_STEP;
      shell_printf("\n\rStarting with %4.2f, %4.2f and IF at %d\n\r", p2, p1, setting.frequency_IF);

      f = 400000;
      while (f < 100000000) {
        p = perform(false, 1, f, false);
#define SPUR_DELTA  6
        if ( p2 < p1 - SPUR_DELTA  && p < p1 - SPUR_DELTA) {
          //        temp_t[i++] = f - FREQ_STEP;
          shell_printf("Spur of %4.2f at %d with count %d\n\r", p1,(f - FREQ_STEP)/1000, add_spur(f - FREQ_STEP));
        }
        //    else
        //      shell_printf("%f at %d\n\r", p1,f - FREQ_STEP);
        p2 = p1;
        p1 = p;
        f += FREQ_STEP;
      }
    }
    shell_printf("\n\rTable for IF at %d\n\r", setting.frequency_IF);
    for (int j = 0; j < last_spur; j++) {
      if ((int)stored_t[j] > 1)
        shell_printf("%d, %d\n\r", ((int)temp_t[j])/1000, (int)stored_t[j]);
    }
    reset_settings(M_LOW);
  } else if (test == 2) {                                   // Attenuator test
    in_selftest = true;
    reset_settings(M_LOW);
    int i = 15;       // calibrate attenuator at 30 MHz;
    float reference_peak_level = 0;
    test_prepare(i);
    for (int j= 0; j < 50; j++ ) {
      test_prepare(i);
      set_RBW(30);

      set_attenuation((float)j);
      float summed_peak_level = 0;
      for (int k=0; k<10; k++) {
        test_acquire(i);                        // Acquire test
        test_validate(i);                       // Validate test
        summed_peak_level += peakLevel;
      }
      peakLevel = summed_peak_level / 10;
      if (j == 0)
        reference_peak_level = peakLevel;
      shell_printf("Attenuation %ddB, measured level %.2fdBm, delta %.2fdB\n\r",j, peakLevel, peakLevel - reference_peak_level);
    }
    reset_settings(M_LOW);
  } else if (test == 3) {                       // RBW step time search
    in_selftest = true;
//    reset_settings(M_LOW);
    setting.auto_IF = false;
    setting.frequency_IF=433900000;
    setting.step_delay = 2;
    ui_mode_normal();
//    int i = 13;       // calibrate low mode power on 30 MHz;
    int i = 15;       // calibrate low mode power on 30 MHz;
    test_prepare(i);
    setting.step_delay = 8000;
    for (int j= 0; j < 57; j++ ) {
      test_prepare(i);
      setting.spur = 0;
      setting.step_delay = setting.step_delay * 5 / 4;
      setting.rbw = SI4432_force_RBW(j);
      shell_printf("RBW = %d, ",setting.rbw);
      set_sweep_frequency(ST_SPAN, (uint32_t)(setting.rbw * 10000));
      setting.repeat = 10;
      test_acquire(i);                        // Acquire test
      test_validate(i);                       // Validate test
      float saved_peakLevel = peakLevel;
 //     if (peakLevel < -35) {
 //       shell_printf("Peak level too low, abort\n\r");
 //       return;
 //     }

      shell_printf("Start level = %f, ",peakLevel);
      while (setting.step_delay > 10 && peakLevel > saved_peakLevel - 1) {
        test_prepare(i);
        setting.spur = 0;
        setting.step_delay = setting.step_delay * 4 / 5;
        //      shell_printf("\n\rRBW = %f",SI4432_force_RBW(j));
        set_sweep_frequency(ST_SPAN, (uint32_t)(setting.rbw * 10000));
        setting.repeat = 10;
        test_acquire(i);                        // Acquire test
        test_validate(i);                       // Validate test
        //      shell_printf(" Step %f, %d",peakLevel, setting.step_delay);
      }
      setting.step_delay = setting.step_delay * 5 / 4;
      shell_printf("End level = %f, step time = %d\n\r",peakLevel, setting.step_delay);
    }
    reset_settings(M_LOW);
  } else if (test == 5) {
//    reset_settings(M_LOW);                      // Make sure we are in a defined state
    in_selftest = true;
    switch (setting.test_argument) {
    case 0:
      touch_draw_test();
      area_width  = AREA_WIDTH_NORMAL;
      area_height = AREA_HEIGHT_NORMAL;
      break;
    case 1:
      reset_settings(M_LOW);
      set_sweep_frequency(ST_START, 0);
      set_sweep_frequency(ST_STOP, 50000000);
      break;
    case 2:
      reset_settings(M_LOW);
      set_sweep_frequency(ST_START, 300000000);
      set_sweep_frequency(ST_STOP, 350000000);
      break;
    case 3:
      reset_settings(M_HIGH);
      set_sweep_frequency(ST_START, 300000000);
      set_sweep_frequency(ST_STOP, 350000000);
      break;
    case 4:
      reset_settings(M_GENLOW);
      set_sweep_frequency(ST_CENTER, 20000000);
      set_sweep_frequency(ST_SPAN, 0);
      setting.mute = false;
      break;
    case 5:
      reset_settings(M_GENHIGH);
      set_sweep_frequency(ST_CENTER, 320000000);
      set_sweep_frequency(ST_SPAN, 0);
      break;
    }
    in_selftest = false;
  }
  show_test_info = FALSE;
  in_selftest = false;
  test_wait = false;
  sweep_mode = SWEEP_ENABLE;
}

void reset_calibration(void)
{
  config.high_level_offset = 100;
  config.low_level_offset = 100;
}

#define CALIBRATE_RBWS  1
const int power_rbw [5] = { 100, 300, 30, 10, 3 };

void calibrate(void)
{
#ifdef __CALIBRATE__
  int local_test_status;
  in_selftest = true;
  reset_calibration();
  reset_settings(M_LOW);
  int i = 11;       // calibrate low mode power on 30 MHz;
  for (int j= 0; j < CALIBRATE_RBWS; j++ ) {
    set_RBW(power_rbw[j]);
    test_prepare(i);
    test_acquire(i);                        // Acquire test
    local_test_status = test_validate(i);                       // Validate test
//    chThdSleepMilliseconds(1000);
    if (local_test_status != TS_PASS) {
      ili9341_set_foreground(BRIGHT_COLOR_RED);
      ili9341_drawstring_7x13("Calibration failed", 30, 120);
      goto quit;
    } else {
      set_actual_power(-22.5);           // Should be -22.5dBm
      chThdSleepMilliseconds(1000);
    }
  }
#if 0               // No high input calibration as CAL OUTPUT is unreliable

  i = 12;           // Measure 270MHz in low mode
  set_RBW(100);
  test_prepare(i);
  test_acquire(i);                        // Acquire test
  float last_peak_level = peakLevel;
  local_test_status = test_validate(i);                       // Validate test
  chThdSleepMilliseconds(1000);

  config.high_level_offset = 0;           /// Preliminary setting

  i = 13;           // Calibrate 270MHz in high mode
  for (int j = 0; j < CALIBRATE_RBWS; j++) {
    set_RBW(power_rbw[j]);
    test_prepare(i);
    test_acquire(i);                        // Acquire test
    local_test_status = test_validate(i);                       // Validate test
//    if (local_test_status != TS_PASS) {                       // Do not validate due to variations in SI4432
//      ili9341_set_foreground(BRIGHT_COLOR_RED);
//      ili9341_drawstring_7x13("Calibration failed", 30, 120);
//      goto quit;
//    } else
      set_actual_power(last_peak_level);
      chThdSleepMilliseconds(1000);
  }

#endif

  config_save();
  ili9341_set_foreground(BRIGHT_COLOR_GREEN);
  ili9341_drawstring_7x13("Calibration complete", 30, 120);
quit:
  ili9341_drawstring_7x13("Touch screen to continue", 30, 140);
  wait_user();
  ili9341_clear_screen();

  in_selftest = false;
  sweep_mode = SWEEP_ENABLE;
  set_refer_output(0);
  reset_settings(M_LOW);
#endif
}


