/*
 * FixedIntervalTime2.cpp
 *
 *  Created on: Feb 24, 2010
 *      Author: gbays
 */

#include "FixedIntervalTime.h"

FixedTimer::FixedTimer(flo dt, flo ratio) {
  this->dt=dt; half_dt=dt/2;
  internal_dt = dt*ratio; internal_half_dt = dt*ratio/2;
  this->ratio = ratio;
}
void FixedTimer::next_transmit(SECONDS* d_true, SECONDS* d_internal) {
  *d_true = half_dt; *d_internal = internal_half_dt;
}
void FixedTimer::next_compute(SECONDS* d_true, SECONDS* d_internal) {
  *d_true = dt; *d_internal = internal_dt;
}

void FixedTimer::set_internal_dt(SECONDS dt) {
  internal_dt = dt;
  internal_half_dt = dt/2;
  this->dt = internal_dt/ratio;
  half_dt = internal_dt/(ratio*2);
}

FixedIntervalTime::FixedIntervalTime(Args* args, SpatialComputer* p) {
  sync = args->extract_switch("-sync");
  dt = (args->extract_switch("-desired-period"))?args->pop_number():1;
  var = (args->extract_switch("-desired-period-variance"))
    ? args->pop_number() : 0;
  ratio = (args->extract_switch("-desired-ratio"))?args->pop_number():1;
  rvar = (args->extract_switch("-desired-ratio-variance"))
    ? args->pop_number() : 0;

  p->hardware.patch(this,SET_DT_FN);
}

DeviceTimer* FixedIntervalTime::next_timer(SECONDS* start_lag) {
  if(sync) { *start_lag=0; return new FixedTimer(dt,ratio); }
  *start_lag = urnd(0,dt);
  flo p = urnd(dt-var,dt+var);
  flo ip = urnd(ratio-rvar,ratio+rvar);
  return new FixedTimer(MAX(0,p),MAX(0,ip));
}

NUM_VAL FixedIntervalTime::set_dt (NUM_VAL dt) {
  ((FixedTimer*)device->timer)->set_internal_dt(dt);
  return dt;
}

FixedIntervalTime::~FixedIntervalTime() {
  // TODO Auto-generated destructor stub
}
